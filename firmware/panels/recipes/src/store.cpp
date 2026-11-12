#include "summit/recipes/store.h"

#include <cstdio>

namespace summit::recipes {

namespace {

constexpr const char *kRoot = "/recipes";
constexpr const char *kIndex = "/recipes/index.json";

bool ends_with_ci(const std::string &s, const char *suffix)
{
    const size_t n = std::char_traits<char>::length(suffix);
    if (s.size() < n) return false;
    for (size_t i = 0; i < n; ++i) {
        char c = s[s.size() - n + i];
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        if (c != suffix[i]) return false;
    }
    return true;
}

}  // namespace

std::string RecipeStore::page_path(const std::string &id, int page)
{
    char name[16];
    std::snprintf(name, sizeof name, "%03d.jpg", page);
    return folder(id) + "/pages/" + name;
}

bool RecipeStore::rebuild_index(std::vector<IndexEntry> &out, std::vector<std::string> *skipped)
{
    if (!storage_.present()) return false;
    std::vector<DirEntry> dirs;
    std::vector<RecipeFolder> folders;
    if (storage_.list(kRoot, dirs)) {
        for (const DirEntry &d : dirs) {
            if (!d.is_dir || d.name == "inbox") continue;
            RecipeFolder f;
            f.id = d.name;
            storage_.read(folder(d.name) + "/recipe.json", f.recipe_json, 256 * 1024);
            std::vector<DirEntry> pages;
            if (storage_.list(folder(d.name) + "/pages", pages)) {
                for (const DirEntry &p : pages)
                    if (!p.is_dir && ends_with_ci(p.name, ".jpg")) ++f.pages;
            }
            f.has_photo = valid_recipe_id(d.name) && storage_.exists(photo_path(d.name));
            folders.push_back(f);
        }
    }
    out = build_index(folders, skipped);
    return storage_.write(kIndex, index_to_json(out));
}

bool RecipeStore::load_index(std::vector<IndexEntry> &out, std::vector<std::string> *skipped)
{
    if (!storage_.present()) return false;
    std::string json;
    if (storage_.read(kIndex, json, 1024 * 1024) && parse_index(json, out)) return true;
    return rebuild_index(out, skipped);
}

bool RecipeStore::load(const std::string &id, Recipe &out, std::string *error)
{
    std::string json;
    if (!valid_recipe_id(id) || !storage_.read(folder(id) + "/recipe.json", json, 256 * 1024)) {
        if (error) *error = "recipe not found";
        return false;
    }
    return parse_recipe(json, out, error);
}

bool RecipeStore::save(const std::string &id, const Recipe &recipe)
{
    if (!valid_recipe_id(id) || recipe.title.empty()) return false;
    if (!storage_.write(folder(id) + "/recipe.json", recipe_to_json(recipe))) return false;
    std::vector<IndexEntry> index;
    return rebuild_index(index);
}

std::string RecipeStore::new_id(const std::string &title)
{
    const std::string base = make_recipe_id(title);
    std::string id = base;
    for (int n = 2; storage_.exists(folder(id)) && n < 1000; ++n) {
        const std::string suffix = "-" + std::to_string(n);
        id = base.substr(0, 48 - suffix.size()) + suffix;
    }
    return id;
}

bool RecipeStore::mark_cooked(const std::string &id, int64_t unix_time)
{
    Recipe r;
    if (!load(id, r)) return false;
    r.cooked = unix_time;
    return save(id, r);
}

std::vector<std::string> RecipeStore::inbox()
{
    std::vector<std::string> out;
    std::vector<DirEntry> entries;
    if (storage_.list("/recipes/inbox", entries)) {
        for (const DirEntry &e : entries)
            if (!e.is_dir && ends_with_ci(e.name, ".pdf")) out.push_back(e.name);
    }
    return out;
}

}  // namespace summit::recipes
