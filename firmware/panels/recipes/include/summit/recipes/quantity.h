// Ingredient quantities: scaling by servings and kitchen-friendly formatting. Pure logic.
#pragma once
#include <string>

#include "summit/recipes/recipe.h"

namespace summit::recipes {

// "1 1/2", "3/4", "2", "0.1". Snaps to halves, thirds, quarters and eighths when
// within 0.02; otherwise up to two decimals.
std::string format_fraction(double qty);
// Metric units (g, kg, mg, ml, cl, dl, l) use decimals: whole numbers from 100
// ("187.5 g" -> "188"), one decimal from 10, two below ("1.25 kg"). Everything else
// uses fractions.
std::string format_qty(double qty, const std::string &unit);
// to/from servings; 1.0 if either is unknown (<= 0).
double scale_factor(double recipe_servings, double wanted_servings);
// "1 1/2 cup flour, sifted" scaled by `factor`. Ingredients without a quantity are unchanged.
std::string format_ingredient(const Ingredient &ing, double factor = 1.0);

}  // namespace summit::recipes
