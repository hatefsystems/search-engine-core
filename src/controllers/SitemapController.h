#ifndef SITEMAP_CONTROLLER_H
#define SITEMAP_CONTROLLER_H

#include "../../include/routing/Controller.h"
#include "../../include/routing/RouteRegistry.h"
#include "../../include/search_engine/storage/ProfileStorage.h"
#include "../../include/Logger.h"
#include <memory>
#include <chrono>
#include <mutex>

class SitemapController : public routing::Controller {
public:
    SitemapController();
    ~SitemapController() = default;

    // Sitemap endpoints
    void getSitemapIndex(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    void getProfilesSitemap(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    void getStaticSitemap(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);

private:
    mutable std::unique_ptr<search_engine::storage::ProfileStorage> storage_;
    
    // Cache for sitemap content
    mutable std::string cachedSitemapIndex_;
    mutable std::string cachedStaticSitemap_;
    mutable std::chrono::system_clock::time_point lastCacheTime_;
    mutable std::mutex cacheMutex_;
    
    // Cache TTL from environment (default 3600 seconds = 1 hour)
    int cacheTtlSeconds_;

    // Lazy initialization helper
    search_engine::storage::ProfileStorage* getStorage() const;

    // Check if cache is valid
    bool isCacheValid() const;

    // Clear cache
    void clearCache();
};

// Route registration
ROUTE_CONTROLLER(SitemapController) {
    using namespace routing;
    LOG_INFO("SitemapController::registerRoutes() called - registering routes");

    REGISTER_ROUTE(HttpMethod::GET, "/sitemap.xml", getSitemapIndex, SitemapController);
    REGISTER_ROUTE(HttpMethod::GET, "/sitemap-profiles-:page.xml", getProfilesSitemap, SitemapController);
    REGISTER_ROUTE(HttpMethod::GET, "/sitemap-static.xml", getStaticSitemap, SitemapController);

    LOG_INFO("SitemapController::registerRoutes() completed - routes registered");
}

#endif // SITEMAP_CONTROLLER_H
