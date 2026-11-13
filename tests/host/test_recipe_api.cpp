#include <cstdlib>
#include <string>
#include <unistd.h>

#include "check.h"
#include "fake_storage.h"
#include "summit/recipes/web_api.h"
#include "summit/system_settings.h"

using namespace summit;
using namespace summit::recipes;
using web::Request;
using web::Response;

struct NoUpdate : web::UpdateControl {
    bool upload_begin(int64_t, std::string *) override { return false; }
    bool upload_data(const uint8_t *, size_t) override { return false; }
    bool upload_complete(std::string *) override { return false; }
    void upload_abort(const char *) override {}
};
struct NoWifi : web::WifiStore {
    std::string ssid() override { return ""; }
    std::string password() override { return ""; }
    bool save(const std::string &, const std::string &, std::string *) override { return true; }
};
struct NoFacts : web::DeviceFacts {
    std::vector<std::pair<std::string, std::string>> facts() override { return {}; }
};

struct Rig {
    std::string root;
    EventBus bus;
    StateStore state{bus};
    FakeStorage nvs;
    Settings settings{system_settings_schema(), nvs, &bus};
    NoUpdate update;
    NoWifi wifi;
    NoFacts facts;
    web::WebApi api{bus, state, settings, update, wifi, facts};
    std::unique_ptr<PosixStorage> sd;
    std::unique_ptr<RecipeStore> store;
    std::unique_ptr<RecipeWebApi> recipes;
    Rig()
    {
        char tmpl[] = "/tmp/summit-api-XXXXXX";
        root = mkdtemp(tmpl);
        sd = std::make_unique<PosixStorage>(root);
        store = std::make_unique<RecipeStore>(*sd);
        recipes = std::make_unique<RecipeWebApi>(*store, *sd);
        api.add_extension(recipes.get());
        settings.load();
    }
    ~Rig() { (void)!system(("rm -rf '" + root + "'").c_str()); }
    Response get(const std::string &path, const std::string &query = "") { return api.handle({"GET", path, "", "", query}); }
    Response post(const std::string &path, const std::string &body) { return api.handle({"POST", path, "application/json", body}); }
    Response put_file(const std::string &query, const std::string &data, int64_t declared = -1)
    {
        Response err;
        auto sink = api.open_upload({"POST", "/api/recipes/file", "application/octet-stream", "", query},
                                    declared < 0 ? static_cast<int64_t>(data.size()) : declared, err);
        if (!sink) return err;
        sink->chunk(reinterpret_cast<const uint8_t *>(data.data()), data.size() / 2);
        sink->chunk(reinterpret_cast<const uint8_t *>(data.data()) + data.size() / 2, data.size() - data.size() / 2);
        return sink->finish();
    }
};

static const char *kSave = R"({"recipe":{"format":1,"title":"Apple Pie","servings":8,
  "ingredients":[{"qty":6,"unit":"","item":"apples"}],"steps":[{"text":"Bake 45 minutes."}],
  "source":{"type":"pdf","name":"pie.pdf"},"tags":["dessert"],"created":"2026-11-13"}})";

TEST(query_params)
{
    CHECK(web::query_param("id=apple-pie&name=pages%2F001.jpg", "name") == "pages/001.jpg");
    CHECK(web::query_param("name=new+one%20two.pdf", "name") == "new one two.pdf" && web::query_param("a=1", "b").empty());
}

TEST(file_name_whitelist)
{
    for (const char *ok : {"pages/001.jpg", "pages/200.jpg", "photo.jpg", "thumb.jpg", "original.pdf"})
        CHECK(RecipeWebApi::valid_file_name(ok));
    for (const char *bad : {"", "recipe.json", "pages/000.jpg", "pages/201.jpg", "pages/1.jpg", "pages/../x.jpg",
                            "../photo.jpg", "pages/001.png", "index.json", "original.pdf.exe"})
        CHECK(!RecipeWebApi::valid_file_name(bad));
}

TEST(save_list_get_and_files)
{
    Rig r;
    CHECK(r.get("/api/recipes").body.find("\"recipes\":[]") != std::string::npos);
    Response s = r.post("/api/recipes/save", kSave);
    CHECK(s.status == 200 && s.body.find("\"id\":\"apple-pie\"") != std::string::npos);
    CHECK(r.post("/api/recipes/save", kSave).body.find("\"id\":\"apple-pie-2\"") != std::string::npos);  // new id each time
    CHECK(r.post("/api/recipes/save", R"({"recipe":{"format":1,"title":""}})").status == 400);
    CHECK(r.post("/api/recipes/save", R"({"recipe":{"format":2,"title":"x"}})").status == 400);
    CHECK(r.post("/api/recipes/save", R"({"id":"nope","recipe":{"format":1,"title":"x"}})").status == 404);
    // Edit in place keeps the id.
    Response e = r.post("/api/recipes/save", R"({"id":"apple-pie","recipe":{"format":1,"title":"Apple Pie - best"}})");
    CHECK(e.status == 200 && r.get("/api/recipes/get", "id=apple-pie").body.find("Apple Pie - best") != std::string::npos);
    CHECK(r.get("/api/recipes/get", "id=../etc").status == 404);

    const std::string jpg(5000, 'J');
    CHECK(r.put_file("id=apple-pie&name=pages%2F001.jpg", jpg).status == 200);
    CHECK(r.put_file("id=apple-pie&name=thumb.jpg", "tiny").status == 200);
    CHECK(r.put_file("id=apple-pie&name=recipe.json", "{}").status == 400);          // not on the whitelist
    CHECK(r.put_file("id=nope&name=photo.jpg", jpg).status == 404);
    CHECK(r.put_file("id=apple-pie&name=photo.jpg", jpg, RecipeWebApi::kMaxImageBytes + 1).status == 413);
    CHECK(r.put_file("id=apple-pie&name=photo.jpg", jpg, 9999).status == 400);       // shorter than declared
    CHECK(!r.sd->exists("/recipes/apple-pie/photo.jpg"));                            // nothing half-written
    std::string data;
    CHECK(r.sd->read("/recipes/apple-pie/pages/001.jpg", data) && data == jpg);
    CHECK(r.get("/api/recipes").body.find("\"pages\":1") != std::string::npos);       // index refreshed
    Response f = r.get("/api/recipes/file", "id=apple-pie&name=pages/001.jpg");
    CHECK(f.status == 200 && f.storage_path == "/recipes/apple-pie/pages/001.jpg" && f.content_type == "image/jpeg");
    CHECK(r.get("/api/recipes/file", "id=apple-pie&name=pages/002.jpg").status == 404);
    CHECK(r.get("/api/recipes/file", "id=apple-pie&name=recipe.json").status == 400);
}

TEST(inbox_download_and_done)
{
    Rig r;
    r.sd->write("/recipes/inbox/Grandma stew.pdf", "%PDF-1.4 stew");
    r.sd->write("/recipes/inbox/notes.txt", "x");
    CHECK(r.get("/api/recipes").body.find("\"inbox\":[\"Grandma stew.pdf\"]") != std::string::npos);
    Response f = r.get("/api/recipes/inbox", "name=Grandma%20stew.pdf");
    CHECK(f.status == 200 && f.storage_path == "/recipes/inbox/Grandma stew.pdf");
    CHECK(r.get("/api/recipes/inbox", "name=..%2F..%2Fsecret.pdf").status == 400);
    CHECK(r.get("/api/recipes/inbox", "name=notes.txt").status == 400 && r.get("/api/recipes/inbox", "name=x.pdf").status == 404);
    r.post("/api/recipes/save", kSave);
    CHECK(r.post("/api/recipes/inbox/done", R"({"name":"Grandma stew.pdf","id":"nope"})").status == 404);
    CHECK(r.post("/api/recipes/inbox/done", R"({"name":"Grandma stew.pdf","id":"apple-pie"})").status == 200);
    std::string data;
    CHECK(r.sd->read("/recipes/apple-pie/original.pdf", data) && data == "%PDF-1.4 stew");  // moved, not deleted
    CHECK(!r.sd->exists("/recipes/inbox/Grandma stew.pdf"));
    CHECK(r.post("/api/recipes/inbox/done", R"({"name":"Grandma stew.pdf","id":"apple-pie"})").status == 409);
}

TEST(no_card_and_routing)
{
    Rig r;
    r.sd->set_present(false);
    CHECK(r.get("/api/recipes").status == 409 && r.post("/api/recipes/save", kSave).status == 409);
    CHECK(r.put_file("id=a&name=photo.jpg", "x").status == 409);
    r.sd->set_present(true);
    CHECK(r.get("/api/status").status == 200);            // core API unaffected
    CHECK(r.get("/api/recipes/nope").status == 404);
    Response err;
    CHECK(!r.api.open_upload({"POST", "/api/other", "", "", ""}, 10, err) && err.status == 404);
    CHECK(!r.api.open_upload({"POST", "/api/recipes/file", "", "", "id=a&name=photo.jpg"}, 0, err) && err.status == 411);
}

int main()
{
    RUN(query_params);
    RUN(file_name_whitelist);
    RUN(save_list_get_and_files);
    RUN(inbox_download_and_done);
    RUN(no_card_and_routing);
    return report("recipe web api");
}
