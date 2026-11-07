#pragma once
#include <string>

#include "summit/update/catalog.h"

namespace summit::update {
// Parses catalog.json (format 1). Unknown fields are ignored; wrong types fail the build entry.
bool parse_catalog_json(const std::string &json, Catalog &out, std::string *error);
}  // namespace summit::update
