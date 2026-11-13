#include <cstdlib>
#include <string>
#include <unistd.h>
#include <vector>

#include "check.h"
#include "summit/event_bus.h"
#include "summit/recipes/store.h"
#include "summit/storage.h"

using namespace summit;
using namespace summit::recipes;

static std::string temp_dir()
{
    char tmpl[] = "/tmp/summit-sd-XXXXXX";
    return mkdtemp(tmpl);
}

static void rm_rf(const std::string &dir) { (void)!system(("rm -rf '" + dir + "'").c_str()); }

TEST(path_validation)
{
    for (const char *ok : {"/", "/recipes", "/recipes/apple-pie/pages/001.jpg", "/a b/c.txt"}) CHECK(valid_storage_path(ok));
    for (const char *bad : {"", "recipes", "/a/../b", "/a/./b", "/a//b", "/a/", "/a\\b", "/a:b", "/a\nb"})
        CHECK(!valid_storage_path(bad));
    CHECK(!valid_storage_path("/" + std::string(300, 'a')));
}

TEST(posix_storage_basics)
{
    const std::string root = temp_dir();
    PosixStorage sd(root);
    CHECK(sd.present() && sd.free_mb() > 0);
    CHECK(!sd.exists("/recipes"));
    CHECK(sd.write("/recipes/a/recipe.json", "{}"));  // parents created
    std::string data;
    CHECK(sd.read("/recipes/a/recipe.json", data) && data == "{}");
    CHECK(sd.write("/recipes/a/recipe.json", "{\"x\":1}") && sd.read("/recipes/a/recipe.json", data) && data == "{\"x\":1}");
    CHECK(sd.write("/recipes/b.txt", "") && sd.make_dir("/recipes/inbox"));
    std::vector<DirEntry> e;
    CHECK(sd.list("/recipes", e) && e.size() == 3);
    CHECK(e[0].name == "a" && e[0].is_dir && e[1].name == "b.txt" && !e[1].is_dir && e[2].name == "inbox");
    CHECK(!sd.read("/recipes/a/recipe.json", data, 3));  // over the size limit
    CHECK(!sd.read("/../etc/passwd", data) && !sd.write("/../x", "x") && !sd.list("/nope", e));
    CHECK(!sd.read("/recipes/a", data));  // a folder

    // Streaming writes: nothing visible until commit; an abandoned writer leaves nothing.
    {
        auto w = sd.begin_write("/recipes/a/pages/001.jpg");
        CHECK(w && w->write(reinterpret_cast<const uint8_t *>("abc"), 3) && !sd.exists("/recipes/a/pages/001.jpg"));
        CHECK(w->write(reinterpret_cast<const uint8_t *>("def"), 3) && w->commit());
        CHECK(sd.size("/recipes/a/pages/001.jpg") == 6 && sd.read_chunk("/recipes/a/pages/001.jpg", 2, 3, data) && data == "cde");
        CHECK(sd.read_chunk("/recipes/a/pages/001.jpg", 6, 3, data) && data.empty());
        { auto dropped = sd.begin_write("/recipes/a/pages/002.jpg"); dropped->write(reinterpret_cast<const uint8_t *>("x"), 1); }
        CHECK(!sd.exists("/recipes/a/pages/002.jpg") && sd.size("/recipes/a/pages/002.jpg") == -1);
        CHECK(sd.list("/recipes/a/pages", e) && e.size() == 1);  // no .tmp left behind
        CHECK(!sd.begin_write("/../x"));
        CHECK(sd.move("/recipes/b.txt", "/recipes/c/moved.txt") && sd.exists("/recipes/c/moved.txt") && !sd.exists("/recipes/b.txt"));
        CHECK(!sd.move("/recipes/a/recipe.json", "/recipes/c/moved.txt"));  // never overwrites
        CHECK(!sd.move("/recipes/nope", "/recipes/x") && sd.move("/recipes/c/moved.txt", "/recipes/b.txt"));
    }

    // Card removed: everything fails cleanly.
    sd.set_present(false);
    CHECK(!sd.present() && sd.free_mb() == 0 && !sd.exists("/recipes") && !sd.read("/recipes/a/recipe.json", data) &&
          !sd.write("/x", "y") && !sd.list("/recipes", e));
    EventBus bus;
    StateStore state(bus);
    publish_storage_state(sd, state);
    CHECK(std::get<bool>(state.get("storage/sd_present")) == false && std::get<int64_t>(state.get("storage/sd_free_mb")) == 0);
    sd.set_present(true);
    publish_storage_state(sd, state);
    CHECK(std::get<bool>(state.get("storage/sd_present")) && std::get<int64_t>(state.get("storage/sd_free_mb")) > 0);
    rm_rf(root);
}

static Recipe recipe(const char *title, bool card)
{
    Recipe r;
    r.title = title;
    r.servings = 4;
    if (card) {
        Ingredient g;
        g.item = "flour";
        g.has_qty = true;
        g.qty = 1.5;
        g.unit = "cup";
        r.ingredients.push_back(g);
        r.steps.push_back({"Bake 25 minutes.", 1500});
    }
    r.source.type = "manual";
    return r;
}

TEST(recipe_store_library)
{
    const std::string root = temp_dir();
    PosixStorage sd(root);
    RecipeStore store(sd);
    std::vector<IndexEntry> idx;
    CHECK(store.load_index(idx) && idx.empty());  // empty card: empty library, index created
    CHECK(sd.exists("/recipes/index.json"));

    CHECK(store.new_id("Apple Pie") == "apple-pie");
    CHECK(store.save("apple-pie", recipe("Apple Pie", true)));
    CHECK(store.new_id("Apple Pie") == "apple-pie-2");
    CHECK(store.save("scan", recipe("Old scan", false)));
    sd.write(RecipeStore::page_path("scan", 1), "jpg");
    sd.write(RecipeStore::page_path("scan", 2), "jpg");
    sd.write("/recipes/scan/pages/notes.txt", "x");
    sd.write(RecipeStore::photo_path("apple-pie"), "jpg");
    sd.write("/recipes/inbox/new one.PDF", "%PDF");
    sd.write("/recipes/inbox/readme.txt", "x");
    sd.write("/recipes/broken/recipe.json", "{oops");

    std::vector<std::string> skipped;
    CHECK(store.rebuild_index(idx, &skipped));
    CHECK(idx.size() == 2 && skipped.size() == 1 && skipped[0].find("broken") == 0);
    CHECK(idx[0].id == "apple-pie" && idx[0].has_card && idx[0].has_photo && !idx[0].has_pages);
    CHECK(idx[1].id == "scan" && !idx[1].has_card && idx[1].pages == 2);
    CHECK((store.inbox() == std::vector<std::string>{"new one.PDF"}));

    Recipe r;
    std::string err;
    CHECK(store.load("apple-pie", r, &err) && r.ingredients.size() == 1);
    CHECK(!store.load("nope", r, &err) && err == "recipe not found");
    CHECK(!store.load("../etc", r, &err) && !store.save("Bad Id", r));
    CHECK(!store.load("broken", r, &err) && err == "recipe.json is not a JSON object");

    // Recently cooked first.
    CHECK(store.mark_cooked("scan", 1766000000) && store.load_index(idx) && idx[0].id == "scan");

    // Corrupt or missing index is rebuilt from the folders.
    sd.write("/recipes/index.json", "garbage");
    CHECK(store.load_index(idx) && idx.size() == 2);
    std::string json;
    CHECK(sd.read("/recipes/index.json", json) && json.find("\"apple-pie\"") != std::string::npos);

    // Card removed.
    sd.set_present(false);
    CHECK(!store.available() && !store.load_index(idx) && !store.load("apple-pie", r) && store.inbox().empty());
    rm_rf(root);
}

int main()
{
    RUN(path_validation);
    RUN(posix_storage_basics);
    RUN(recipe_store_library);
    return report("storage + recipe store");
}
