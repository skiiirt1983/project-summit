#include "summit/recipes/jsonld.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

#include "cJSON.h"

namespace summit::recipes {

namespace {

using JsonPtr = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;

std::string lower(std::string s)
{
    for (char &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string trim(const std::string &s)
{
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

void append_utf8(std::string &out, unsigned long cp)
{
    if (cp < 0x80) out += static_cast<char>(cp);
    else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

struct Unit {
    const char *word, *unit;
};
const Unit kUnits[] = {
    {"cup", "cup"}, {"cups", "cup"}, {"tbsp", "tbsp"}, {"tablespoon", "tbsp"}, {"tablespoons", "tbsp"},
    {"tsp", "tsp"}, {"teaspoon", "tsp"}, {"teaspoons", "tsp"}, {"g", "g"}, {"gram", "g"}, {"grams", "g"},
    {"kg", "kg"}, {"ml", "ml"}, {"l", "l"}, {"litre", "l"}, {"litres", "l"}, {"liter", "l"}, {"liters", "l"},
    {"oz", "oz"}, {"ounce", "oz"}, {"ounces", "oz"}, {"lb", "lb"}, {"lbs", "lb"}, {"pound", "lb"}, {"pounds", "lb"},
    {"pinch", "pinch"}, {"clove", "clove"}, {"cloves", "clove"}, {"can", "can"}, {"cans", "can"}, {"tin", "tin"},
    {"tins", "tin"}, {"slice", "slice"}, {"slices", "slice"}, {"stick", "stick"}, {"sticks", "stick"}, {"bunch", "bunch"},
};

// UTF-8 vulgar fractions: ½ ¼ ¾ (C2 BD/BC/BE), ⅓ ⅔ ⅛ (E2 85 93/94/9B).
bool vulgar(const std::string &t, size_t i, double &v, size_t &len)
{
    if (t.compare(i, 2, "\xC2\xBD") == 0) return v = 0.5, len = 2, true;
    if (t.compare(i, 2, "\xC2\xBC") == 0) return v = 0.25, len = 2, true;
    if (t.compare(i, 2, "\xC2\xBE") == 0) return v = 0.75, len = 2, true;
    if (t.compare(i, 3, "\xE2\x85\x93") == 0) return v = 1.0 / 3, len = 3, true;
    if (t.compare(i, 3, "\xE2\x85\x94") == 0) return v = 2.0 / 3, len = 3, true;
    if (t.compare(i, 3, "\xE2\x85\x9B") == 0) return v = 0.125, len = 3, true;
    return false;
}

bool isdig(char c) { return c >= '0' && c <= '9'; }

// Quantity at the start of t; advances i past it and following spaces.
bool quantity(const std::string &t, size_t &i, double &out)
{
    size_t p = i;
    double v = 0, frac;
    size_t flen;
    bool have = false;
    if (p < t.size() && isdig(t[p])) {
        while (p < t.size() && isdig(t[p])) v = v * 10 + (t[p++] - '0');
        have = true;
        if (p + 1 < t.size() && t[p] == '.' && isdig(t[p + 1])) {
            ++p;
            double scale = 0.1;
            while (p < t.size() && isdig(t[p])) v += (t[p++] - '0') * scale, scale *= 0.1;
        } else if (p + 1 < t.size() && t[p] == '/' && isdig(t[p + 1])) {
            size_t q = p + 1;
            double den = 0;
            while (q < t.size() && isdig(t[q])) den = den * 10 + (t[q++] - '0');
            if (den > 0) v /= den, p = q;
        } else {
            size_t q = p;
            while (q < t.size() && t[q] == ' ') ++q;
            if (vulgar(t, q, frac, flen)) {
                v += frac;
                p = q + flen;
            } else if (q > p && q < t.size() && isdig(t[q])) {  // "1 1/2"
                size_t r = q;
                double num = 0, den = 0;
                while (r < t.size() && isdig(t[r])) num = num * 10 + (t[r++] - '0');
                if (r + 1 < t.size() && t[r] == '/' && isdig(t[r + 1])) {
                    ++r;
                    while (r < t.size() && isdig(t[r])) den = den * 10 + (t[r++] - '0');
                    if (den > 0) v += num / den, p = r;
                }
            }
        }
    } else if (vulgar(t, p, frac, flen)) {
        v = frac;
        p += flen;
        have = true;
    }
    if (!have || !(v > 0)) return false;
    // Range "2-3" / "2 – 3": keep the lower number, skip the upper.
    size_t q = p;
    while (q < t.size() && t[q] == ' ') ++q;
    size_t dash = 0;
    if (q < t.size() && t[q] == '-') dash = 1;
    else if (t.compare(q, 3, "\xE2\x80\x93") == 0) dash = 3;
    if (dash) {
        size_t r = q + dash;
        while (r < t.size() && t[r] == ' ') ++r;
        if (r < t.size() && isdig(t[r])) {
            while (r < t.size() && (isdig(t[r]) || t[r] == '.' || t[r] == '/')) ++r;
            p = r;
        }
    }
    while (p < t.size() && t[p] == ' ') ++p;
    out = v;
    i = p;
    return true;
}

bool type_is_recipe(const cJSON *node)
{
    const cJSON *type = cJSON_GetObjectItemCaseSensitive(node, "@type");
    if (cJSON_IsString(type)) return std::strcmp(type->valuestring, "Recipe") == 0;
    const cJSON *t = nullptr;
    if (cJSON_IsArray(type)) {
        cJSON_ArrayForEach(t, type)
        {
            if (cJSON_IsString(t) && std::strcmp(t->valuestring, "Recipe") == 0) return true;
        }
    }
    return false;
}

const cJSON *find_recipe(const cJSON *node, int depth = 0)
{
    if (!node || depth > 6) return nullptr;
    if (cJSON_IsObject(node)) {
        if (type_is_recipe(node)) return node;
        if (const cJSON *r = find_recipe(cJSON_GetObjectItemCaseSensitive(node, "@graph"), depth + 1)) return r;
        return find_recipe(cJSON_GetObjectItemCaseSensitive(node, "mainEntity"), depth + 1);
    }
    if (cJSON_IsArray(node)) {
        const cJSON *it = nullptr;
        cJSON_ArrayForEach(it, node)
        {
            if (const cJSON *r = find_recipe(it, depth + 1)) return r;
        }
    }
    return nullptr;
}

std::string text_of(const cJSON *v)
{
    if (cJSON_IsString(v)) return html_to_text(v->valuestring);
    if (cJSON_IsNumber(v)) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%g", v->valuedouble);
        return buf;
    }
    return "";
}

void add_step(Recipe &r, const std::string &text)
{
    const std::string t = trim(text);
    if (!t.empty() && r.steps.size() < 300) r.steps.push_back({t.substr(0, 4000), 0});
}

// recipeInstructions: a string, or a list of strings / HowToStep / HowToSection.
void add_instructions(Recipe &r, const cJSON *v, int depth = 0)
{
    if (!v || depth > 4) return;
    if (cJSON_IsString(v)) {
        // One string: split on line breaks (kept by html_to_text as '\n').
        std::string s = v->valuestring, part;
        for (size_t i = 0; i < s.size(); ++i) {
            const bool br = s.compare(i, 4, "<br>") == 0 || s.compare(i, 5, "<br/>") == 0 || s.compare(i, 6, "<br />") == 0 ||
                            s.compare(i, 4, "</p>") == 0 || s.compare(i, 5, "</li>") == 0;
            if (s[i] == '\n' || br) {
                add_step(r, html_to_text(part));
                part.clear();
                if (br) i = s.find('>', i);
            } else {
                part += s[i];
            }
        }
        add_step(r, html_to_text(part));
    } else if (cJSON_IsArray(v)) {
        const cJSON *it = nullptr;
        cJSON_ArrayForEach(it, v) add_instructions(r, it, depth + 1);
    } else if (cJSON_IsObject(v)) {
        const cJSON *list = cJSON_GetObjectItemCaseSensitive(v, "itemListElement");
        if (list) {
            add_instructions(r, list, depth + 1);  // HowToSection
        } else {
            const cJSON *text = cJSON_GetObjectItemCaseSensitive(v, "text");
            if (!cJSON_IsString(text)) text = cJSON_GetObjectItemCaseSensitive(v, "name");
            if (cJSON_IsString(text)) add_step(r, html_to_text(text->valuestring));
        }
    }
}

std::string image_url(const cJSON *v, int depth = 0)
{
    if (!v || depth > 3) return "";
    if (cJSON_IsString(v)) return v->valuestring;
    if (cJSON_IsArray(v)) return image_url(cJSON_GetArrayItem(v, 0), depth + 1);
    if (cJSON_IsObject(v)) return image_url(cJSON_GetObjectItemCaseSensitive(v, "url"), depth + 1);
    return "";
}

void add_tags(Recipe &r, const cJSON *v)
{
    auto add = [&r](const std::string &raw) {
        const std::string t = lower(trim(html_to_text(raw)));
        if (t.empty() || t.size() > 40 || r.tags.size() >= 8) return;
        for (const auto &x : r.tags)
            if (x == t) return;
        r.tags.push_back(t);
    };
    if (cJSON_IsString(v)) {
        std::string s = v->valuestring;
        size_t start = 0;
        while (start <= s.size()) {
            size_t end = s.find(',', start);
            if (end == std::string::npos) end = s.size();
            add(s.substr(start, end - start));
            start = end + 1;
        }
    } else if (cJSON_IsArray(v)) {
        const cJSON *it = nullptr;
        cJSON_ArrayForEach(it, v)
        {
            if (cJSON_IsString(it)) add(it->valuestring);
        }
    }
}

}  // namespace

Ingredient parse_ingredient_line(const std::string &line)
{
    std::string t = trim(line);
    // Leading bullets.
    for (const char *b : {"\xE2\x80\xA2", "-", "*", "\xE2\x80\x93"}) {
        const size_t n = std::strlen(b);
        if (t.compare(0, n, b) == 0 && t.size() > n && t[n] == ' ') t = trim(t.substr(n));
    }
    Ingredient g;
    size_t i = 0;
    double q;
    if (quantity(t, i, q)) {
        g.has_qty = true;
        g.qty = q;
        size_t e = i;
        while (e < t.size() && std::isalpha(static_cast<unsigned char>(t[e]))) ++e;
        const std::string word = lower(t.substr(i, e - i));
        const bool boundary = e == t.size() || t[e] == ' ' || t[e] == '.';
        for (const Unit &u : kUnits) {
            if (boundary && word == u.word) {
                g.unit = u.unit;
                i = e;
                if (i < t.size() && t[i] == '.') ++i;
                while (i < t.size() && t[i] == ' ') ++i;
                if (lower(t.substr(i, 3)) == "of ") i += 3;
                break;
            }
        }
    }
    const std::string rest = t.substr(i);
    const size_t comma = rest.find(',');
    g.item = trim(comma == std::string::npos ? rest : rest.substr(0, comma));
    g.note = comma == std::string::npos ? "" : trim(rest.substr(comma + 1));
    if (g.item.empty()) {
        g = Ingredient{};
        g.item = trim(line);
    }
    return g;
}

std::string html_to_text(const std::string &html)
{
    std::string out;
    for (size_t i = 0; i < html.size(); ++i) {
        const char c = html[i];
        if (c == '<') {
            const size_t end = html.find('>', i);
            if (end == std::string::npos) break;
            i = end;
            out += ' ';
        } else if (c == '&') {
            const size_t semi = html.find(';', i);
            const std::string ent = semi != std::string::npos && semi - i <= 8 ? html.substr(i + 1, semi - i - 1) : "";
            if (ent == "amp") out += '&';
            else if (ent == "lt") out += '<';
            else if (ent == "gt") out += '>';
            else if (ent == "quot") out += '"';
            else if (ent == "apos") out += '\'';
            else if (ent == "nbsp") out += ' ';
            else if (ent == "frac12") out += "\xC2\xBD";
            else if (ent == "frac14") out += "\xC2\xBC";
            else if (ent == "frac34") out += "\xC2\xBE";
            else if (ent == "deg") out += "\xC2\xB0";
            else if (ent.size() > 1 && ent[0] == '#') {
                const unsigned long cp = (ent[1] == 'x' || ent[1] == 'X') ? std::strtoul(ent.c_str() + 2, nullptr, 16)
                                                                          : std::strtoul(ent.c_str() + 1, nullptr, 10);
                if (cp >= 0x20) append_utf8(out, cp);
                else out += ' ';
            } else {
                out += c;
                continue;
            }
            i = semi;
        } else {
            out += c;
        }
    }
    // Collapse whitespace.
    std::string clean;
    bool space = false;
    for (char c : out) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            space = true;
        } else {
            if (space && !clean.empty()) clean += ' ';
            space = false;
            clean += c;
        }
    }
    return clean;
}

bool valid_import_url(const std::string &url)
{
    const std::string scheme = "https://";
    if (url.size() > 500 || url.compare(0, scheme.size(), scheme) != 0) return false;
    const size_t end = url.find_first_of("/?#", scheme.size());
    const std::string host = lower(url.substr(scheme.size(), end == std::string::npos ? std::string::npos : end - scheme.size()));
    if (host.empty() || host.size() > 253 || host.find('.') == std::string::npos) return false;
    bool all_numeric = true;
    for (char c : host) {
        if (!((c >= 'a' && c <= 'z') || isdig(c) || c == '-' || c == '.')) return false;  // no credentials, ports, brackets
        if (!(isdig(c) || c == '.')) all_numeric = false;
    }
    if (all_numeric || host.front() == '.' || host.back() == '.') return false;
    for (const char *bad : {".local", ".lan", ".internal", ".home", ".localhost"}) {
        const size_t n = std::strlen(bad);
        if (host.size() >= n && host.compare(host.size() - n, n, bad) == 0) return false;
    }
    for (char c : url)
        if (static_cast<unsigned char>(c) <= ' ' || c == '"' || c == '\\' || c == 0x7F) return false;
    return true;
}

bool extract_recipe_jsonld(const std::string &html, const std::string &page_url, WebRecipe &out, std::string *error)
{
    auto fail = [error](const char *msg) {
        if (error) *error = msg;
        return false;
    };
    const std::string low = lower(html);
    size_t pos = 0;
    bool any_block = false;
    while ((pos = low.find("<script", pos)) != std::string::npos) {
        const size_t tag_end = low.find('>', pos);
        if (tag_end == std::string::npos) break;
        const std::string tag = low.substr(pos, tag_end - pos);
        pos = tag_end + 1;
        if (tag.find("application/ld+json") == std::string::npos) continue;
        const size_t close = low.find("</script", pos);
        if (close == std::string::npos) break;
        any_block = true;
        const std::string json = html.substr(pos, close - pos);
        pos = close;
        JsonPtr doc(cJSON_ParseWithLength(json.data(), json.size()), cJSON_Delete);
        const cJSON *node = find_recipe(doc.get());
        if (!node) continue;  // not JSON, or no Recipe in this block: try the next one

        WebRecipe w;
        Recipe &r = w.recipe;
        r.title = text_of(cJSON_GetObjectItemCaseSensitive(node, "name")).substr(0, 120);
        if (r.title.empty()) continue;
        // recipeYield: 4, "4", "4 servings", ["4", "4 servings"].
        const cJSON *yield = cJSON_GetObjectItemCaseSensitive(node, "recipeYield");
        if (cJSON_IsArray(yield)) yield = cJSON_GetArrayItem(yield, 0);
        const std::string ytext = text_of(yield);
        for (size_t i = 0; i < ytext.size(); ++i) {
            if (isdig(ytext[i])) {
                const double n = std::atof(ytext.c_str() + i);
                if (n >= 1 && n <= 1000) r.servings = n;
                break;
            }
        }
        const cJSON *ings = cJSON_GetObjectItemCaseSensitive(node, "recipeIngredient");
        if (!ings) ings = cJSON_GetObjectItemCaseSensitive(node, "ingredients");  // older markup
        const cJSON *it = nullptr;
        if (cJSON_IsArray(ings)) {
            cJSON_ArrayForEach(it, ings)
            {
                const std::string line = text_of(it);
                if (!line.empty() && r.ingredients.size() < 300) r.ingredients.push_back(parse_ingredient_line(line.substr(0, 400)));
            }
        }
        add_instructions(r, cJSON_GetObjectItemCaseSensitive(node, "recipeInstructions"));
        add_tags(r, cJSON_GetObjectItemCaseSensitive(node, "recipeCategory"));
        add_tags(r, cJSON_GetObjectItemCaseSensitive(node, "keywords"));
        r.source.type = "url";
        r.source.url = page_url;
        const std::string img = image_url(cJSON_GetObjectItemCaseSensitive(node, "image"));
        if (valid_import_url(img)) w.photo_url = img;
        out = std::move(w);
        return true;
    }
    return fail(any_block ? "This page has no recipe data (schema.org Recipe) to import"
                          : "This page has no structured recipe data to import");
}

}  // namespace summit::recipes
