// The recipe library on the SD card (ARCHITECTURE.md "Recipe reader", storage layout):
//   /recipes/inbox/*.pdf            waiting for conversion in the web UI
//   /recipes/<id>/recipe.json       + pages/001.jpg ..., photo.jpg, original.pdf
//   /recipes/index.json             rebuildable index
// Pure logic over the Storage interface; host-tested with a local folder.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "summit/recipes/recipe.h"
#include "summit/storage.h"

namespace summit::recipes {

class RecipeStore {
public:
    explicit RecipeStore(Storage &storage) : storage_(storage) {}

    bool available() { return storage_.present(); }

    // The library, from index.json; rebuilt from the folders (and saved) when the
    // index is missing or corrupt. `skipped` lists folders that could not be read.
    bool load_index(std::vector<IndexEntry> &out, std::vector<std::string> *skipped = nullptr);
    // Always scans the folders and rewrites index.json.
    bool rebuild_index(std::vector<IndexEntry> &out, std::vector<std::string> *skipped = nullptr);

    bool load(const std::string &id, Recipe &out, std::string *error = nullptr);
    // Writes recipe.json and refreshes the index.
    bool save(const std::string &id, const Recipe &recipe);
    // A folder name for a new recipe that doesn't exist yet ("apple-pie", "apple-pie-2").
    std::string new_id(const std::string &title);
    // Records that the recipe was opened for cooking (for "recently cooked first").
    bool mark_cooked(const std::string &id, int64_t unix_time);

    // How many recipe folders are on the card (readable or not).
    int count();
    // Deletes every recipe folder and the index. Explicit, confirmed user action only
    // ("Delete recipes"). PDFs still waiting in the inbox are kept. Returns how many were deleted.
    bool delete_all(int *deleted = nullptr);

    // PDFs in /recipes/inbox waiting for conversion (file names, sorted).
    std::vector<std::string> inbox();

    static std::string folder(const std::string &id) { return "/recipes/" + id; }
    // "/recipes/<id>/pages/007.jpg" for page 1..N.
    static std::string page_path(const std::string &id, int page);
    static std::string photo_path(const std::string &id) { return folder(id) + "/photo.jpg"; }
    static std::string thumb_path(const std::string &id) { return folder(id) + "/thumb.jpg"; }

private:
    Storage &storage_;
};

}  // namespace summit::recipes
