#include "../include/StaticFileHandler.h"
#include "../include/utils.h"
#include <iostream>
//#include "../include/WebServer.h"

constexpr std::string_view INDEX_FILE = "index.html";

StaticFileHandler::StaticFileHandler(const fs::path& path) : basePath(path) {
    try {
        indexHtml = utils::loadFile(basePath / INDEX_FILE);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Failed to load index.html: " << e.what() << std::endl;
    }
}

void StaticFileHandler::handleRequest(auto* res, auto* req) {
    std::string_view path = req->getUrl();
    
    // Serve index.html for root path
    if (path == "/" || path == "/index.html") {
        res->writeHeader("Content-Type", "text/html; charset=utf-8");
        res->writeHeader("Server", "HatefEngine 1.0")->end(indexHtml);
        return;
    }
    
    // Handle other static files
    std::string content;
    std::string mimeType;
    
    if (utils::loadStaticFile(basePath, path, content, mimeType)) {
        res->writeHeader("Content-Type", mimeType);
        res->writeHeader("Server", "HatefEngine 1.0")->end(content);
    } else {
        res->writeStatus("404 Not Found");
        res->writeHeader("Content-Type", "text/plain");
        res->writeHeader("Server", "HatefEngine 1.0")->end("File not found");
    }
} 