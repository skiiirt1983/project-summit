// Recipe import from a web page (D-008): reads the schema.org `Recipe` JSON-LD block
// most recipe sites embed. Pure logic, host-tested on saved sample pages. The device
// fetches the page (PageFetcher); nothing here touches the network.
#pragma once
#include <string>

#include "summit/recipes/recipe.h"

namespace summit::recipes {

// "1 1/2 cups flour, sifted" -> {1.5, "cup", "flour", "sifted"} (same rules as the web
// import's recipe_import.mjs: fractions, "½", unit names normalised, lower end of a range).
Ingredient parse_ingredient_line(const std::string &line);

// Tags removed, common entities decoded, whitespace collapsed.
std::string html_to_text(const std::string &html);

struct WebRecipe {
    Recipe recipe;          // source = {type: "url", url}
    std::string photo_url;  // main photo if the page names one (https only), else ""
};

// Finds a Recipe in the page's <script type="application/ld+json"> blocks (also inside
// @graph and arrays). False with a readable error if there is none or it has no name.
bool extract_recipe_jsonld(const std::string &html, const std::string &page_url, WebRecipe &out, std::string *error);

// Only public https pages: "https://host.tld/path". No IP addresses, localhost or
// single-label / .local names (the device must not be used to probe the home network).
bool valid_import_url(const std::string &url);

// Fetches a URL (device: HTTPS client; simulator and tests: fixtures).
class PageFetcher {
public:
    virtual ~PageFetcher() = default;
    virtual bool get(const std::string &url, size_t max_bytes, std::string &body, std::string *error) = 0;
};

}  // namespace summit::recipes
