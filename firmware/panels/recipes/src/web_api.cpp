#include "summit/recipes/web_api.h"

#include <memory>

#include "cJSON.h"
#include "summit/json_writer.h"

namespace summit::recipes {

using web::json_error;
using web::query_param;
using web::Request;
using web::Response;

namespace {

using JsonPtr = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
JsonPtr parse(const std::string &s) { return JsonPtr(cJSON_ParseWithLength(s.data(), s.size()), cJSON_Delete); }

Response ok_json(const std::string &body)
{
    Response r;
    r.body = body;
    return r;
}

bool ends_with(const std::string &s, const char *suffix)
{
    const std::string x = suffix;
    return s.size() >= x.size() && s.compare(s.size() - x.size(), x.size(), x) == 0;
}

// A file name inside /recipes/inbox: no path parts, ends in .pdf (any case).
bool valid_inbox_name(const std::string &name)
{
    if (name.empty() || name.size() > 120 || name[0] == '.') return false;
    for (unsigned char c : name)
        if (c < 0x20 || c == '/' || c == '\\' || c == ':' || c == 0x7F) return false;
    std::string lower = name;
    for (char &c : lower)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return ends_with(lower, ".pdf");
}

class FileSink : public web::UploadSink {
public:
    FileSink(RecipeStore &store, std::unique_ptr<StorageWriter> writer, int64_t length)
        : store_(store), writer_(std::move(writer)), expected_(length) {}
    bool chunk(const uint8_t *data, size_t len) override
    {
        got_ += static_cast<int64_t>(len);
        return got_ <= expected_ && writer_->write(data, len);
    }
    Response finish() override
    {
        if (got_ != expected_) return json_error(400, "upload incomplete");  // writer discarded
        if (!writer_->commit()) return json_error(500, "could not save the file");
        std::vector<IndexEntry> index;
        store_.rebuild_index(index);  // pages / photo flags may have changed
        return ok_json("{\"ok\":true}");
    }

private:
    RecipeStore &store_;
    std::unique_ptr<StorageWriter> writer_;
    int64_t expected_, got_ = 0;
};

}  // namespace

bool RecipeWebApi::valid_file_name(const std::string &name)
{
    if (name == "photo.jpg" || name == "thumb.jpg" || name == "original.pdf") return true;
    if (name.size() != 13 || name.compare(0, 6, "pages/") != 0 || name.compare(9, 4, ".jpg") != 0) return false;
    int n = 0;
    for (size_t i = 6; i < 9; ++i) {
        if (name[i] < '0' || name[i] > '9') return false;
        n = n * 10 + (name[i] - '0');
    }
    return n >= 1 && n <= kMaxPages;
}

bool RecipeWebApi::handle(const Request &req, Response &out)
{
    const std::string &p = req.path;
    if (p.compare(0, 12, "/api/recipes") != 0) return false;
    const bool get = req.method == "GET", post = req.method == "POST";
    if (!store_.available()) {
        out = json_error(409, "Insert a microSD card");
        return true;
    }
    if (p == "/api/recipes" && get) out = list();
    else if (p == "/api/recipes/get" && get) out = this->get(req.query);
    else if (p == "/api/recipes/save" && post) out = save(req.body);
    else if (p == "/api/recipes/file" && get) out = file(req.query);
    else if (p == "/api/recipes/inbox" && get) out = inbox_file(req.query);
    else if (p == "/api/recipes/inbox/done" && post) out = inbox_done(req.body);
    else if (p == "/api/recipes/import-url" && post) out = import_url(req.body);
    else return false;
    return true;
}

Response RecipeWebApi::list()
{
    std::vector<IndexEntry> index;
    std::vector<std::string> skipped;
    store_.load_index(index, &skipped);
    JsonWriter w;
    w.begin_object().key("recipes").begin_array();
    for (const IndexEntry &e : index) {
        w.begin_object().key("id").value(e.id).key("title").value(e.title).key("has_card").value(e.has_card);
        w.key("pages").value(e.pages).key("has_photo").value(e.has_photo).key("tags").begin_array();
        for (const auto &t : e.tags) w.value(t);
        w.end_array().end_object();
    }
    w.end_array().key("inbox").begin_array();
    for (const auto &n : store_.inbox()) w.value(n);
    w.end_array().key("skipped").begin_array();
    for (const auto &s : skipped) w.value(s);
    w.end_array().end_object();
    return ok_json(w.str());
}

Response RecipeWebApi::get(const std::string &query)
{
    const std::string id = query_param(query, "id");
    Recipe r;
    std::string err;
    if (!store_.load(id, r, &err)) return json_error(404, err);
    return ok_json(recipe_to_json(r));
}

Response RecipeWebApi::save(const std::string &body)
{
    JsonPtr j = parse(body);
    const cJSON *recipe = j ? cJSON_GetObjectItemCaseSensitive(j.get(), "recipe") : nullptr;
    const cJSON *idj = j ? cJSON_GetObjectItemCaseSensitive(j.get(), "id") : nullptr;
    if (!cJSON_IsObject(recipe)) return json_error(400, "recipe required");
    char *text = cJSON_PrintUnformatted(recipe);
    const std::string recipe_json = text ? text : "";
    cJSON_free(text);
    Recipe r;
    std::string err;
    if (!parse_recipe(recipe_json, r, &err)) return json_error(400, err);  // same validator as the device
    std::string id;
    if (cJSON_IsString(idj) && idj->valuestring[0]) {
        id = idj->valuestring;  // editing an existing recipe
        if (!valid_recipe_id(id) || !storage_.exists(RecipeStore::folder(id))) return json_error(404, "recipe not found");
    } else {
        id = store_.new_id(r.title);
    }
    if (!store_.save(id, r)) return json_error(500, "could not save the recipe");
    JsonWriter w;
    w.begin_object().key("ok").value(true).key("id").value(id).end_object();
    return ok_json(w.str());
}

Response RecipeWebApi::file(const std::string &query)
{
    const std::string id = query_param(query, "id"), name = query_param(query, "name");
    if (!valid_recipe_id(id) || !valid_file_name(name)) return json_error(400, "invalid file");
    const std::string path = RecipeStore::folder(id) + "/" + name;
    if (storage_.size(path) < 0) return json_error(404, "not found");
    Response r;
    r.content_type = ends_with(name, ".pdf") ? "application/pdf" : "image/jpeg";
    r.storage_path = path;
    return r;
}

Response RecipeWebApi::inbox_file(const std::string &query)
{
    const std::string name = query_param(query, "name");
    if (!valid_inbox_name(name)) return json_error(400, "invalid name");
    const std::string path = "/recipes/inbox/" + name;
    if (storage_.size(path) < 0) return json_error(404, "not found");
    Response r;
    r.content_type = "application/pdf";
    r.storage_path = path;
    return r;
}

Response RecipeWebApi::inbox_done(const std::string &body)
{
    JsonPtr j = parse(body);
    const cJSON *name = j ? cJSON_GetObjectItemCaseSensitive(j.get(), "name") : nullptr;
    const cJSON *id = j ? cJSON_GetObjectItemCaseSensitive(j.get(), "id") : nullptr;
    if (!cJSON_IsString(name) || !cJSON_IsString(id) || !valid_inbox_name(name->valuestring) ||
        !valid_recipe_id(id->valuestring))
        return json_error(400, "name and id required");
    const std::string folder = RecipeStore::folder(id->valuestring);
    if (!storage_.exists(folder + "/recipe.json")) return json_error(404, "recipe not found");
    // The PDF is kept: it moves next to its recipe so it can be re-converted later.
    if (!storage_.move(std::string("/recipes/inbox/") + name->valuestring, folder + "/original.pdf"))
        return json_error(409, "could not move the PDF (already converted?)");
    return ok_json("{\"ok\":true}");
}

Response RecipeWebApi::import_url(const std::string &body)
{
    if (!fetcher_) return json_error(501, "importing from a website is not available on this device");
    JsonPtr j = parse(body);
    const cJSON *u = j ? cJSON_GetObjectItemCaseSensitive(j.get(), "url") : nullptr;
    if (!cJSON_IsString(u) || !valid_import_url(u->valuestring))
        return json_error(400, "enter the full address of a recipe page, starting with https://");
    const std::string url = u->valuestring;
    std::string page, err;
    if (!fetcher_->get(url, kMaxPageBytes, page, &err)) return json_error(502, "could not fetch the page: " + err);
    WebRecipe w;
    if (!extract_recipe_jsonld(page, url, w, &err)) return json_error(422, err);
    const std::string id = store_.new_id(w.recipe.title);
    if (!store_.save(id, w.recipe)) return json_error(500, "could not save the recipe");
    // The main photo, if the page names one and it really is a JPEG. Optional.
    bool photo = false;
    std::string image;
    if (!w.photo_url.empty() && fetcher_->get(w.photo_url, static_cast<size_t>(kMaxImageBytes), image, &err) &&
        image.size() > 3 && static_cast<unsigned char>(image[0]) == 0xFF && static_cast<unsigned char>(image[1]) == 0xD8) {
        photo = storage_.write(RecipeStore::photo_path(id), image);
        std::vector<IndexEntry> index;
        if (photo) store_.rebuild_index(index);
    }
    JsonWriter w2;
    w2.begin_object().key("ok").value(true).key("id").value(id).key("title").value(w.recipe.title);
    w2.key("ingredients").value(static_cast<int64_t>(w.recipe.ingredients.size()));
    w2.key("steps").value(static_cast<int64_t>(w.recipe.steps.size())).key("photo").value(photo).end_object();
    return ok_json(w2.str());
}

std::unique_ptr<web::UploadSink> RecipeWebApi::upload(const Request &req, int64_t length, Response &error)
{
    if (req.path != "/api/recipes/file") return nullptr;  // not ours: error stays 404
    if (!store_.available()) {
        error = json_error(409, "Insert a microSD card");
        return nullptr;
    }
    const std::string id = query_param(req.query, "id"), name = query_param(req.query, "name");
    if (!valid_recipe_id(id) || !valid_file_name(name)) {
        error = json_error(400, "invalid file");
        return nullptr;
    }
    if (!storage_.exists(RecipeStore::folder(id) + "/recipe.json")) {
        error = json_error(404, "recipe not found");
        return nullptr;
    }
    if (length > (ends_with(name, ".pdf") ? kMaxPdfBytes : kMaxImageBytes)) {
        error = json_error(413, "file too large");
        return nullptr;
    }
    if (storage_.free_mb() < length / (1024 * 1024) + 8) {
        error = json_error(507, "not enough space on the microSD card");
        return nullptr;
    }
    std::unique_ptr<StorageWriter> writer = storage_.begin_write(RecipeStore::folder(id) + "/" + name);
    if (!writer) {
        error = json_error(500, "could not open the file");
        return nullptr;
    }
    return std::make_unique<FileSink>(store_, std::move(writer), length);
}

}  // namespace summit::recipes
