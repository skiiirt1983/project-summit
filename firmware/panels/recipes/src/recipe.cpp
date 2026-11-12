#include "summit/recipes/recipe.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include "cJSON.h"
#include "summit/json_writer.h"

namespace summit::recipes {

namespace {

using JsonPtr = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
JsonPtr parse(const std::string &s) { return JsonPtr(cJSON_ParseWithLength(s.data(), s.size()), cJSON_Delete); }

constexpr size_t kMaxText = 4000;    // one step / field
constexpr int kMaxItems = 300;       // ingredients, steps, tags

const cJSON *get(const cJSON *o, const char *k) { return cJSON_GetObjectItemCaseSensitive(o, k); }

// Optional string: missing/null -> "", wrong type or too long -> false.
bool opt_str(const cJSON *o, const char *k, std::string &out)
{
    const cJSON *v = get(o, k);
    if (!v || cJSON_IsNull(v)) return true;
    if (!cJSON_IsString(v) || std::char_traits<char>::length(v->valuestring) > kMaxText) return false;
    out = v->valuestring;
    return true;
}

bool fail(std::string *error, const std::string &msg)
{
    if (error) *error = msg;
    return false;
}

std::string trim(const std::string &s)
{
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

}  // namespace

bool parse_recipe(const std::string &json, Recipe &out, std::string *error)
{
    JsonPtr j = parse(json);
    if (!j || !cJSON_IsObject(j.get())) return fail(error, "recipe.json is not a JSON object");
    const cJSON *format = get(j.get(), "format");
    if (!cJSON_IsNumber(format) || format->valuedouble != std::floor(format->valuedouble) || format->valuedouble < 1)
        return fail(error, "missing or invalid format");
    if (format->valuedouble > kRecipeFormat) return fail(error, "recipe is in a newer format than this firmware reads");

    Recipe r;
    if (!opt_str(j.get(), "title", r.title) || trim(r.title).empty()) return fail(error, "missing title");
    r.title = trim(r.title);
    const cJSON *servings = get(j.get(), "servings");
    if (servings && !cJSON_IsNull(servings)) {
        if (!cJSON_IsNumber(servings) || servings->valuedouble < 0 || servings->valuedouble > 1000)
            return fail(error, "invalid servings");
        r.servings = servings->valuedouble;
    }

    const cJSON *ings = get(j.get(), "ingredients");
    if (ings && !cJSON_IsNull(ings)) {
        if (!cJSON_IsArray(ings) || cJSON_GetArraySize(ings) > kMaxItems) return fail(error, "invalid ingredients");
        const cJSON *it = nullptr;
        cJSON_ArrayForEach(it, ings)
        {
            Ingredient g;
            if (!cJSON_IsObject(it) || !opt_str(it, "unit", g.unit) || !opt_str(it, "item", g.item) ||
                !opt_str(it, "note", g.note) || trim(g.item).empty())
                return fail(error, "invalid ingredient");
            const cJSON *q = get(it, "qty");
            if (q && !cJSON_IsNull(q)) {
                if (!cJSON_IsNumber(q) || !(q->valuedouble > 0) || q->valuedouble > 1e6)
                    return fail(error, "invalid ingredient quantity");
                g.has_qty = true;
                g.qty = q->valuedouble;
            }
            r.ingredients.push_back(g);
        }
    }

    const cJSON *steps = get(j.get(), "steps");
    if (steps && !cJSON_IsNull(steps)) {
        if (!cJSON_IsArray(steps) || cJSON_GetArraySize(steps) > kMaxItems) return fail(error, "invalid steps");
        const cJSON *it = nullptr;
        cJSON_ArrayForEach(it, steps)
        {
            Step s;
            if (!cJSON_IsObject(it) || !opt_str(it, "text", s.text) || trim(s.text).empty())
                return fail(error, "invalid step");
            const cJSON *t = get(it, "timer_s");
            if (t && !cJSON_IsNull(t)) {
                if (!cJSON_IsNumber(t) || t->valuedouble < 0 || t->valuedouble > 48 * 3600)
                    return fail(error, "invalid step timer");
                s.timer_s = static_cast<int>(t->valuedouble);
            }
            r.steps.push_back(s);
        }
    }

    const cJSON *source = get(j.get(), "source");
    if (source && !cJSON_IsNull(source)) {
        if (!cJSON_IsObject(source) || !opt_str(source, "type", r.source.type) ||
            !opt_str(source, "name", r.source.name) || !opt_str(source, "url", r.source.url))
            return fail(error, "invalid source");
    }
    const cJSON *tags = get(j.get(), "tags");
    if (tags && !cJSON_IsNull(tags)) {
        if (!cJSON_IsArray(tags) || cJSON_GetArraySize(tags) > kMaxItems) return fail(error, "invalid tags");
        const cJSON *it = nullptr;
        cJSON_ArrayForEach(it, tags)
        {
            if (!cJSON_IsString(it)) return fail(error, "invalid tags");
            std::string t = trim(it->valuestring);
            if (!t.empty() && t.size() <= 40) r.tags.push_back(t);
        }
    }
    if (!opt_str(j.get(), "created", r.created)) return fail(error, "invalid created");
    const cJSON *cooked = get(j.get(), "cooked");
    if (cJSON_IsNumber(cooked) && cooked->valuedouble > 0) r.cooked = static_cast<int64_t>(cooked->valuedouble);
    out = std::move(r);
    return true;
}

std::string recipe_to_json(const Recipe &r)
{
    JsonWriter w;
    w.begin_object().key("format").value(kRecipeFormat).key("title").value(r.title);
    if (r.servings > 0) w.key("servings").value(r.servings);
    w.key("ingredients").begin_array();
    for (const Ingredient &g : r.ingredients) {
        w.begin_object();
        if (g.has_qty) w.key("qty").value(g.qty);
        else w.key("qty").null();
        w.key("unit").value(g.unit).key("item").value(g.item);
        if (!g.note.empty()) w.key("note").value(g.note);
        w.end_object();
    }
    w.end_array().key("steps").begin_array();
    for (const Step &s : r.steps) {
        w.begin_object().key("text").value(s.text);
        if (s.timer_s > 0) w.key("timer_s").value(s.timer_s);
        w.end_object();
    }
    w.end_array().key("source").begin_object().key("type").value(r.source.type);
    if (!r.source.name.empty()) w.key("name").value(r.source.name);
    if (!r.source.url.empty()) w.key("url").value(r.source.url);
    w.end_object().key("tags").begin_array();
    for (const auto &t : r.tags) w.value(t);
    w.end_array().key("created").value(r.created);
    if (r.cooked > 0) w.key("cooked").value(r.cooked);
    w.end_object();
    return w.str();
}

bool valid_recipe_id(const std::string &id)
{
    if (id.empty() || id.size() > 48 || id.front() == '-' || id.back() == '-') return false;
    for (char c : id)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
    return id != "inbox";  // reserved folder
}

std::string make_recipe_id(const std::string &title)
{
    std::string id;
    for (unsigned char c : title) {
        if (c >= 'A' && c <= 'Z') c = static_cast<unsigned char>(c - 'A' + 'a');
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            id += static_cast<char>(c);
        } else if (c == '\'' || c >= 0x80) {
            // apostrophes and non-ASCII letters are dropped, not turned into dashes
        } else if (!id.empty() && id.back() != '-') {
            id += '-';
        }
        if (id.size() >= 48) break;
    }
    while (!id.empty() && id.back() == '-') id.pop_back();
    if (id.empty() || id == "inbox") id = "recipe";
    return id;
}

std::vector<IndexEntry> build_index(const std::vector<RecipeFolder> &folders, std::vector<std::string> *skipped)
{
    std::vector<IndexEntry> out;
    for (const RecipeFolder &f : folders) {
        Recipe r;
        std::string err;
        if (!valid_recipe_id(f.id)) {
            if (skipped) skipped->push_back(f.id + ": invalid folder name");
            continue;
        }
        if (!parse_recipe(f.recipe_json, r, &err)) {
            if (skipped) skipped->push_back(f.id + ": " + err);
            continue;
        }
        IndexEntry e;
        e.id = f.id;
        e.title = r.title;
        e.tags = r.tags;
        e.has_card = has_card(r);
        e.pages = f.pages > 0 ? f.pages : 0;
        e.has_pages = e.pages > 0;
        e.has_photo = f.has_photo;
        e.cooked = r.cooked;
        out.push_back(e);
    }
    std::stable_sort(out.begin(), out.end(), [](const IndexEntry &a, const IndexEntry &b) {
        if (a.cooked != b.cooked) return a.cooked > b.cooked;  // recently cooked first
        return a.title < b.title;
    });
    return out;
}

std::string index_to_json(const std::vector<IndexEntry> &entries)
{
    JsonWriter w;
    w.begin_object().key("format").value(1).key("recipes").begin_array();
    for (const IndexEntry &e : entries) {
        w.begin_object().key("id").value(e.id).key("title").value(e.title).key("tags").begin_array();
        for (const auto &t : e.tags) w.value(t);
        w.end_array().key("has_card").value(e.has_card).key("has_pages").value(e.has_pages);
        w.key("has_photo").value(e.has_photo).key("pages").value(e.pages).key("cooked").value(e.cooked).end_object();
    }
    w.end_array().end_object();
    return w.str();
}

bool parse_index(const std::string &json, std::vector<IndexEntry> &out)
{
    JsonPtr j = parse(json);
    if (!j || !cJSON_IsObject(j.get())) return false;
    const cJSON *format = get(j.get(), "format");
    const cJSON *recipes = get(j.get(), "recipes");
    if (!cJSON_IsNumber(format) || format->valuedouble != 1 || !cJSON_IsArray(recipes)) return false;
    std::vector<IndexEntry> result;
    const cJSON *it = nullptr;
    cJSON_ArrayForEach(it, recipes)
    {
        IndexEntry e;
        const cJSON *id = get(it, "id"), *title = get(it, "title"), *tags = get(it, "tags");
        const cJSON *card = get(it, "has_card"), *pages = get(it, "has_pages");
        if (!cJSON_IsString(id) || !valid_recipe_id(id->valuestring) || !cJSON_IsString(title) || !cJSON_IsBool(card) ||
            !cJSON_IsBool(pages))
            return false;
        e.id = id->valuestring;
        e.title = title->valuestring;
        e.has_card = cJSON_IsTrue(card);
        e.has_pages = cJSON_IsTrue(pages);
        e.has_photo = cJSON_IsTrue(get(it, "has_photo"));
        const cJSON *n = get(it, "pages"), *cooked = get(it, "cooked"), *t = nullptr;
        if (cJSON_IsNumber(n) && n->valuedouble >= 0) e.pages = static_cast<int>(n->valuedouble);
        if (cJSON_IsNumber(cooked) && cooked->valuedouble > 0) e.cooked = static_cast<int64_t>(cooked->valuedouble);
        if (cJSON_IsArray(tags)) {
            cJSON_ArrayForEach(t, tags)
            {
                if (cJSON_IsString(t)) e.tags.push_back(t->valuestring);
            }
        }
        result.push_back(e);
    }
    out = std::move(result);
    return true;
}

}  // namespace summit::recipes
