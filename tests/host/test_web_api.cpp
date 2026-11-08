#include <memory>
#include <string>
#include <vector>

#include "cJSON.h"
#include "check.h"
#include "fake_storage.h"
#include "summit/system_settings.h"
#include "summit/web/api.h"

using namespace summit;
using namespace summit::web;

struct FakeUpdate : UpdateControl {
    std::vector<uint8_t> got;
    int64_t expected = 0;
    bool refuse = false, fail_complete = false;
    std::string aborted;
    bool upload_begin(int64_t size, std::string *e) override
    {
        if (refuse) {
            *e = "Connect USB power or charge to 40% first";
            return false;
        }
        expected = size;
        return true;
    }
    bool upload_data(const uint8_t *d, size_t n) override
    {
        got.insert(got.end(), d, d + n);
        return true;
    }
    bool upload_complete(std::string *e) override
    {
        if (fail_complete) *e = "SHA-256 mismatch";
        return !fail_complete && static_cast<int64_t>(got.size()) == expected;
    }
    void upload_abort(const char *r) override { aborted = r; }
};

struct FakeWifi : WifiStore {
    std::string s, pw;
    std::string ssid() override { return s; }
    std::string password() override { return pw; }
    bool save(const std::string &a, const std::string &b, std::string *) override
    {
        s = a;
        pw = b;
        return true;
    }
};

struct FakeFacts : DeviceFacts {
    std::vector<std::pair<std::string, std::string>> facts() override { return {{"uptime_s", "42"}}; }
};

static SettingsSchema schema_with_secret()
{
    SettingsSchema s = system_settings_schema();
    s.defs.push_back({"printer/access_code", std::string(""), {}, "Printer access code", "From the printer's screen", true});
    return s;
}

struct Rig {
    EventBus bus{64};
    StateStore state{bus};
    FakeStorage storage;
    Settings settings{schema_with_secret(), storage, &bus};
    FakeUpdate update;
    FakeWifi wifi;
    FakeFacts facts;
    WebApi api{bus, state, settings, update, wifi, facts};
    std::vector<std::string> events;
    Rig()
    {
        settings.load();
        state.set("system/firmware_version", std::string("0.1.0"));
        state.set("system/hardware_id", std::string("waveshare-p4-wifi6-lcd5-rev3"));
        for (const char *t : {"update/command", "net/connect"})
            bus.subscribe(t, [this](const std::string &topic, const Value &v) {
                events.push_back(topic + "=" + std::get<std::string>(v));
            });
    }
    Response get(const std::string &p) { return api.handle({"GET", p, "", ""}); }
    Response post(const std::string &p, const std::string &body, const std::string &ct = "application/json")
    {
        Response r = api.handle({"POST", p, ct, body});
        bus.dispatch();
        return r;
    }
};

struct Json {
    cJSON *j;
    explicit Json(const std::string &s) : j(cJSON_Parse(s.c_str())) {}
    ~Json() { cJSON_Delete(j); }
    const cJSON *at(const char *path) const
    {
        const cJSON *cur = j;
        std::string p = path;
        size_t start = 0;
        while (cur && start <= p.size()) {
            size_t dot = p.find('.', start);
            std::string k = p.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
            cur = cJSON_GetObjectItemCaseSensitive(cur, k.c_str());
            if (dot == std::string::npos) break;
            start = dot + 1;
        }
        return cur;
    }
    std::string s(const char *path) const
    {
        const cJSON *v = at(path);
        return cJSON_IsString(v) ? v->valuestring : "<none>";
    }
};

TEST(routing_and_request_guards)
{
    Rig r;
    CHECK(r.get("/api/nope").status == 404);
    CHECK(r.api.handle({"DELETE", "/api/status", "", ""}).status == 405);
    CHECK(r.post("/api/wifi", "ssid=x", "application/x-www-form-urlencoded").status == 415);  // no cross-site form posts
    CHECK(r.post("/api/wifi", std::string(40000, ' ')).status == 413);
    Response s = r.get("/api/status");
    Json j(s.body);
    CHECK(s.status == 200 && j.s("firmware") == "0.1.0" && j.s("hardware_id") == "waveshare-p4-wifi6-lcd5-rev3");
}

TEST(wifi_password_is_write_only)
{
    Rig r;
    CHECK(r.post("/api/wifi", R"({"ssid":"home","password":"short"})").status == 400);
    CHECK(r.post("/api/wifi", R"({"ssid":"","password":"longenough"})").status == 400);
    CHECK(r.post("/api/wifi", R"({"ssid":"home","password":"correct horse"})").status == 202);
    CHECK(r.wifi.pw == "correct horse");
    CHECK((r.events == std::vector<std::string>{"net/connect=home"}));
    Response g = r.get("/api/wifi");
    CHECK(Json(g.body).s("configured_ssid") == "home" && g.body.find("correct horse") == std::string::npos);
    r.state.set("net/scan", std::string("[{\"ssid\":\"home\",\"rssi\":-50}"));  // malformed producer
    CHECK(cJSON_IsArray(Json(r.get("/api/wifi").body).at("networks")));
    CHECK(r.post("/api/wifi", R"({"ssid":"cafe"})").status == 202);  // open network
}

TEST(firmware_commands_go_through_the_update_engine)
{
    Rig r;
    r.state.set("update/catalog", std::string(R"([{"version":"0.2.0","installable":true}])"));
    Json f(r.get("/api/firmware").body);
    CHECK(cJSON_GetArraySize(f.at("catalog")) == 1 && f.s("channel") == "stable");
    CHECK(r.post("/api/firmware/check", "{}").status == 202);
    CHECK(r.post("/api/firmware/install", R"({"version":"0.2.0"})").status == 202);
    CHECK(r.post("/api/firmware/install", R"({"version":"0.2.0;rm"})").status == 400);
    CHECK(r.post("/api/firmware/reboot", "{}").status == 409);  // nothing waiting
    r.state.set("update/stage", std::string("ready_to_reboot"));
    CHECK(r.post("/api/firmware/reboot", "{}").status == 202);
    CHECK(r.post("/api/firmware/previous", R"({"confirm":true})").status == 409);  // none available
    r.state.set("update/previous_version", std::string("0.0.9"));
    CHECK(r.post("/api/firmware/previous", "{}").status == 400);  // needs confirmation
    CHECK(r.post("/api/firmware/previous", R"({"confirm":true})").status == 202);
    CHECK((r.events == std::vector<std::string>{"update/command=check", "update/command=install:0.2.0",
                                                "update/command=reboot", "update/command=previous"}));
}

TEST(firmware_upload_streams_into_engine)
{
    Rig r;
    Response err;
    CHECK(WebApi::upload_refused().status == 403);
    CHECK(!r.api.upload_begin(0, &err) && err.status == 411);
    r.update.refuse = true;
    CHECK(!r.api.upload_begin(100, &err) && err.status == 409 && Json(err.body).s("error").find("40%") != std::string::npos);
    r.update.refuse = false;
    std::vector<uint8_t> img(300, 7);
    r.state.set("update/target_version", std::string("0.3.0"));
    CHECK(r.api.upload_begin(300, &err));
    CHECK(r.api.upload_chunk(img.data(), 100) && r.api.upload_chunk(img.data() + 100, 200));
    Response done = r.api.upload_finish();
    CHECK(done.status == 200 && Json(done.body).s("version") == "0.3.0" && r.update.got == img);
    r.update.fail_complete = true;
    r.api.upload_begin(1, &err);
    CHECK(r.api.upload_finish().status == 422);
}

TEST(settings_schema_get_and_typed_post)
{
    Rig r;
    Json g(r.get("/api/settings").body);
    CHECK(cJSON_GetArraySize(g.at("settings")) >= 6 && g.s("notice") == "none");
    Response p = r.post("/api/settings", R"({"display/brightness":42,"ui/rotation_lock":"portrait"})");
    CHECK(p.status == 200);
    CHECK(std::get<int64_t>(r.settings.get("display/brightness")) == 42);
    p = r.post("/api/settings", R"({"display/brightness":"bright","ui/rotation_lock":"sideways","nope":1})");
    Json j(p.body);
    CHECK(p.status == 400 && j.s("results.display/brightness") == "wrong type");
    const cJSON *res = j.at("results");
    CHECK(std::string(cJSON_GetObjectItemCaseSensitive(res, "display/brightness")->valuestring) == "wrong type");
    CHECK(std::string(cJSON_GetObjectItemCaseSensitive(res, "ui/rotation_lock")->valuestring) == "not an allowed value");
    CHECK(std::string(cJSON_GetObjectItemCaseSensitive(res, "nope")->valuestring) == "unknown setting");
}

TEST(settings_export_import_round_trip)
{
    Rig a;
    a.settings.set("display/brightness", int64_t{25});
    a.settings.set("update/channel", std::string("beta"));
    Response e = a.get("/api/settings/export");
    CHECK(e.status == 200 && e.filename == "summit-settings-0.1.0.json");
    CHECK(e.body.find("password") == std::string::npos);  // Wi-Fi is not in settings
    Json ej(e.body);
    CHECK(ej.s("format") == "summit-settings" && cJSON_GetObjectItemCaseSensitive(ej.j, "schema_version")->valueint == 1);

    Rig b;
    Response i = b.post("/api/settings/import", e.body);
    CHECK(i.status == 200);
    CHECK(std::get<int64_t>(b.settings.get("display/brightness")) == 25);
    CHECK(std::get<std::string>(b.settings.get("update/channel")) == "beta");
    CHECK(b.get("/api/settings/backup").status == 200);  // pre-import settings kept

    std::string newer = e.body;
    newer.replace(newer.find("\"schema_version\":1"), 18, "\"schema_version\":9");
    CHECK(b.post("/api/settings/import", newer).status == 409);
    CHECK(b.post("/api/settings/import", R"({"format":"other"})").status == 400);
    CHECK(b.post("/api/settings/import", R"({"format":"summit-settings","schema_version":1,"values":{"Bad Key":1}})").status == 400);
}

TEST(settings_reset_and_unreadable_export)
{
    Rig r;
    CHECK(r.get("/api/settings/backup").status == 404);
    CHECK(r.post("/api/settings/reset", "{}").status == 400);  // must confirm
    r.settings.set("display/brightness", int64_t{10});
    r.state.set("system/settings_notice", std::string("x"));
    CHECK(r.post("/api/settings/reset", R"({"confirm":true})").status == 200);
    CHECK(std::get<int64_t>(r.settings.get("display/brightness")) == 80);
    CHECK(std::get<std::string>(r.state.get("system/settings_notice")).empty());
    CHECK(r.get("/api/settings/backup").status == 200);

    EventBus bus;
    StateStore state(bus);
    FakeStorage bad;
    bad.blob = "corrupt";
    bad.has_blob = true;
    Settings s(system_settings_schema(), bad, &bus);
    s.load();
    FakeUpdate u;
    FakeWifi w;
    FakeFacts f;
    WebApi api(bus, state, s, u, w, f);
    Response e = api.handle({"GET", "/api/settings/export", "", ""});
    CHECK(e.status == 200 && e.content_type == "application/octet-stream" && e.body == "corrupt");
    CHECK(Json(api.handle({"GET", "/api/settings", "", ""}).body).s("notice") == "unreadable");
}

TEST(diagnostics_lists_addon_warnings)
{
    Rig r;
    r.state.set("diag/warnings", std::string("LIS2DW12 accelerometer not found\nmicroSD not found"));
    Json d(r.get("/api/diagnostics").body);
    CHECK(cJSON_GetArraySize(d.at("warnings")) == 2);
    CHECK(d.s("device.uptime_s") == "42");
}

TEST(import_is_validated_first_and_refused_cleanly)
{
    Rig r;
    r.settings.set("display/brightness", int64_t{30});
    std::string blob = r.storage.blob;
    const char *bad = R"({"format":"summit-settings","schema_version":1,"values":{"display/brightness":"bright","ui/rotation_lock":"sideways"}})";
    Response v = r.post("/api/settings/import/validate", bad);
    CHECK(v.status == 400 && Json(v.body).s("error") ==
                                 "settings file has invalid values: display/brightness, ui/rotation_lock");
    Response i = r.post("/api/settings/import", bad);
    CHECK(i.status == 400 && r.storage.blob == blob && !r.storage.has_backup);  // nothing changed
    const char *good = R"({"format":"summit-settings","schema_version":1,"values":{"display/brightness":55}})";
    CHECK(r.post("/api/settings/import/validate", good).status == 200 && r.storage.blob == blob);  // dry run only
    CHECK(r.post("/api/settings/import", good).status == 200);
    CHECK(std::get<int64_t>(r.settings.get("display/brightness")) == 55);
    // Undo import restores what was there before.
    CHECK(r.post("/api/settings/undo", "{}").status == 200);
    CHECK(std::get<int64_t>(r.settings.get("display/brightness")) == 30);
}

TEST(secrets_left_out_of_exports_unless_asked)
{
    Rig r;
    r.wifi.s = "home";
    r.wifi.pw = "wifi-secret-pw";
    r.settings.set("printer/access_code", std::string("12345678"));
    Response e = r.get("/api/settings/export");
    CHECK(e.body.find("wifi-secret-pw") == std::string::npos && e.body.find("12345678") == std::string::npos);
    CHECK(Json(e.body).s("wifi.ssid") == "home");
    Response es = r.api.handle({"GET", "/api/settings/export", "", "", "secrets=1"});
    CHECK(es.body.find("wifi-secret-pw") != std::string::npos && es.body.find("12345678") != std::string::npos);
    CHECK(es.filename == "summit-settings-0.1.0-with-secrets.json");
    // GET /api/settings never shows a secret, only whether it is set.
    Response g = r.get("/api/settings");
    CHECK(g.body.find("12345678") == std::string::npos && g.body.find("\"is_set\":true") != std::string::npos);
    CHECK(g.body.find("\"label\":\"Printer access code\"") != std::string::npos);

    // Import of a file without secrets keeps the current ones (setting and Wi-Fi password).
    Rig other;
    other.wifi.pw = "other-pw-123";
    other.settings.set("printer/access_code", std::string("99999999"));
    CHECK(other.post("/api/settings/import", e.body).status == 200);
    CHECK(std::get<std::string>(other.settings.get("printer/access_code")) == "99999999");
    CHECK(other.wifi.s == "home" && other.wifi.pw == "other-pw-123");
    // With secrets, they are replaced.
    CHECK(other.post("/api/settings/import", es.body).status == 200);
    CHECK(std::get<std::string>(other.settings.get("printer/access_code")) == "12345678" &&
          other.wifi.pw == "wifi-secret-pw");
}

int main()
{
    RUN(routing_and_request_guards);
    RUN(wifi_password_is_write_only);
    RUN(firmware_commands_go_through_the_update_engine);
    RUN(firmware_upload_streams_into_engine);
    RUN(settings_schema_get_and_typed_post);
    RUN(settings_export_import_round_trip);
    RUN(settings_reset_and_unreadable_export);
    RUN(diagnostics_lists_addon_warnings);
    RUN(import_is_validated_first_and_refused_cleanly);
    RUN(secrets_left_out_of_exports_unless_asked);
    return report("web api");
}
