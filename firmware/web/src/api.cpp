#include "summit/web/api.h"

#include <cmath>
#include <cstring>
#include <memory>

#include "cJSON.h"
#include "summit/json_writer.h"

namespace summit::web {

namespace {

using JsonPtr = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;

JsonPtr parse(const std::string &body) { return JsonPtr(cJSON_ParseWithLength(body.data(), body.size()), cJSON_Delete); }

Response json(int status, const std::string &body)
{
    Response r;
    r.status = status;
    r.body = body;
    return r;
}

Response error(int status, const std::string &message)
{
    JsonWriter w;
    w.begin_object().key("ok").value(false).key("error").value(message).end_object();
    return json(status, w.str());
}

Response accepted()
{
    return json(202, "{\"ok\":true}");
}

const char *type_name(ValueType t)
{
    switch (t) {
    case ValueType::None: return "none";
    case ValueType::Bool: return "bool";
    case ValueType::Int: return "int";
    case ValueType::Double: return "double";
    case ValueType::String: return "string";
    }
    return "?";
}

const char *set_result_text(SetResult r)
{
    switch (r) {
    case SetResult::Ok: return "ok";
    case SetResult::Unchanged: return "unchanged";
    case SetResult::UnknownKey: return "unknown setting";
    case SetResult::WrongType: return "wrong type";
    case SetResult::NotAChoice: return "not an allowed value";
    case SetResult::ReadOnly: return "settings are read-only until updated or reset";
    case SetResult::StorageError: return "could not save";
    }
    return "?";
}

const char *notice_code(SettingsNotice n)
{
    switch (n) {
    case SettingsNotice::None: return "none";
    case SettingsNotice::NewerVersion: return "newer_version";
    case SettingsNotice::NoMigration: return "no_migration";
    case SettingsNotice::Unreadable: return "unreadable";
    }
    return "?";
}

const SettingDef *find_def(const SettingsSchema &schema, const std::string &key)
{
    for (const SettingDef &d : schema.defs)
        if (d.key == key) return &d;
    return nullptr;
}

// JSON value -> Value. Known keys use their schema type for numbers; unknown keys
// keep integers as int. Unsupported JSON (objects, arrays) -> false.
bool to_value(const cJSON *j, const SettingDef *d, Value &out)
{
    if (cJSON_IsNull(j)) out = Value{};
    else if (cJSON_IsBool(j)) out = cJSON_IsTrue(j) != 0;
    else if (cJSON_IsString(j)) out = std::string(j->valuestring);
    else if (cJSON_IsNumber(j)) {
        const double v = j->valuedouble;
        const bool want_double = d && type_of(d->default_value) == ValueType::Double;
        if (!want_double && std::floor(v) == v && std::fabs(v) <= 9007199254740992.0) out = static_cast<int64_t>(v);
        else out = v;
    } else return false;
    return true;
}

// Raw JSON from a state topic, re-validated so a bad producer can't break the response.
std::string json_array_or_empty(const std::string &raw)
{
    JsonPtr j = parse(raw);
    if (!j || !cJSON_IsArray(j.get())) return "[]";
    char *s = cJSON_PrintUnformatted(j.get());
    std::string out = s ? s : "[]";
    cJSON_free(s);
    return out;
}

bool valid_version_text(const std::string &v)
{
    if (v.empty() || v.size() > 31) return false;
    for (char c : v) {
        bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '.' || c == '-';
        if (!ok) return false;
    }
    return true;
}

bool confirmed(const std::string &body)
{
    JsonPtr j = parse(body);
    const cJSON *c = j ? cJSON_GetObjectItemCaseSensitive(j.get(), "confirm") : nullptr;
    return cJSON_IsTrue(c);
}

}  // namespace

std::string snapshot_to_json(const SettingsSnapshot &snap, const SettingsSchema &schema, const std::string &firmware,
                             bool include_secrets, const WifiExport &wifi)
{
    JsonWriter w;
    w.begin_object().key("format").value("summit-settings").key("schema_version").value(snap.schema_version);
    w.key("firmware").value(firmware).key("includes_secrets").value(include_secrets).key("values").begin_object();
    for (const auto &kv : snap.values) {
        const SettingDef *d = find_def(schema, kv.first);
        if (d && d->secret && !include_secrets) continue;
        w.key(kv.first).value(kv.second);
    }
    w.end_object();
    if (wifi.present) {
        w.key("wifi").begin_object().key("ssid").value(wifi.ssid);
        if (include_secrets) w.key("password").value(wifi.password);
        w.end_object();
    }
    w.end_object();
    return w.str();
}

bool snapshot_from_json(const std::string &body, const SettingsSchema &schema, SettingsSnapshot &out,
                        WifiExport &wifi, std::string *err)
{
    JsonPtr j = parse(body);
    if (!j || !cJSON_IsObject(j.get())) {
        *err = "not a JSON object";
        return false;
    }
    const cJSON *format = cJSON_GetObjectItemCaseSensitive(j.get(), "format");
    if (!cJSON_IsString(format) || std::string(format->valuestring) != "summit-settings") {
        *err = "not a Project Summit settings file";
        return false;
    }
    const cJSON *version = cJSON_GetObjectItemCaseSensitive(j.get(), "schema_version");
    if (!cJSON_IsNumber(version) || version->valuedouble < 1 || version->valuedouble > 1e6 ||
        std::floor(version->valuedouble) != version->valuedouble) {
        *err = "missing or invalid schema_version";
        return false;
    }
    const cJSON *values = cJSON_GetObjectItemCaseSensitive(j.get(), "values");
    if (!cJSON_IsObject(values)) {
        *err = "missing values";
        return false;
    }
    SettingsSnapshot s;
    s.schema_version = static_cast<uint32_t>(version->valuedouble);
    const cJSON *item = nullptr;
    cJSON_ArrayForEach(item, values)
    {
        std::string key = item->string ? item->string : "";
        if (!valid_topic(key) || key[0] == '_') {
            *err = "invalid key: " + key;
            return false;
        }
        Value v;
        if (!to_value(item, find_def(schema, key), v)) {
            *err = "unsupported value for " + key;
            return false;
        }
        s.values[key] = v;
    }
    WifiExport wf;
    const cJSON *w = cJSON_GetObjectItemCaseSensitive(j.get(), "wifi");
    if (w) {
        const cJSON *ssid = cJSON_GetObjectItemCaseSensitive(w, "ssid");
        const cJSON *pass = cJSON_GetObjectItemCaseSensitive(w, "password");
        if (!cJSON_IsObject(w) || !cJSON_IsString(ssid) || (pass && !cJSON_IsString(pass)) ||
            std::strlen(ssid->valuestring) == 0 || std::strlen(ssid->valuestring) > 32) {
            *err = "invalid wifi section";
            return false;
        }
        wf.present = true;
        wf.ssid = ssid->valuestring;
        if (pass) {
            wf.has_password = true;
            wf.password = pass->valuestring;
            if (!wf.password.empty() && (wf.password.size() < 8 || wf.password.size() > 63)) {
                *err = "invalid Wi-Fi password in file";
                return false;
            }
        }
    }
    out = std::move(s);
    wifi = wf;
    return true;
}

std::string WebApi::str(const char *key) const
{
    Value v = state_.get(key);
    const std::string *s = std::get_if<std::string>(&v);
    return s ? *s : std::string();
}

Response WebApi::handle(const Request &req)
{
    const std::string &p = req.path;
    const bool get = req.method == "GET", post = req.method == "POST";
    if (post) {
        // Only JSON bodies: a cross-site form post can't send them without a CORS
        // preflight, which this device never approves.
        if (req.content_type.rfind("application/json", 0) != 0) return error(415, "expected application/json");
        if (static_cast<int64_t>(req.body.size()) > kMaxJsonBody) return error(413, "request too large");
    }
    if (p == "/api/status" && get) return status();
    if (p == "/api/wifi" && get) return wifi_get();
    if (p == "/api/wifi" && post) return wifi_post(req.body);
    if (p == "/api/firmware" && get) return firmware_get();
    if (p.rfind("/api/firmware/", 0) == 0 && post && p != "/api/firmware/upload")
        return firmware_command(p.substr(14), req.body);
    if (p == "/api/settings" && get) return settings_get();
    if (p == "/api/settings" && post) return settings_post(req.body);
    if (p == "/api/settings/export" && get) return settings_export(req.query.find("secrets=1") != std::string::npos);
    if (p == "/api/settings/import" && post) return settings_import(req.body, false);
    if (p == "/api/settings/import/validate" && post) return settings_import(req.body, true);
    if (p == "/api/settings/undo" && post) return settings_undo();
    if (p == "/api/settings/reset" && post) return settings_reset(req.body);
    if (p == "/api/settings/backup" && get) return settings_backup();
    if (p == "/api/diagnostics" && get) return diagnostics();
    if (modules_ && p == "/api/modules" && get) return modules_get();
    if (modules_ && post && (p == "/api/modules/install" || p == "/api/modules/remove" || p == "/api/modules/delete-data"))
        return modules_command(p.substr(13), req.body);
    static const char *known[] = {"/api/status", "/api/wifi", "/api/firmware", "/api/settings",
                                  "/api/settings/export", "/api/settings/import", "/api/settings/reset",
                                  "/api/settings/backup", "/api/settings/import/validate", "/api/settings/undo",
                                  "/api/diagnostics", "/api/modules", "/api/modules/install", "/api/modules/remove",
                                  "/api/modules/delete-data"};
    for (ApiExtension *ext : extensions_) {
        Response out;
        if (ext->handle(req, out)) return out;
    }
    for (const char *k : known)
        if (p == k) return error(405, "method not allowed");
    return error(404, "not found");
}

Response WebApi::status()
{
    JsonWriter w;
    w.begin_object().key("firmware").value(str("system/firmware_version"));
    w.key("hardware_id").value(str("system/hardware_id"));
    w.key("wifi").begin_object().key("connected").value(state_.get("net/wifi_connected"));
    w.key("ssid").value(str("net/ssid")).key("ip").value(str("net/ip")).end_object();
    w.key("power").begin_object().key("battery_pct").value(state_.get("power/battery_pct"));
    w.key("usb_power").value(state_.get("power/usb_power")).end_object();
    w.key("settings_notice").value(str("system/settings_notice"));
    w.key("update").begin_object().key("stage").value(str("update/stage"));
    w.key("available_version").value(str("update/available_version")).end_object();
    w.key("current_panel").value(str("shell/current_panel")).end_object();
    return json(200, w.str());
}

Response WebApi::wifi_get()
{
    JsonWriter w;
    w.begin_object().key("configured_ssid").value(wifi_.ssid());
    w.key("connected").value(state_.get("net/wifi_connected"));
    w.key("ssid").value(str("net/ssid")).key("ip").value(str("net/ip"));
    w.key("error").value(str("net/last_error"));  // why the last attempt failed, in words ("" when fine)
    w.key("networks").raw(json_array_or_empty(str("net/scan"))).end_object();
    return json(200, w.str());
}

Response WebApi::wifi_post(const std::string &body)
{
    JsonPtr j = parse(body);
    const cJSON *ssid = j ? cJSON_GetObjectItemCaseSensitive(j.get(), "ssid") : nullptr;
    const cJSON *pass = j ? cJSON_GetObjectItemCaseSensitive(j.get(), "password") : nullptr;
    if (!cJSON_IsString(ssid) || (pass && !cJSON_IsString(pass))) return error(400, "ssid and password must be strings");
    const std::string s = ssid->valuestring, pw = pass ? pass->valuestring : "";
    if (s.empty() || s.size() > 32) return error(400, "SSID must be 1-32 bytes");
    if (!pw.empty() && (pw.size() < 8 || pw.size() > 63)) return error(400, "password must be empty (open) or 8-63 characters");
    std::string err;
    if (!wifi_.save(s, pw, &err)) return error(500, "could not save Wi-Fi settings: " + err);
    bus_.publish("net/connect", s);  // the network service reconnects; the password never leaves the store
    return accepted();
}

Response WebApi::firmware_get()
{
    JsonWriter w;
    w.begin_object().key("firmware").value(str("system/firmware_version"));
    w.key("hardware_id").value(str("system/hardware_id"));
    w.key("channel").value(settings_.get("update/channel"));
    w.key("stage").value(str("update/stage")).key("progress_pct").value(state_.get("update/progress_pct"));
    w.key("in_progress").value(state_.get("update/in_progress")).key("error").value(str("update/error"));
    w.key("target_version").value(str("update/target_version"));
    w.key("available_version").value(str("update/available_version")).key("notes").value(str("update/notes"));
    w.key("previous_version").value(str("update/previous_version"));
    w.key("catalog").raw(json_array_or_empty(str("update/catalog"))).end_object();
    return json(200, w.str());
}

Response WebApi::firmware_command(const std::string &cmd, const std::string &body)
{
    if (cmd == "check") {
        bus_.publish("update/command", std::string("check"));
        return accepted();
    }
    if (cmd == "install") {
        JsonPtr j = parse(body);
        const cJSON *v = j ? cJSON_GetObjectItemCaseSensitive(j.get(), "version") : nullptr;
        if (!cJSON_IsString(v) || !valid_version_text(v->valuestring)) return error(400, "version required");
        bus_.publish("update/command", std::string("install:") + v->valuestring);
        return accepted();
    }
    if (cmd == "reboot") {
        if (str("update/stage") != "ready_to_reboot") return error(409, "no update is waiting for a restart");
        bus_.publish("update/command", std::string("reboot"));
        return accepted();
    }
    if (cmd == "previous") {
        if (str("update/previous_version").empty()) return error(409, "no previous version available");
        if (!confirmed(body)) return error(400, "confirmation required");
        bus_.publish("update/command", std::string("previous"));
        return accepted();
    }
    return error(404, "not found");
}

Response WebApi::settings_get()
{
    const SettingsSchema &schema = settings_.schema();
    JsonWriter w;
    w.begin_object().key("schema_version").value(schema.version);
    w.key("notice").value(notice_code(settings_.notice()));
    w.key("notice_text").value(notice_message(settings_.notice()));
    w.key("read_only").value(settings_.read_only()).key("settings").begin_array();
    for (const SettingDef &d : schema.defs) {
        w.begin_object().key("key").value(d.key).key("type").value(type_name(type_of(d.default_value)));
        if (!d.label.empty()) w.key("label").value(d.label);
        if (!d.help.empty()) w.key("help").value(d.help);
        if (d.secret) {
            // Write-only: report whether it is set, never the value.
            Value v = settings_.get(d.key);
            const std::string *sv = std::get_if<std::string>(&v);
            w.key("secret").value(true).key("is_set").value(sv ? !sv->empty() : v != d.default_value);
            w.key("value").null().key("default").null();
        } else {
            w.key("value").value(settings_.get(d.key)).key("default").value(d.default_value);
        }
        if (!d.choices.empty()) {
            w.key("choices").begin_array();
            for (const auto &c : d.choices) w.value(c);
            w.end_array();
        }
        w.end_object();
    }
    w.end_array().end_object();
    return json(200, w.str());
}

Response WebApi::settings_post(const std::string &body)
{
    JsonPtr j = parse(body);
    if (!j || !cJSON_IsObject(j.get())) return error(400, "expected an object of key: value");
    const SettingsSchema &schema = settings_.schema();
    JsonWriter w;
    bool all_ok = true;
    w.begin_object().key("results").begin_object();
    const cJSON *item = nullptr;
    cJSON_ArrayForEach(item, j.get())
    {
        const std::string key = item->string ? item->string : "";
        Value v;
        SetResult r = SetResult::WrongType;
        if (to_value(item, find_def(schema, key), v)) r = settings_.set(key, v);
        if (r != SetResult::Ok && r != SetResult::Unchanged) all_ok = false;
        w.key(key).value(set_result_text(r));
    }
    w.end_object().key("ok").value(all_ok).end_object();
    return json(all_ok ? 200 : 400, w.str());
}

Response WebApi::settings_export(bool include_secrets)
{
    SettingsSnapshot snap = settings_.snapshot();
    const std::string fw = str("system/firmware_version");
    if (snap.schema_version == 0) {
        // Stored data can't be decoded: hand out the raw bytes rather than nothing.
        std::string raw = settings_.stored_blob();
        if (raw.empty()) return error(404, "no stored settings");
        Response r;
        r.content_type = "application/octet-stream";
        r.body = raw;
        r.filename = "summit-settings-raw.bin";
        return r;
    }
    WifiExport wifi;
    wifi.ssid = wifi_.ssid();
    wifi.present = !wifi.ssid.empty();
    if (include_secrets) wifi.password = wifi_.password();
    Response r = json(200, snapshot_to_json(snap, settings_.schema(), fw, include_secrets, wifi));
    r.filename = "summit-settings-" + (fw.empty() ? std::string("device") : fw) +
                 (include_secrets ? "-with-secrets" : "") + ".json";
    return r;
}

static Response import_refused(const ImportResult &r)
{
    JsonWriter w;
    std::string message = import_error_text(r.error);
    if (r.error == ImportError::InvalidValues) {
        message += ": ";
        for (size_t i = 0; i < r.invalid.size(); ++i) message += (i ? ", " : "") + r.invalid[i];
    }
    w.begin_object().key("ok").value(false).key("error").value(message).key("invalid").begin_array();
    for (const auto &k : r.invalid) w.value(k);
    w.end_array().end_object();
    const bool conflict = r.error == ImportError::NewerSchema || r.error == ImportError::MigrationMissing;
    const bool bad = r.error == ImportError::InvalidValues;
    return json(conflict ? 409 : bad ? 400 : 500, w.str());
}

Response WebApi::settings_import(const std::string &body, bool dry_run)
{
    // Validate the whole file before anything changes.
    SettingsSnapshot snap;
    WifiExport wifi;
    std::string err;
    if (!snapshot_from_json(body, settings_.schema(), snap, wifi, &err)) return error(400, err);
    ImportResult check = settings_.validate_import(snap);
    if (check.error != ImportError::None) return import_refused(check);
    if (dry_run) {
        JsonWriter w;
        w.begin_object().key("ok").value(true).key("schema_version").value(snap.schema_version);
        w.key("settings").value(static_cast<int64_t>(snap.values.size())).key("wifi").value(wifi.present);
        w.end_object();
        return json(200, w.str());
    }
    ImportResult r = settings_.import(snap);
    if (r.error != ImportError::None) return import_refused(r);
    if (wifi.present) {
        // No password in the file: keep the current one.
        std::string e;
        wifi_.save(wifi.ssid, wifi.has_password ? wifi.password : wifi_.password(), &e);
    }
    state_.set("system/settings_notice", std::string());
    return json(200, "{\"ok\":true,\"undo\":true}");
}

Response WebApi::settings_undo()
{
    std::string blob;
    SettingsMap stored;
    if (!settings_.backup_blob(blob) || blob.empty()) return error(404, "nothing to undo");
    if (!decode_settings(blob, stored)) return error(409, "the backup can't be read; download it instead");
    SettingsSnapshot snap;
    auto it = stored.find(Settings::kVersionKey);
    const int64_t *v = it != stored.end() ? std::get_if<int64_t>(&it->second) : nullptr;
    snap.schema_version = v ? static_cast<uint32_t>(*v) : 0;
    stored.erase(Settings::kVersionKey);
    snap.values = std::move(stored);
    ImportResult r = settings_.import(snap);  // backs up the current settings in turn
    if (r.error != ImportError::None) return import_refused(r);
    state_.set("system/settings_notice", std::string());
    return json(200, "{\"ok\":true}");
}

Response WebApi::settings_reset(const std::string &body)
{
    if (!confirmed(body)) return error(400, "confirmation required");
    ResetResult r = settings_.reset();
    if (r == ResetResult::BackupFailed) return error(500, "could not back up the current settings; nothing was reset");
    if (r == ResetResult::WriteFailed) return error(500, "could not save default settings");
    state_.set("system/settings_notice", std::string());
    return json(200, "{\"ok\":true}");
}

Response WebApi::settings_backup()
{
    std::string blob;
    if (!settings_.backup_blob(blob) || blob.empty()) return error(404, "no backup (created by reset or import)");
    Response r;
    r.content_type = "application/octet-stream";
    r.body = blob;
    r.filename = "summit-settings-backup.bin";
    return r;
}

Response WebApi::diagnostics()
{
    JsonWriter w;
    w.begin_object().key("warnings").begin_array();
    const std::string warnings = str("diag/warnings");
    size_t start = 0;
    while (start < warnings.size()) {
        size_t end = warnings.find('\n', start);
        if (end == std::string::npos) end = warnings.size();
        if (end > start) w.value(warnings.substr(start, end - start));
        start = end + 1;
    }
    w.end_array();
    w.key("settings").begin_object().key("notice").value(notice_code(settings_.notice()));
    w.key("schema_version").value(settings_.schema_version()).key("stored_version").value(settings_.stored_version());
    w.end_object();
    EventBus::Stats b = bus_.stats();
    w.key("event_bus").begin_object().key("published").value(b.published).key("delivered").value(b.delivered);
    w.key("dropped").value(b.dropped).key("high_water").value(static_cast<int64_t>(b.high_water)).end_object();
    w.key("update").begin_object().key("stage").value(str("update/stage")).key("error").value(str("update/error"));
    w.end_object().key("device").begin_object();
    for (const auto &kv : facts_.facts()) w.key(kv.first).value(kv.second);
    w.end_object().end_object();
    return json(200, w.str());
}

Response json_error(int status, const std::string &message) { return error(status, message); }

std::string query_param(const std::string &query, const std::string &key)
{
    size_t start = 0;
    while (start <= query.size()) {
        size_t end = query.find('&', start);
        if (end == std::string::npos) end = query.size();
        const std::string pair = query.substr(start, end - start);
        const size_t eq = pair.find('=');
        if (pair.substr(0, eq) == key) {
            std::string raw = eq == std::string::npos ? "" : pair.substr(eq + 1), out;
            for (size_t i = 0; i < raw.size(); ++i) {
                auto hex = [](char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; };
                if (raw[i] == '%' && i + 2 < raw.size() + 0 && hex(raw[i + 1]) >= 0 && hex(raw[i + 2]) >= 0) {
                    out += static_cast<char>(hex(raw[i + 1]) * 16 + hex(raw[i + 2]));
                    i += 2;
                } else {
                    out += raw[i] == '+' ? ' ' : raw[i];
                }
            }
            return out;
        }
        start = end + 1;
    }
    return "";
}

std::unique_ptr<UploadSink> WebApi::open_upload(const Request &req, int64_t length, Response &err)
{
    if (length <= 0) {
        err = error(411, "Content-Length required");
        return nullptr;
    }
    for (ApiExtension *ext : extensions_) {
        Response e = error(404, "not found");
        std::unique_ptr<UploadSink> sink = ext->upload(req, length, e);
        if (sink) return sink;
        if (e.status != 404) {
            err = e;
            return nullptr;
        }
    }
    err = error(404, "not found");
    return nullptr;
}

Response WebApi::upload_refused() { return error(403, "firmware uploads need the X-Summit-Upload: 1 header"); }

static ModuleEnv module_env(StateStore &state)
{
    ModuleEnv e;
    Value p = state.get("storage/sd_present"), f = state.get("storage/sd_free_mb");
    e.sd_present = std::get_if<bool>(&p) && std::get<bool>(p);
    e.sd_free_mb = std::get_if<int64_t>(&f) ? std::get<int64_t>(f) : 0;
    return e;
}

Response WebApi::modules_get()
{
    const ModuleEnv env = module_env(state_);
    JsonWriter w;
    w.begin_object().key("modules").begin_array();
    for (const ModuleManifest &m : modules_->modules()) {
        w.begin_object().key("id").value(m.id).key("name").value(m.name).key("description").value(m.description);
        w.key("installed").value(modules_->installed(settings_, m.id));
        ModuleData *data = modules_->data(m.id);
        const int n = data ? data->count() : 0;
        w.key("data_count").value(n).key("data_noun").value(data ? data->noun(n) : std::string());
        w.key("delete_question").value(n > 0 ? delete_data_question(*data) : std::string());
        w.key("requirements").value(requirements_text(m.requires_)).key("missing").begin_array();
        for (const auto &x : missing_requirements(m.requires_, env)) w.value(x);
        w.end_array().end_object();
    }
    w.end_array().end_object();
    return json(200, w.str());
}

Response WebApi::modules_command(const std::string &cmd, const std::string &body)
{
    JsonPtr j = parse(body);
    const cJSON *id = j ? cJSON_GetObjectItemCaseSensitive(j.get(), "id") : nullptr;
    if (!cJSON_IsString(id)) return error(400, "id required");
    if (cmd == "delete-data") {
        // Separate from remove, and only with the count the user was shown in the confirm.
        ModuleData *data = modules_->find(id->valuestring) ? modules_->data(id->valuestring) : nullptr;
        if (!data) return error(404, "unknown module");
        const cJSON *confirm = cJSON_GetObjectItemCaseSensitive(j.get(), "confirm");
        const cJSON *count = cJSON_GetObjectItemCaseSensitive(j.get(), "count");
        if (!cJSON_IsTrue(confirm) || !cJSON_IsNumber(count)) return error(400, "confirmation required");
        const int now = data->count();
        if (now == 0) return error(409, "nothing to delete");
        if (static_cast<int>(count->valuedouble) != now)
            return error(409, "the number of items changed; check and confirm again");
        int deleted = 0;
        if (!data->delete_all(&deleted)) return error(500, "some files could not be deleted");
        JsonWriter w;
        w.begin_object().key("ok").value(true).key("deleted").value(deleted).end_object();
        return json(200, w.str());
    }
    std::vector<std::string> missing;
    ModuleResult r = cmd == "install" ? modules_->install(settings_, id->valuestring, module_env(state_), &missing)
                                      : modules_->remove(settings_, id->valuestring);  // remove keeps the data
    switch (r) {
    case ModuleResult::Ok: return json(200, "{\"ok\":true}");
    case ModuleResult::UnknownModule: return error(404, "unknown module");
    case ModuleResult::AlreadyInstalled: return error(409, "already installed");
    case ModuleResult::NotInstalled: return error(409, "not installed");
    case ModuleResult::MissingRequirements: return error(409, missing.empty() ? "requirements not met" : missing.front());
    case ModuleResult::SettingsError: break;
    }
    return error(500, "could not save the setting");
}

bool WebApi::upload_begin(int64_t length, Response *err)
{
    if (length <= 0) {
        *err = error(411, "Content-Length required");
        return false;
    }
    std::string e;
    if (!update_.upload_begin(length, &e)) {
        *err = error(409, e.empty() ? "update engine busy" : e);
        return false;
    }
    return true;
}

bool WebApi::upload_chunk(const uint8_t *data, size_t len) { return update_.upload_data(data, len); }

Response WebApi::upload_finish()
{
    std::string e;
    if (!update_.upload_complete(&e)) return error(422, e.empty() ? str("update/error") : e);
    JsonWriter w;
    w.begin_object().key("ok").value(true).key("version").value(str("update/target_version")).end_object();
    return json(200, w.str());
}

}  // namespace summit::web
