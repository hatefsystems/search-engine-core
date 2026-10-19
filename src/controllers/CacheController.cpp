#include "CacheController.h"
#include "../../include/Logger.h"
#include <sstream>
#include <iomanip>

// Initialize static metrics
std::atomic<uint64_t> CacheController::totalRequests{0};
std::atomic<uint64_t> CacheController::cacheHits{0};
std::atomic<uint64_t> CacheController::cacheMisses{0};
std::atomic<uint64_t> CacheController::minificationTime{0};
std::atomic<uint64_t> CacheController::cacheTime{0};

CacheController::CacheController() {
    // Routes are registered via the ROUTE_CONTROLLER macro below
}

void CacheController::getCacheStats(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    uint64_t total = totalRequests.load();
    uint64_t hits = cacheHits.load();
    uint64_t misses = cacheMisses.load();
    uint64_t minTime = minificationTime.load();
    uint64_t cacheTimeTotal = cacheTime.load();
    
    double hitRate = total > 0 ? (double)hits / total : 0.0;
    double avgMinTime = hits > 0 ? (double)minTime / hits / 1000.0 : 0.0; // Convert to ms
    double avgCacheTime = total > 0 ? (double)cacheTimeTotal / total / 1000.0 : 0.0; // Convert to ms
    
    std::ostringstream json;
    json << "{"
         << "\"cache_hit_rate\":" << std::fixed << std::setprecision(3) << hitRate << ","
         << "\"total_requests\":" << total << ","
         << "\"cache_hits\":" << hits << ","
         << "\"cache_misses\":" << misses << ","
         << "\"avg_minification_time_ms\":" << std::fixed << std::setprecision(2) << avgMinTime << ","
         << "\"avg_cache_time_ms\":" << std::fixed << std::setprecision(2) << avgCacheTime << ","
         << "\"total_minification_time_ms\":" << std::fixed << std::setprecision(2) << (minTime / 1000.0) << ","
         << "\"total_cache_time_ms\":" << std::fixed << std::setprecision(2) << (cacheTimeTotal / 1000.0) << ","
         << "\"timestamp\":\"" << std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count() << "\""
         << "}";
    
    res->writeStatus("200 OK")
       ->writeHeader("Content-Type", "application/json")
       ->writeHeader("Cache-Control", "no-cache")
       ->writeHeader("Server", "HatefEngine 1.0")->end(json.str());
    
    LOG_INFO("Cache stats requested - Hit rate: " + std::to_string(hitRate * 100) + "%");
}

void CacheController::clearCache(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    // Reset all metrics
    totalRequests.store(0);
    cacheHits.store(0);
    cacheMisses.store(0);
    minificationTime.store(0);
    cacheTime.store(0);
    
    std::string response = "{\"status\":\"success\",\"message\":\"Cache metrics cleared\"}";
    
    res->writeStatus("200 OK")
       ->writeHeader("Content-Type", "application/json")
       ->writeHeader("Server", "HatefEngine 1.0")->end(response);
    
    LOG_INFO("Cache metrics cleared");
}

void CacheController::getCacheInfo(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    std::ostringstream json;
    json << "{"
         << "\"cache_type\":\"redis\","
         << "\"cache_enabled\":true,"
         << "\"cache_ttl_seconds\":3600,"
         << "\"redis_db\":1,"
         << "\"features\":["
         << "\"automatic_expiration\","
         << "\"thread_safe\","
         << "\"shared_across_instances\","
         << "\"persistent_storage\""
         << "],"
         << "\"performance\":{"
         << "\"memory_usage\":\"50-100MB\","
         << "\"access_time\":\"~2ms\","
         << "\"max_memory\":\"256MB\""
         << "}"
         << "}";
    
    res->writeStatus("200 OK")
       ->writeHeader("Content-Type", "application/json")
       ->writeHeader("Cache-Control", "public, max-age=300") // Cache for 5 minutes
       ->writeHeader("Server", "HatefEngine 1.0")->end(json.str());
}

void CacheController::recordCacheHit(uint64_t cacheTimeUs) {
    totalRequests.fetch_add(1);
    cacheHits.fetch_add(1);
    cacheTime.fetch_add(cacheTimeUs);
}

void CacheController::recordCacheMiss(uint64_t minificationTimeUs) {
    totalRequests.fetch_add(1);
    cacheMisses.fetch_add(1);
    minificationTime.fetch_add(minificationTimeUs);
}

double CacheController::getCacheHitRate() {
    uint64_t total = totalRequests.load();
    uint64_t hits = cacheHits.load();
    return total > 0 ? (double)hits / total : 0.0;
}

double CacheController::getAverageMinificationTime() {
    uint64_t misses = cacheMisses.load();
    uint64_t totalTime = minificationTime.load();
    return misses > 0 ? (double)totalTime / misses / 1000.0 : 0.0; // Convert to ms
}

double CacheController::getAverageCacheTime() {
    uint64_t total = totalRequests.load();
    uint64_t totalTime = cacheTime.load();
    return total > 0 ? (double)totalTime / total / 1000.0 : 0.0; // Convert to ms
}

// Route registration using macros (similar to .NET Core attributes)
ROUTE_CONTROLLER(CacheController) {
    using namespace routing;
    REGISTER_ROUTE(HttpMethod::GET, "/api/cache/stats", getCacheStats, CacheController);
    REGISTER_ROUTE(HttpMethod::POST, "/api/cache/clear", clearCache, CacheController);
    REGISTER_ROUTE(HttpMethod::GET, "/api/cache/info", getCacheInfo, CacheController);
}
