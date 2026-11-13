// Recipe endpoints for the web UI (import in the browser, D-008):
//   GET  /api/recipes                      library index + inbox PDF names
//   GET  /api/recipes/get?id=              recipe.json
//   POST /api/recipes/save                 {"id"?: ..., "recipe": {...}} -> {"ok", "id"}
//   POST /api/recipes/file?id=&name=       upload pages/NNN.jpg, photo.jpg, thumb.jpg, original.pdf
//   GET  /api/recipes/file?id=&name=       those files back (thumbnails, pages)
//   GET  /api/recipes/inbox?name=          a PDF waiting in /recipes/inbox
//   POST /api/recipes/inbox/done           {"name", "id"}: move the PDF into the recipe as original.pdf
//   POST /api/recipes/import-url           {"url"}: the device fetches the page and saves its schema.org Recipe
// Nothing here deletes a recipe. Pure logic over RecipeStore / Storage; host-tested.
#pragma once
#include "summit/recipes/jsonld.h"
#include "summit/recipes/store.h"
#include "summit/storage.h"
#include "summit/web/api.h"

namespace summit::recipes {

class RecipeWebApi : public web::ApiExtension {
public:
    static constexpr int64_t kMaxImageBytes = 2 * 1024 * 1024;
    static constexpr int64_t kMaxPdfBytes = 32 * 1024 * 1024;
    static constexpr int kMaxPages = 200;

    RecipeWebApi(RecipeStore &store, Storage &storage) : store_(store), storage_(storage) {}
    // Enables import-url. The fetcher does the network I/O (device: HTTPS client).
    void set_fetcher(PageFetcher *fetcher) { fetcher_ = fetcher; }
    static constexpr size_t kMaxPageBytes = 2 * 1024 * 1024;

    bool handle(const web::Request &req, web::Response &out) override;
    std::unique_ptr<web::UploadSink> upload(const web::Request &req, int64_t length, web::Response &error) override;

    // "pages/001.jpg".."pages/200.jpg", "photo.jpg", "thumb.jpg", "original.pdf".
    static bool valid_file_name(const std::string &name);

private:
    web::Response list();
    web::Response get(const std::string &query);
    web::Response save(const std::string &body);
    web::Response file(const std::string &query);
    web::Response inbox_file(const std::string &query);
    web::Response inbox_done(const std::string &body);
    web::Response import_url(const std::string &body);

    RecipeStore &store_;
    Storage &storage_;
    PageFetcher *fetcher_ = nullptr;
};

}  // namespace summit::recipes
