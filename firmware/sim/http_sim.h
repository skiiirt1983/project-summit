// Minimal HTTP/1.1 server for the simulator's --web mode: serves the device's
// web page and WebApi on localhost, one request per connection. Not for the device.
#pragma once
#include <string>

#include "summit/storage.h"
#include "summit/web/api.h"

namespace sim {

class HttpSim {
public:
    // web_root: folder with index.html and its assets. storage: for responses that stream a card file.
    HttpSim(summit::web::WebApi &api, std::string web_root, summit::Storage *storage = nullptr)
        : api_(api), page_path_(std::move(web_root)), storage_(storage) {}
    ~HttpSim();
    bool listen(int port);
    // Handles at most one pending connection; returns immediately if none.
    void poll();

private:
    void serve(int fd);
    summit::web::WebApi &api_;
    std::string page_path_;
    summit::Storage *storage_;
    int listen_fd_ = -1;
};

}  // namespace sim
