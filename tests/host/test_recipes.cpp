#include <string>
#include <vector>

#include "check.h"
#include "summit/recipes/quantity.h"
#include "summit/recipes/recipe.h"
#include "summit/recipes/step_time.h"

using namespace summit::recipes;

static const char *kPancakes = R"({
  "format": 1, "title": "  Pancakes ", "servings": 4,
  "ingredients": [
    {"qty": 1.5, "unit": "cup", "item": "flour", "note": "sifted"},
    {"qty": 2, "unit": "", "item": "eggs"},
    {"qty": 250, "unit": "ml", "item": "milk"},
    {"qty": null, "unit": "", "item": "salt", "note": "to taste"}
  ],
  "steps": [
    {"text": "Whisk everything together."},
    {"text": "Rest the batter for 30 minutes.", "timer_s": 1800},
    {"text": "Fry 2-3 minutes per side."}
  ],
  "source": {"type": "pdf", "name": "pancakes.pdf"},
  "tags": ["breakfast", " quick "], "created": "2026-11-12T09:00:00", "cooked": 1765000000,
  "future_field": {"anything": true}
})";

TEST(recipe_parse_and_round_trip)
{
    Recipe r;
    std::string err;
    CHECK(parse_recipe(kPancakes, r, &err));
    CHECK(r.title == "Pancakes" && r.servings == 4 && r.ingredients.size() == 4 && r.steps.size() == 3);
    CHECK(r.ingredients[0].has_qty && r.ingredients[0].qty == 1.5 && r.ingredients[0].note == "sifted");
    CHECK(!r.ingredients[3].has_qty && r.steps[1].timer_s == 1800);
    CHECK(r.source.type == "pdf" && r.tags.size() == 2 && r.tags[1] == "quick" && r.cooked == 1765000000);
    CHECK(has_card(r));
    Recipe again;
    CHECK(parse_recipe(recipe_to_json(r), again, &err));
    CHECK(recipe_to_json(again) == recipe_to_json(r));
    Recipe minimal;
    CHECK(parse_recipe(R"({"format":1,"title":"Scan only"})", minimal, &err) && !has_card(minimal));
}

TEST(recipe_validation_errors)
{
    Recipe r;
    r.title = "keep";
    std::string err;
    struct Case {
        const char *json, *error;
    };
    const Case cases[] = {
        {"not json", "recipe.json is not a JSON object"},
        {R"({"title":"x"})", "missing or invalid format"},
        {R"({"format":2,"title":"x"})", "recipe is in a newer format than this firmware reads"},
        {R"({"format":1,"title":"  "})", "missing title"},
        {R"({"format":1,"title":"x","servings":-1})", "invalid servings"},
        {R"({"format":1,"title":"x","ingredients":[{"qty":1}]})", "invalid ingredient"},
        {R"({"format":1,"title":"x","ingredients":[{"qty":"two","item":"eggs"}]})", "invalid ingredient quantity"},
        {R"({"format":1,"title":"x","steps":[{"text":""}]})", "invalid step"},
        {R"({"format":1,"title":"x","steps":[{"text":"a","timer_s":-5}]})", "invalid step timer"},
        {R"({"format":1,"title":"x","tags":"breakfast"})", "invalid tags"},
    };
    for (const Case &c : cases) {
        CHECK(!parse_recipe(c.json, r, &err));
        CHECK(err == c.error);
    }
    CHECK(r.title == "keep");  // untouched on failure
}

TEST(fractions_and_metric_formatting)
{
    struct Case {
        double qty;
        const char *text;
    };
    const Case cases[] = {{1.5, "1 1/2"}, {0.75, "3/4"}, {2, "2"}, {0.333, "1/3"}, {2.6667, "2 2/3"}, {0.125, "1/8"},
                          {1.99, "2"},    {0.1, "0.1"},  {3.4, "3.4"}, {0.02, "0.02"}, {10.5, "10 1/2"}};
    for (const Case &c : cases) CHECK(format_fraction(c.qty) == c.text);
    CHECK(format_qty(187.5, "g") == "188" && format_qty(1.25, "kg") == "1.25" && format_qty(12.5, "g") == "12.5" && format_qty(0.5, "l") == "0.5");
    CHECK(format_qty(1.5, "cup") == "1 1/2" && format_qty(1.5, "") == "1 1/2");
}

TEST(servings_scaling)
{
    Recipe r;
    std::string err;
    parse_recipe(kPancakes, r, &err);
    const double f = scale_factor(r.servings, 6);  // 4 -> 6 servings
    CHECK(format_ingredient(r.ingredients[0], f) == "2 1/4 cup flour, sifted");
    CHECK(format_ingredient(r.ingredients[1], f) == "3 eggs");
    CHECK(format_ingredient(r.ingredients[2], f) == "375 ml milk");
    CHECK(format_ingredient(r.ingredients[3], f) == "salt, to taste");  // no number: never scaled
    CHECK(format_ingredient(r.ingredients[0]) == "1 1/2 cup flour, sifted");
    CHECK(format_ingredient(r.ingredients[0], scale_factor(r.servings, 2)) == "3/4 cup flour, sifted");
    CHECK(scale_factor(0, 6) == 1.0 && scale_factor(4, 0) == 1.0);
}

TEST(step_time_detection)
{
    struct Case {
        const char *text;
        int seconds;
    };
    const Case cases[] = {
        {"Bake for 60 minutes.", 3600},
        {"bake 25\xE2\x80\x93" "30 minutes until golden", 1500},  // en dash range -> lower bound
        {"Simmer 10 to 12 min, stirring.", 600},
        {"Roast for 1 hour 30 minutes.", 5400},
        {"Cook 1 hour and 15 minutes", 4500},
        {"Rest 1 1/2 hours", 5400},
        {"Chill for 1/2 hour", 1800},
        {"Microwave 45 sec", 45},
        {"Proof 2-3 hrs", 7200},
        {"After a 30-minute rest, shape.", 1800},
        {"Boil 2.5 minutes", 150},
        {"Heat oven to 350 degrees and add 3 eggs", 0},
        {"Use a 9x13 pan, serves 4", 0},
        {"Add 5 ml vanilla and mix 2 cups flour", 0},
        {"Step 2: stir.", 0},
    };
    for (const Case &c : cases) CHECK(detect_timer_s(c.text) == c.seconds);

    std::string text = "Sear 2 minutes, then bake 25-30 minutes.";
    auto spans = find_times(text);
    CHECK(spans.size() == 2);
    CHECK(text.substr(spans[0].start, spans[0].length) == "2 minutes" && spans[0].seconds == 120);
    CHECK(text.substr(spans[1].start, spans[1].length) == "25-30 minutes" && spans[1].seconds == 1500);
    CHECK(format_duration(1500) == "25 min" && format_duration(5400) == "1 h 30 min" && format_duration(45) == "45 s" &&
          format_duration(90) == "1 min 30 s" && format_duration(7200) == "2 h");
}

TEST(recipe_ids)
{
    CHECK(make_recipe_id("Mum's Apple Pie!") == "mums-apple-pie");
    CHECK(make_recipe_id("  Cr\xC3\xA8me br\xC3\xBBl\xC3\xA9" "e  ") == "crme-brle");
    CHECK(make_recipe_id("!!!") == "recipe" && make_recipe_id("Inbox") == "recipe");
    CHECK(make_recipe_id(std::string(100, 'a')).size() == 48);
    CHECK(valid_recipe_id("mums-apple-pie") && valid_recipe_id("a1"));
    for (const char *bad : {"", "Inbox", "inbox", "a b", "../x", "-a", "a-", "a/b"}) CHECK(!valid_recipe_id(bad));
}

TEST(index_rebuild_from_folders)
{
    std::vector<RecipeFolder> folders = {
        {"pancakes", kPancakes, 2, true},
        {"apple-pie", R"({"format":1,"title":"Apple pie","tags":["dessert"]})", 3, false},
        {"zucchini", R"({"format":1,"title":"Zucchini soup","cooked":1766000000,"ingredients":[{"item":"zucchini"}],"steps":[{"text":"Cook."}]})", 0, false},
        {"broken", "{oops", 1, false},
        {"Bad Name", R"({"format":1,"title":"x"})", 0, false},
        {"future", R"({"format":9,"title":"x"})", 0, false},
    };
    std::vector<std::string> skipped;
    auto idx = build_index(folders, &skipped);
    CHECK(idx.size() == 3 && skipped.size() == 3);
    CHECK(idx[0].id == "zucchini" && idx[1].id == "pancakes" && idx[2].id == "apple-pie");  // recently cooked first
    CHECK(idx[1].has_card && idx[1].has_pages && idx[1].pages == 2 && idx[1].has_photo);
    CHECK(!idx[2].has_card && idx[2].has_pages && !idx[0].has_pages);
    CHECK(skipped[0] == "broken: recipe.json is not a JSON object");

    std::vector<IndexEntry> back;
    CHECK(parse_index(index_to_json(idx), back) && back.size() == 3);
    CHECK(index_to_json(back) == index_to_json(idx));
    // Missing or corrupt index -> caller rebuilds.
    back.clear();
    for (const char *bad : {"", "{", R"({"format":2,"recipes":[]})", R"({"format":1,"recipes":[{"id":"../x","title":"t","has_card":true,"has_pages":false}]})",
                            R"({"format":1,"recipes":[{"id":"ok"}]})"})
        CHECK(!parse_index(bad, back) && back.empty());
}

int main()
{
    RUN(recipe_parse_and_round_trip);
    RUN(recipe_validation_errors);
    RUN(fractions_and_metric_formatting);
    RUN(servings_scaling);
    RUN(step_time_detection);
    RUN(recipe_ids);
    RUN(index_rebuild_from_folders);
    return report("recipes");
}
