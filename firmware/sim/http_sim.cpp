#include "http_sim.h"

#include <arpa/inet.h>
#include <cstring>
#include <fstream>
#include <netinet/in.h>
#include <poll.h>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>

namespace sim {

using summit::web::Request;
using summit::web::Response;

namespace {

void send_all(int fd, const std::string &s)
{
    size_t off = 0;
    while (off < s.size()) {
        ssize_t n = ::send(fd, s.data() + off, s.size() - off, MSG_NOSIGNAL);
        if (n <= 0) return;
        off += static_cast<size_t>(n);
    }
}

void respond(int fd, const Response &r)
{
    std::ostringstream h;
    h << "HTTP/1.1 " << r.status << " X\r\nContent-Type: " << r.content_type << "\r\nContent-Length: " << r.body.size()
      << "\r\nCache-Control: no-store\r\nConnection: close\r\n";
    if (!r.filename.empty()) h << "Content-Disposition: attachment; filename=\"" << r.filename << "\"\r\n";
    h << "\r\n";
    send_all(fd, h.str() + r.body);
}

std::string lower(std::string s)
{
    for (char &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

}  // namespace

HttpSim::~HttpSim()
{
    if (listen_fd_ >= 0) ::close(listen_fd_);
}

bool HttpSim::listen(int port)
{
    listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(static_cast<uint16_t>(port));
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    return ::bind(listen_fd_, reinterpret_cast<sockaddr *>(&a), sizeof a) == 0 && ::listen(listen_fd_, 8) == 0;
}

void HttpSim::poll()
{
    pollfd p{listen_fd_, POLLIN, 0};
    if (::poll(&p, 1, 0) <= 0) return;
    int fd = ::accept(listen_fd_, nullptr, nullptr);
    if (fd < 0) return;
    timeval tv{5, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    serve(fd);
    ::close(fd);
}

void HttpSim::serve(int fd)
{
    std::string buf;
    char chunk[8192];
    size_t header_end;
    while ((header_end = buf.find("\r\n\r\n")) == std::string::npos) {
        ssize_t n = ::recv(fd, chunk, sizeof chunk, 0);
        if (n <= 0 || buf.size() > 16384) return;
        buf.append(chunk, static_cast<size_t>(n));
    }
    std::istringstream head(buf.substr(0, header_end));
    std::string line, method, target, version;
    std::getline(head, line);
    std::istringstream(line) >> method >> target >> version;
    std::string content_type, upload_hdr;
    int64_t length = 0;
    while (std::getline(head, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t c = line.find(':');
        if (c == std::string::npos) continue;
        std::string k = lower(line.substr(0, c)), v = line.substr(c + 1);
        v.erase(0, v.find_first_not_of(' '));
        if (k == "content-length") length = std::stoll(v);
        if (k == "content-type") content_type = v;
        if (k == "x-summit-upload") upload_hdr = v;
    }
    std::string path = target.substr(0, target.find('?'));
    std::string query = target.find('?') == std::string::npos ? "" : target.substr(target.find('?') + 1);
    std::string body = buf.substr(header_end + 4);

    if (method == "GET" && (path == "/" || path == "/index.html")) {
        std::ifstream f(page_path_, std::ios::binary);
        Response r;
        r.content_type = "text/html; charset=utf-8";
        r.body.assign(std::istreambuf_iterator<char>(f), {});
        respond(fd, r);
        return;
    }
    if (method == "POST" && path == "/api/firmware/upload") {
        if (upload_hdr != "1") {
            respond(fd, summit::web::WebApi::upload_refused());
            return;
        }
        Response err;
        if (!api_.upload_begin(length, &err)) {
            respond(fd, err);
            return;
        }
        int64_t got = 0;
        bool ok = true;
        auto feed = [&](const char *p, size_t n) {
            if (ok) ok = api_.upload_chunk(reinterpret_cast<const uint8_t *>(p), n);
            got += static_cast<int64_t>(n);
        };
        feed(body.data(), body.size());
        while (got < length) {
            ssize_t n = ::recv(fd, chunk, sizeof chunk, 0);
            if (n <= 0) {
                api_.upload_abort("upload connection lost");
                return;
            }
            feed(chunk, static_cast<size_t>(n));
        }
        respond(fd, api_.upload_finish());
        return;
    }
    while (static_cast<int64_t>(body.size()) < length && length <= summit::web::WebApi::kMaxJsonBody + 1) {
        ssize_t n = ::recv(fd, chunk, sizeof chunk, 0);
        if (n <= 0) break;
        body.append(chunk, static_cast<size_t>(n));
    }
    summit::web::Request req;
    req.method = method;
    req.path = path;
    req.query = query;
    req.content_type = content_type;
    req.body = body;
    respond(fd, api_.handle(req));
}

}  // namespace sim
