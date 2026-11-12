// recipe.json format 1 (ARCHITECTURE.md "Recipe reader"): model, parser/validator,
// writer, and the library index. Pure logic, host-tested. Unknown fields are ignored;
// a newer `format` is refused.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace summit::recipes {

constexpr int kRecipeFormat = 1;

struct Ingredient {
    bool has_qty = false;  // false: "salt, to taste" — never scaled
    double qty = 0;
    std::string unit;      // "cup", "g", "" (count)
    std::string item;      // required
    std::string note;      // "sifted", optional
};

struct Step {
    std::string text;      // required
    int timer_s = 0;       // 0 = none; otherwise the suggested timer for this step
};

struct Source {
    std::string type;      // "pdf" | "url" | "manual"
    std::string name;      // original file name (pdf)
    std::string url;       // page address (url)
};

struct Recipe {
    std::string title;     // required
    double servings = 0;   // 0 = unknown (no scaling)
    std::vector<Ingredient> ingredients;
    std::vector<Step> steps;
    Source source;
    std::vector<std::string> tags;
    std::string created;   // ISO date/time, display only
    int64_t cooked = 0;    // unix time last opened for cooking; 0 = never
};

// Card mode needs both ingredients and steps.
inline bool has_card(const Recipe &r) { return !r.ingredients.empty() && !r.steps.empty(); }

bool parse_recipe(const std::string &json, Recipe &out, std::string *error);
std::string recipe_to_json(const Recipe &r);

// Folder names under /recipes/: lowercase letters, digits and '-', 1..48 chars.
bool valid_recipe_id(const std::string &id);
// Slug from a title ("Mum's Apple Pie!" -> "mums-apple-pie"); "recipe" if nothing usable.
std::string make_recipe_id(const std::string &title);

// ---- library index (/recipes/index.json), rebuildable from the folders ----

struct IndexEntry {
    std::string id, title;
    std::vector<std::string> tags;
    bool has_card = false;
    bool has_pages = false;
    bool has_photo = false;
    int pages = 0;
    int64_t cooked = 0;
};

// What a scan of one /recipes/<id>/ folder found.
struct RecipeFolder {
    std::string id;
    std::string recipe_json;  // contents of recipe.json ("" if missing)
    int pages = 0;            // files in pages/
    bool has_photo = false;
};

// Entries for every valid folder, recently cooked first, then by title. Folders with a
// bad id or an unreadable recipe.json are skipped and named in `skipped`.
std::vector<IndexEntry> build_index(const std::vector<RecipeFolder> &folders, std::vector<std::string> *skipped);
std::string index_to_json(const std::vector<IndexEntry> &entries);
// False if missing fields / corrupt: the caller rebuilds from the folders.
bool parse_index(const std::string &json, std::vector<IndexEntry> &out);

}  // namespace summit::recipes
