// The device's web UI API (config portal): status, Wi-Fi, firmware, settings
// export/import, diagnostics. Transport-agnostic and pure: the device maps
// esp_http_server requests onto it (esp/http_server.cpp), the PC simulator its
// own small server, and host tests call it directly.
//
// Firmware changes go through the one update engine (hard rule 5): catalog
// installs as "update/command" events, uploads via UpdateControl.
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "summit/event_bus.h"
#include "summit/modules.h"
#include "summit/settings.h"
#include "summit/state_store.h"

namespace summit::web {

struct Request {
    Request() = default;
    Request(std::string m, std::string p, std::string ct = {}, std::string b = {}, std::string q = {})
        : method(std::move(m)), path(std::move(p)), content_type(std::move(ct)), body(std::move(b)), query(std::move(q)) {}

    std::string method;        // "GET" | "POST"
    std::string path;          // without query string
    std::string content_type;  // request Content-Type
    std::string body;
    std::string query;         // after '?', e.g. "secrets=1"
};

struct Response {
    int status = 200;
    std::string content_type = "application/json";
    std::string body;
    std::string filename;  // set -> Content-Disposition: attachment
    // Set -> the transport streams this file from the SD card as the body (large files).
    std::string storage_path;
};

// A streamed upload body (anything but firmware, which has its own path).
class UploadSink {
public:
    virtual ~UploadSink() = default;
    virtual bool chunk(const uint8_t *data, size_t len) = 0;
    virtual Response finish() = 0;
};

// Extra endpoints provided by a module (e.g. the Recipe reader's /api/recipes/...).
class ApiExtension {
public:
    virtual ~ApiExtension() = default;
    // True if this request is the extension's (out is then the response).
    virtual bool handle(const Request &req, Response &out) = 0;
    // A sink for an upload to req.path, or null with `error` set (404 if not its path).
    virtual std::unique_ptr<UploadSink> upload(const Request &req, int64_t length, Response &error) = 0;
};

// "a=1&name=x%20y" -> value of `key` (URL-decoded), "" if absent.
std::string query_param(const std::string &query, const std::string &key);
Response json_error(int status, const std::string &message);

// Streaming firmware upload into the update engine (device: summit::update::upload_*).
class UpdateControl {
public:
    virtual ~UpdateControl() = default;
    virtual bool upload_begin(int64_t size, std::string *error) = 0;
    virtual bool upload_data(const uint8_t *data, size_t len) = 0;
    virtual bool upload_complete(std::string *error) = 0;
    virtual void upload_abort(const char *reason) = 0;
};

// Wi-Fi credentials live outside settings, so a settings export never contains them
// (hard rule 6). The password is write-only.
class WifiStore {
public:
    virtual ~WifiStore() = default;
    virtual std::string ssid() = 0;
    // Only used for an export with secrets ticked; never returned by any GET.
    virtual std::string password() = 0;
    virtual bool save(const std::string &ssid, const std::string &password, std::string *error) = 0;
};

// Device facts for the diagnostics page (uptime, heap, reset reason, partitions...).
class DeviceFacts {
public:
    virtual ~DeviceFacts() = default;
    virtual std::vector<std::pair<std::string, std::string>> facts() = 0;
};

class WebApi {
public:
    WebApi(EventBus &bus, StateStore &state, Settings &settings, UpdateControl &update, WifiStore &wifi,
           DeviceFacts &facts)
        : bus_(bus), state_(state), settings_(settings), update_(update), wifi_(wifi), facts_(facts) {}

    void add_extension(ApiExtension *ext) { extensions_.push_back(ext); }
    // Uploads other than firmware: POST with the X-Summit-Upload header, body streamed.
    std::unique_ptr<UploadSink> open_upload(const Request &req, int64_t length, Response &error);

    // Optional: enables /api/modules.
    void set_modules(const ModuleManager *modules) { modules_ = modules; }

    // Everything except the firmware upload body.
    Response handle(const Request &req);

    // POST /api/firmware/upload: raw .bin body, streamed. Call begin with the
    // Content-Length, then chunks, then finish. Requires the X-Summit-Upload header
    // (checked by the transport) so other web pages can't post to it.
    bool upload_begin(int64_t length, Response *error);
    bool upload_chunk(const uint8_t *data, size_t len);
    Response upload_finish();
    void upload_abort(const char *reason) { update_.upload_abort(reason); }
    // Response for an upload without the X-Summit-Upload: 1 header.
    static Response upload_refused();

    static constexpr int64_t kMaxJsonBody = 32 * 1024;

private:
    Response status();
    Response wifi_get();
    Response wifi_post(const std::string &body);
    Response firmware_get();
    Response firmware_command(const std::string &cmd, const std::string &body);
    Response settings_get();
    Response settings_post(const std::string &body);
    Response settings_export(bool include_secrets);
    Response settings_import(const std::string &body, bool dry_run);
    Response settings_undo();
    Response settings_reset(const std::string &body);
    Response settings_backup();
    Response diagnostics();
    Response modules_get();
    Response modules_command(const std::string &cmd, const std::string &body);

    std::string str(const char *key) const;

    EventBus &bus_;
    StateStore &state_;
    Settings &settings_;
    UpdateControl &update_;
    WifiStore &wifi_;
    DeviceFacts &facts_;
    const ModuleManager *modules_ = nullptr;
    std::vector<ApiExtension *> extensions_;
};

// Settings file format for export/import (pure; tested on the host).
// Secret settings are left out unless include_secrets. Wi-Fi: SSID always, password
// only with secrets.
struct WifiExport {
    bool present = false;
    std::string ssid, password;
    bool has_password = false;
};
std::string snapshot_to_json(const SettingsSnapshot &snap, const SettingsSchema &schema, const std::string &firmware,
                             bool include_secrets, const WifiExport &wifi);
bool snapshot_from_json(const std::string &json, const SettingsSchema &schema, SettingsSnapshot &out,
                        WifiExport &wifi, std::string *error);

}  // namespace summit::web
