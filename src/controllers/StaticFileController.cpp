#include "StaticFileController.h"
#include "../../include/Logger.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include "../common/JsMinifierClient.h"
#include "CacheController.h"

// Redis cache for minified JS files
#include <sw/redis++/redis++.h>

class JsMinificationCache {
private:
    std::unique_ptr<sw::redis::Redis> redis_;
    std::mutex cacheMutex_;
    bool redisAvailable_;
    
    // In-memory fallback cache (small files only)
    std::unordered_map<std::string, std::string> memoryCache_;
    std::mutex memoryCacheMutex_;
    static constexpr size_t MAX_MEMORY_CACHE_SIZE = 100; // Max 100 files in memory
    
public:
    JsMinificationCache() : redisAvailable_(false) {
        try {
            // Try to connect to Redis
            const char* redisUri = std::getenv("SEARCH_REDIS_URI");
            std::string connectionString = redisUri ? redisUri : "tcp://127.0.0.1:6379";
            
            sw::redis::ConnectionOptions opts;
            if (connectionString.find("tcp://") == 0) {
                std::string hostPort = connectionString.substr(6);
                size_t colonPos = hostPort.find(':');
                if (colonPos != std::string::npos) {
                    opts.host = hostPort.substr(0, colonPos);
                    opts.port = std::stoi(hostPort.substr(colonPos + 1));
                }
            }
            
            redis_ = std::make_unique<sw::redis::Redis>(opts);
            redis_->ping(); // Test connection
            redisAvailable_ = true;
            LOG_INFO("Redis cache initialized for JS minification");
        } catch (const std::exception& e) {
            LOG_WARNING("Redis cache not available, using memory cache only: " + std::string(e.what()));
            redisAvailable_ = false;
        }
    }
    
    std::string getCachedMinified(const std::string& filePath, const std::string& originalContent) {
        std::string cacheKey = generateCacheKey(filePath, originalContent);
        
        // Try Redis first
        if (redisAvailable_) {
            try {
                auto result = redis_->get(cacheKey);
                if (result) {
                    LOG_DEBUG("JS minification cache HIT (Redis): " + filePath);
                    return *result;
                }
            } catch (const std::exception& e) {
                LOG_WARNING("Redis cache error: " + std::string(e.what()));
            }
        }
        
        // Try memory cache
        {
            std::lock_guard<std::mutex> lock(memoryCacheMutex_);
            auto it = memoryCache_.find(cacheKey);
            if (it != memoryCache_.end()) {
                LOG_DEBUG("JS minification cache HIT (Memory): " + filePath);
                return it->second;
            }
        }
        
        LOG_DEBUG("JS minification cache MISS: " + filePath);
        return "";
    }
    
    void cacheMinified(const std::string& filePath, const std::string& originalContent, 
                      const std::string& minifiedContent) {
        std::string cacheKey = generateCacheKey(filePath, originalContent);
        
        // Cache in Redis (with expiration)
        if (redisAvailable_) {
            try {
                redis_->setex(cacheKey, 3600, minifiedContent); // 1 hour expiration
                LOG_DEBUG("Cached minified JS in Redis: " + filePath);
                return;
            } catch (const std::exception& e) {
                LOG_WARNING("Failed to cache in Redis: " + std::string(e.what()));
            }
        }
        
        // Fallback to memory cache
        {
            std::lock_guard<std::mutex> lock(memoryCacheMutex_);
            
            // Evict oldest entries if cache is full
            if (memoryCache_.size() >= MAX_MEMORY_CACHE_SIZE) {
                memoryCache_.clear(); // Simple eviction strategy
                LOG_DEBUG("Memory cache cleared due to size limit");
            }
            
            memoryCache_[cacheKey] = minifiedContent;
            LOG_DEBUG("Cached minified JS in memory: " + filePath);
        }
    }
    
private:
    std::string generateCacheKey(const std::string& filePath, const std::string& content) {
        // Create a hash of file path + content + minification level
        std::hash<std::string> hasher;
        size_t pathHash = hasher(filePath);
        size_t contentHash = hasher(content);
        size_t levelHash = hasher("advanced"); // Current minification level
        
        return "js_minify:" + std::to_string(pathHash) + ":" + 
               std::to_string(contentHash) + ":" + std::to_string(levelHash);
    }
};

// Global cache instance
static JsMinificationCache jsCache;

void StaticFileController::setCSPHeaders(uWS::HttpResponse<false>* res, const std::string& mimeType, std::string& content) {
    // Set basic security headers for all responses
    res->writeHeader("X-Content-Type-Options", "nosniff");
    res->writeHeader("X-Frame-Options", "DENY");
    res->writeHeader("X-XSS-Protection", "1; mode=block");
    res->writeHeader("Referrer-Policy", "strict-origin-when-cross-origin");
    res->writeHeader("Cross-Origin-Opener-Policy", "same-origin");
    res->writeHeader("Strict-Transport-Security", "max-age=31536000; includeSubDomains; preload");
}

void StaticFileController::serveStatic(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    std::string path = std::string(req->getUrl());
    
    // Remove query parameters if any
    size_t queryPos = path.find('?');
    if (queryPos != std::string::npos) {
        path = path.substr(0, queryPos);
    }

    // Strip /assets prefix to map /assets/* to public/*
    if (path.rfind("/assets/", 0) == 0) {
        path = path.substr(7); // Remove "/assets" (7 characters)
    } else if (path == "/assets") {
        path = "/"; // Map /assets to root of public directory
    }

    // Construct full file path
    std::string filePath = "public" + path;
    
    // Security: prevent directory traversal
    if (path.find("..") != std::string::npos) {
        notFound(res, "Invalid path");
        return;
    }
    
    // Check if file exists
    if (!std::filesystem::exists(filePath) || !std::filesystem::is_regular_file(filePath)) {
        LOG_WARNING("Static file not found: " + path);
        notFound(res, "File not found");
        return;
    }
    
    // Read file content
    std::string content = readFile(filePath);
    if (content.empty() && std::filesystem::file_size(filePath) > 0) {
        serverError(res, "Failed to read file");
        return;
    }
    
    // Determine MIME type
    std::string mimeType = getMimeType(filePath);
    
    // Minify JS if enabled and file is JS
    if (mimeType == "application/javascript") {
        auto start = std::chrono::high_resolution_clock::now();
        
        // Check cache first
        std::string cachedMinified = jsCache.getCachedMinified(filePath, content);
        auto cacheEnd = std::chrono::high_resolution_clock::now();
        auto cacheTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(cacheEnd - start).count();
        
        if (!cachedMinified.empty()) {
            content = cachedMinified;
            CacheController::recordCacheHit(cacheTimeUs);
            LOG_DEBUG("Using cached minified JS: " + path);
        } else {
            // Minify and cache
            static JsMinifierClient client("http://js-minifier:3002");
            if (client.isServiceAvailable()) {
                try {
                    auto minifyStart = std::chrono::high_resolution_clock::now();
                    std::string minified = client.minify(content, "advanced");
                    auto minifyEnd = std::chrono::high_resolution_clock::now();
                    auto minifyTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(minifyEnd - minifyStart).count();
                    
                    jsCache.cacheMinified(filePath, content, minified);
                    content = minified;
                    CacheController::recordCacheMiss(minifyTimeUs);
                    LOG_DEBUG("Minified and cached JS: " + path);
                } catch (const std::exception& e) {
                    LOG_WARNING("JS minification failed, using original content: " + std::string(e.what()));
                }
            }
        }
    }
    
    // Set response headers
    res->writeStatus("200 OK")
       ->writeHeader("Content-Type", mimeType);
    
    // Set basic security headers (no CSP for static files)
    setCSPHeaders(res, mimeType, content);
    
    // Add cache headers for static assets
    if (mimeType == "application/javascript") {
        // JavaScript files: Cache for 1 year with versioning
        res->writeHeader("Cache-Control", "public, max-age=31536000, immutable");
        res->writeHeader("ETag", generateETag(content));
        res->writeHeader("Last-Modified", getLastModifiedHeader(filePath));
    } else if (shouldCache(filePath)) {
        // Other static assets: Cache for 1 year
        res->writeHeader("Cache-Control", "public, max-age=31536000");
        res->writeHeader("ETag", generateETag(content));
        res->writeHeader("Last-Modified", getLastModifiedHeader(filePath));
    } else {
        // Dynamic content: No cache
        res->writeHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        res->writeHeader("Pragma", "no-cache");
        res->writeHeader("Expires", "0");
    }
    
    // Send content
    res->end(content);
    
    LOG_INFO("Served static file: " + path + " (" + std::to_string(content.length()) + " bytes)");
}

std::string StaticFileController::getMimeType(const std::string& path) {
    static const std::unordered_map<std::string, std::string> mimeTypes = {
        {".html", "text/html; charset=utf-8"},
        {".css", "text/css"},
        {".js", "application/javascript"},
        {".json", "application/json"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif", "image/gif"},
        {".svg", "image/svg+xml"},
        {".ico", "image/x-icon"},
        {".woff", "font/woff"},
        {".woff2", "font/woff2"},
        {".ttf", "font/ttf"},
        {".otf", "font/otf"},
        {".mp4", "video/mp4"},
        {".webm", "video/webm"},
        {".mp3", "audio/mpeg"},
        {".wav", "audio/wav"},
        {".pdf", "application/pdf"},
        {".zip", "application/zip"},
        {".txt", "text/plain"}
    };
    
    // Get file extension
    size_t dotPos = path.find_last_of('.');
    if (dotPos != std::string::npos) {
        std::string ext = path.substr(dotPos);
        auto it = mimeTypes.find(ext);
        if (it != mimeTypes.end()) {
            return it->second;
        }
    }
    
    return "application/octet-stream";
}

bool StaticFileController::shouldCache(const std::string& path) {
    // Cache images, fonts, and other assets
    static const std::vector<std::string> cacheableExtensions = {
        ".png", ".jpg", ".jpeg", ".gif", ".svg", ".ico",
        ".woff", ".woff2", ".ttf", ".otf",
        ".mp4", ".webm", ".mp3", ".wav"
    };
    
    for (const auto& ext : cacheableExtensions) {
        if (path.length() >= ext.length() && 
            path.compare(path.length() - ext.length(), ext.length(), ext) == 0) {
            return true;
        }
    }
    
    return false;
}

std::string StaticFileController::readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open file: " + path);
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string StaticFileController::generateETag(const std::string& content) {
    // Generate ETag based on content hash
    std::hash<std::string> hasher;
    size_t hash = hasher(content);
    return "\"" + std::to_string(hash) + "\"";
}

std::string StaticFileController::getLastModifiedHeader(const std::string& filePath) {
    try {
        auto ftime = std::filesystem::last_write_time(filePath);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
        auto time_t = std::chrono::system_clock::to_time_t(sctp);
        
        char buffer[100];
        std::strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", std::gmtime(&time_t));
        return std::string(buffer);
    } catch (const std::exception& e) {
        LOG_WARNING("Failed to get last modified time for " + filePath + ": " + std::string(e.what()));
        return "";
    }
} 