#pragma once
#include "../../include/routing/Controller.h"
#include <string>
#include <unordered_map>

class StaticFileController : public routing::Controller {
public:
    // Handles all static file requests
    void serveStatic(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);

private:
    void setCSPHeaders(uWS::HttpResponse<false>* res, const std::string& mimeType, std::string& content);
    std::string getMimeType(const std::string& path);
    bool shouldCache(const std::string& path);
    std::string readFile(const std::string& path);
    std::string generateETag(const std::string& content);
    std::string getLastModifiedHeader(const std::string& filePath);
};

// Route registration - serve static files from /assets/* prefix
ROUTE_CONTROLLER(StaticFileController) {
    using namespace routing;
    REGISTER_ROUTE(HttpMethod::GET, "/assets/*", serveStatic, StaticFileController);
} 