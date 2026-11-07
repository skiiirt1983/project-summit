#include "catalog_json.h"

#include "cJSON.h"

namespace summit::update {

namespace {

std::string str(const cJSON *o, const char *key)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    return cJSON_IsString(v) && v->valuestring ? v->valuestring : "";
}

int64_t num(const cJSON *o, const char *key)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    if (!cJSON_IsNumber(v) || v->valuedouble < 0 || v->valuedouble > 1e15) return -1;
    return static_cast<int64_t>(v->valuedouble);
}

}  // namespace

bool parse_catalog_json(const std::string &json, Catalog &out, std::string *error)
{
    cJSON *root = cJSON_ParseWithLength(json.data(), json.size());
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        if (error) *error = "catalog is not valid JSON";
        return false;
    }
    Catalog c;
    c.format = num(root, "format");
    c.project = str(root, "project");
    const cJSON *builds = cJSON_GetObjectItemCaseSensitive(root, "builds");
    const cJSON *b = nullptr;
    if (cJSON_IsArray(builds)) {
        cJSON_ArrayForEach(b, builds)
        {
            if (!cJSON_IsObject(b)) continue;
            CatalogBuild x;
            x.version = str(b, "version");
            x.channel = str(b, "channel");
            x.hardware_id = str(b, "hardware_id");
            x.partition_layout = num(b, "partition_layout");
            x.url = str(b, "url");
            x.size = num(b, "size");
            x.sha256 = str(b, "sha256");
            x.released = str(b, "released");
            x.notes = str(b, "notes");
            c.builds.push_back(x);  // validated by evaluate()
        }
    }
    cJSON_Delete(root);
    out = std::move(c);
    return true;
}

}  // namespace summit::update
