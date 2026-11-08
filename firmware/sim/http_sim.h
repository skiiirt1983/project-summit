// Minimal HTTP/1.1 server for the simulator's --web mode: serves the device's
// web page and WebApi on localhost, one request per connection. Not for the device.
#pragma once
#include <string>

#include "summit/web/api.h"

namespace sim {

class HttpSim {
public:
    HttpSim(summit::web::WebApi &api, std::string page_path) : api_(api), page_path_(std::move(page_path)) {}
    ~HttpSim();
    bool listen(int port);
    // Handles at most one pending connection; returns immediately if none.
    void poll();

private:
    void serve(int fd);
    summit::web::WebApi &api_;
    std::string page_path_;
    int listen_fd_ = -1;
};

}  // namespace sim
