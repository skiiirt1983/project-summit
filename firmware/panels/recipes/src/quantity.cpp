#include "summit/recipes/quantity.h"

#include <cmath>
#include <cstdio>

namespace summit::recipes {

namespace {

std::string decimal(double v, int places)
{
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.*f", places, v);
    std::string s = buf;
    if (s.find('.') != std::string::npos) {
        while (s.back() == '0') s.pop_back();
        if (s.back() == '.') s.pop_back();
    }
    return s;
}

bool metric(const std::string &unit)
{
    for (const char *u : {"g", "kg", "mg", "ml", "cl", "dl", "l"})
        if (unit == u) return true;
    return false;
}

}  // namespace

std::string format_fraction(double qty)
{
    if (!(qty > 0) || !std::isfinite(qty)) return "0";
    struct F {
        double v;
        const char *text;
    };
    static const F fracs[] = {{0.0, ""},      {0.125, "1/8"}, {0.25, "1/4"}, {1.0 / 3, "1/3"}, {0.375, "3/8"},
                              {0.5, "1/2"},   {0.625, "5/8"}, {2.0 / 3, "2/3"}, {0.75, "3/4"}, {0.875, "7/8"},
                              {1.0, ""}};
    double whole = std::floor(qty);
    const double rest = qty - whole;
    const F *best = nullptr;
    double best_d = 0.02;
    for (const F &f : fracs) {
        double d = std::fabs(rest - f.v);
        if (d <= best_d) {
            best_d = d;
            best = &f;
        }
    }
    if (!best) return decimal(qty, 2);
    if (best->v == 1.0) whole += 1;
    const std::string frac = best->text;
    if (whole == 0 && frac.empty()) return decimal(qty, 2);  // tiny amount: don't show "0"
    if (frac.empty()) return decimal(whole, 0);
    if (whole == 0) return frac;
    return decimal(whole, 0) + " " + frac;
}

std::string format_qty(double qty, const std::string &unit)
{
    if (!metric(unit)) return format_fraction(qty);
    if (qty >= 100) return decimal(std::round(qty), 0);
    if (qty >= 10) return decimal(qty, 1);
    return decimal(qty, 2);
}

double scale_factor(double recipe_servings, double wanted_servings)
{
    if (!(recipe_servings > 0) || !(wanted_servings > 0)) return 1.0;
    return wanted_servings / recipe_servings;
}

std::string format_ingredient(const Ingredient &ing, double factor)
{
    std::string s;
    if (ing.has_qty) {
        s = format_qty(ing.qty * factor, ing.unit);
        if (!ing.unit.empty()) s += " " + ing.unit;
        s += " ";
    }
    s += ing.item;
    if (!ing.note.empty()) s += ", " + ing.note;
    return s;
}

}  // namespace summit::recipes
