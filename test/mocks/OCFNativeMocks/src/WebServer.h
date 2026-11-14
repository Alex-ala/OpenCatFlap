#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

#include <Arduino.h>
#include <FS.h>

enum HTTPMethod
{
    HTTP_ANY,
    HTTP_GET,
    HTTP_HEAD,
    HTTP_POST,
    HTTP_PUT,
    HTTP_PATCH,
    HTTP_DELETE,
    HTTP_OPTIONS
};

enum HTTPUploadStatus
{
    UPLOAD_FILE_START,
    UPLOAD_FILE_WRITE,
    UPLOAD_FILE_END,
    UPLOAD_FILE_ABORTED
};

#define HTTP_UPLOAD_BUFLEN 1436

struct HTTPUpload
{
    HTTPUploadStatus status;
    String filename;
    String name;
    String type;
    size_t totalSize;
    size_t currentSize;
    uint8_t buf[HTTP_UPLOAD_BUFLEN];
};

namespace mock
{
    struct HttpResponse
    {
        int code;
        std::string content_type;
        std::string body;
    };
    struct Route
    {
        std::string uri;
        HTTPMethod method;
        std::function<void(void)> handler;
        std::function<void(void)> upload_handler;
    };
    struct WebState
    {
        std::map<std::string, std::string> args;
        std::vector<HttpResponse> responses;
        std::vector<Route> routes;
        std::map<std::string, std::string> static_routes;
        bool begun = false;
        HTTPUpload upload{};
    };
    inline WebState web;

    inline void reset_web_request()
    {
        web.args.clear();
        web.responses.clear();
        web.upload = HTTPUpload{};
    }
    inline void reset_web()
    {
        web = WebState();
    }
    inline bool dispatch(const std::string &uri, HTTPMethod method)
    {
        for (const auto &r : web.routes)
            if (r.uri == uri && (r.method == HTTP_ANY || r.method == method))
            {
                r.handler();
                return true;
            }
        return false;
    }
    inline bool dispatch_upload(const std::string &uri, HTTPMethod method, HTTPUploadStatus status, const uint8_t *data = nullptr, size_t len = 0)
    {
        for (const auto &r : web.routes)
            if (r.uri == uri && (r.method == HTTP_ANY || r.method == method) && r.upload_handler)
            {
                web.upload.status = status;
                web.upload.currentSize = len;
                if (len) memcpy(web.upload.buf, data, len);
                if (status == UPLOAD_FILE_WRITE) web.upload.totalSize += len;
                r.upload_handler();
                return true;
            }
        return false;
    }
}

class WebServer
{
public:
    typedef std::function<void(void)> THandlerFunction;

    explicit WebServer(int = 80) {}
    void on(const char *uri, THandlerFunction handler) { on(uri, HTTP_ANY, handler); }
    void on(const char *uri, HTTPMethod method, THandlerFunction handler)
    {
        mock::web.routes.push_back({uri, method, handler, nullptr});
    }
    void on(const char *uri, HTTPMethod method, THandlerFunction handler, THandlerFunction upload_handler)
    {
        mock::web.routes.push_back({uri, method, handler, upload_handler});
    }
    HTTPUpload &upload() { return mock::web.upload; }
    void serveStatic(const char *uri, fs::FS &, const char *path, const char * = nullptr)
    {
        mock::web.static_routes[uri] = path;
    }
    void begin() { mock::web.begun = true; }
    void handleClient() {}

    bool hasArg(const String &name) const { return mock::web.args.count(name.c_str()) > 0; }
    String arg(const String &name) const
    {
        auto it = mock::web.args.find(name.c_str());
        return it == mock::web.args.end() ? String() : String(it->second.c_str());
    }
    void send(int code, const char *content_type, const String &content)
    {
        mock::web.responses.push_back({code, content_type ? content_type : "", content.c_str()});
    }
    void send(int code, const char *content_type, const char *content)
    {
        send(code, content_type, String(content));
    }
};
