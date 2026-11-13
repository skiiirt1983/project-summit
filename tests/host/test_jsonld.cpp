#include <fstream>
#include <sstream>
#include <string>

#include "check.h"
#include "summit/recipes/jsonld.h"
#include "summit/recipes/quantity.h"

using namespace summit::recipes;

#ifndef FIXTURES
#define FIXTURES "tests/fixtures/web"
#endif

static std::string page(const char *name)
{
    std::ifstream f(std::string(FIXTURES) + "/" + name, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

TEST(ingredient_lines)
{
    struct Case {
        const char *line;
        bool has_qty;
        double qty;
        const char *unit, *item, *note;
    };
    const Case cases[] = {
        {"1 1/2 cups flour, sifted", true, 1.5, "cup", "flour", "sifted"},
        {"3 ripe bananas, mashed", true, 3, "", "ripe bananas", "mashed"},
        {"1/3 cup melted butter", true, 1.0 / 3, "cup", "melted butter", ""},
        {"1 teaspoon baking soda", true, 1, "tsp", "baking soda", ""},
        {"Pinch of salt", false, 0, "", "Pinch of salt", ""},
        {"\xC2\xBD cup sugar", true, 0.5, "cup", "sugar", ""},
        {"1\xC2\xBD tbsp chili powder", true, 1.5, "tbsp", "chili powder", ""},
        {"2-3 lemons, juiced", true, 2, "", "lemons", "juiced"},
        {"500 g minced beef", true, 500, "g", "minced beef", ""},
        {"2 Tablespoons of olive oil", true, 2, "tbsp", "olive oil", ""},
        {"\xE2\x80\xA2 250 ml milk", true, 250, "ml", "milk", ""},
        {"2 large eggs", true, 2, "", "large eggs", ""},
        {"Salt & pepper", false, 0, "", "Salt & pepper", ""},
    };
    for (const Case &c : cases) {
        Ingredient g = parse_ingredient_line(c.line);
        CHECK(g.has_qty == c.has_qty && g.unit == c.unit && g.item == c.item && g.note == c.note);
        if (c.has_qty) CHECK(g.qty > c.qty - 0.001 && g.qty < c.qty + 0.001);
    }
}

TEST(html_text)
{
    CHECK(html_to_text("<p>Brown the beef &amp; onions.</p>") == "Brown the beef & onions.");
    CHECK(html_to_text("350&deg;F  for\n30&#8211;40&nbsp;minutes") == "350\xC2\xB0" "F for 30\xE2\x80\x93" "40 minutes");
    CHECK(html_to_text("1&frac12; cups &quot;good&quot; flour &unknown; x") == "1\xC2\xBD cups \"good\" flour &unknown; x");
}

TEST(url_rules)
{
    for (const char *ok : {"https://www.example-recipes.com/banana-bread", "https://sample-kitchen.org/r?id=3"})
        CHECK(valid_import_url(ok));
    for (const char *bad : {"http://example.com/r", "https://192.168.1.1/admin", "https://localhost/x", "https://printer.local/x",
                            "https://router/x", "https://user:pw@example.com/x", "https://example.com:8443/x", "ftp://example.com/x",
                            "https://nas.lan/x", "https://[::1]/x", "", "https://example.com/a b"})
        CHECK(!valid_import_url(bad));
}

TEST(simple_recipe_object)
{
    WebRecipe w;
    std::string err;
    CHECK(extract_recipe_jsonld(page("simple.html"), "https://www.example-recipes.com/banana-bread", w, &err));
    const Recipe &r = w.recipe;
    CHECK(r.title == "Classic Banana Bread" && r.servings == 8 && r.ingredients.size() == 7 && r.steps.size() == 3);
    CHECK(r.ingredients[0].item == "ripe bananas" && r.ingredients[6].qty == 1.5 && r.ingredients[6].unit == "cup");
    CHECK(r.steps[0].text == "Preheat the oven to 350\xC2\xB0" "F and butter a loaf pan.");
    CHECK(r.source.type == "url" && r.source.url == "https://www.example-recipes.com/banana-bread");
    CHECK((r.tags == std::vector<std::string>{"baking", "banana", "quick bread", "breakfast"}));
    CHECK(w.photo_url == "https://cdn.example-recipes.com/banana-bread-1x1.jpg");
    // What we extract is a valid recipe.json for the device.
    Recipe back;
    CHECK(parse_recipe(recipe_to_json(r), back, &err) && has_card(back));
}

TEST(graph_with_sections_and_entities)
{
    WebRecipe w;
    std::string err;
    CHECK(extract_recipe_jsonld(page("graph.html"), "https://sample-kitchen.org/chili", w, &err));
    const Recipe &r = w.recipe;
    CHECK(r.title == "Weeknight Chili & Rice" && r.servings == 6 && r.ingredients.size() == 5 && r.steps.size() == 3);
    CHECK(r.ingredients[2].qty == 1.5 && r.ingredients[2].unit == "tbsp" && r.ingredients[4].item == "Salt & pepper");
    CHECK(r.steps[0].text == "Brown the beef with the onions for 8 minutes.");
    CHECK(r.steps[1].text == "Add the spices and tomatoes; simmer for 30\xE2\x80\x93" "40 minutes.");
    CHECK(w.photo_url == "https://img.sample-kitchen.org/chili.jpg" && r.tags.size() == 2);
}

TEST(array_with_bad_block_and_string_instructions)
{
    WebRecipe w;
    std::string err;
    CHECK(extract_recipe_jsonld(page("array-string.html"), "https://x.example.com/posset", w, &err));
    const Recipe &r = w.recipe;
    CHECK(r.title == "Lemon Posset" && r.servings == 4 && r.ingredients.size() == 3);
    CHECK(r.ingredients[1].qty == 0.5 && r.ingredients[2].qty == 2 && r.ingredients[2].note == "juiced");
    CHECK(r.steps.size() == 3 && r.steps[2].text == "Chill for 3 hours until set.");
    CHECK(w.photo_url.empty());  // http photo is not fetched
}

TEST(pages_without_a_recipe)
{
    WebRecipe w;
    w.recipe.title = "keep";
    std::string err;
    CHECK(!extract_recipe_jsonld(page("no-recipe.html"), "https://x.example.com/a", w, &err));
    CHECK(err == "This page has no recipe data (schema.org Recipe) to import");
    CHECK(!extract_recipe_jsonld(page("plain.html"), "https://x.example.com/a", w, &err));
    CHECK(err == "This page has no structured recipe data to import" && w.recipe.title == "keep");
    CHECK(!extract_recipe_jsonld("<script type=\"application/ld+json\">{\"@type\":\"Recipe\"}</script>", "https://x.example.com/a", w, &err));
    CHECK(!extract_recipe_jsonld("<script type=\"application/ld+json\">{\"@type\":\"Recipe\",\"name\":\"x\"", "u", w, &err));  // unterminated
}

int main()
{
    RUN(ingredient_lines);
    RUN(html_text);
    RUN(url_rules);
    RUN(simple_recipe_object);
    RUN(graph_with_sections_and_entities);
    RUN(array_with_bad_block_and_string_instructions);
    RUN(pages_without_a_recipe);
    return report("recipe url import (json-ld)");
}
