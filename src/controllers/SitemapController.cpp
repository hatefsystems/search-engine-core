#include "SitemapController.h"
#include "../../include/search_engine/seo/SitemapGenerator.h"
#include "../../include/Logger.h"
#include <cstdlib>

SitemapController::SitemapController() {
    LOG_DEBUG("SitemapController created (lazy initialization)");
    
    // Get cache TTL from environment
    const char* ttlEnv = std::getenv("SITEMAP_CACHE_TTL");
    cacheTtlSeconds_ = ttlEnv ? std::atoi(ttlEnv) : 3600; // Default 1 hour
    
    LOG_INFO("SitemapController cache TTL: " + std::to_string(cacheTtlSeconds_) + " seconds");
}

search_engine::storage::ProfileStorage* SitemapController::getStorage() const {
    if (!storage_) {
        try {
            LOG_INFO("Lazy initializing ProfileStorage for sitemap");
            storage_ = std::make_unique<search_engine::storage::ProfileStorage>();
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to lazy initialize ProfileStorage: " + std::string(e.what()));
            throw;
        }
    }
    return storage_.get();
}

bool SitemapController::isCacheValid() const {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastCacheTime_).count();
    return elapsed < cacheTtlSeconds_;
}

void SitemapController::clearCache() {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    cachedSitemapIndex_.clear();
    cachedStaticSitemap_.clear();
    lastCacheTime_ = std::chrono::system_clock::time_point{};
}

void SitemapController::getSitemapIndex(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    try {
        LOG_INFO("Serving sitemap index");

        // Check cache first
        if (isCacheValid() && !cachedSitemapIndex_.empty()) {
            LOG_DEBUG("Serving cached sitemap index");
            res->writeHeader("Content-Type", "application/xml; charset=utf-8");
            res->writeHeader("Cache-Control", "public, max-age=" + std::to_string(cacheTtlSeconds_));
            res->end(cachedSitemapIndex_);
            return;
        }

        // Get base URL from environment
        const char* baseUrlEnv = std::getenv("BASE_URL");
        std::string baseUrl = baseUrlEnv ? std::string(baseUrlEnv) : "http://localhost:3000";

        // Fetch profiles (for simplicity, we'll fetch a batch and generate single sitemap)
        // In production with >50k profiles, you'd need pagination logic
        auto profilesResult = getStorage()->findAll(50000, 0);
        
        if (!profilesResult.success) {
            LOG_ERROR("Failed to fetch profiles for sitemap: " + profilesResult.message);
            serverError(res, "Failed to generate sitemap");
            return;
        }

        int totalProfiles = profilesResult.value.size();
        LOG_DEBUG("Generating sitemap for " + std::to_string(totalProfiles) + " profiles");

        // Generate sitemap
        std::string sitemapXml;
        
        if (totalProfiles <= 50000) {
            // Single sitemap - generate profiles sitemap directly
            sitemapXml = search_engine::seo::SitemapGenerator::generateProfilesSitemap(
                profilesResult.value,
                baseUrl
            );
        } else {
            // Multiple sitemaps needed - generate sitemap index
            sitemapXml = search_engine::seo::SitemapGenerator::generateSitemapIndex(
                totalProfiles,
                baseUrl
            );
        }

        // Cache the result
        {
            std::lock_guard<std::mutex> lock(cacheMutex_);
            cachedSitemapIndex_ = sitemapXml;
            lastCacheTime_ = std::chrono::system_clock::now();
        }

        // Send response
        res->writeHeader("Content-Type", "application/xml; charset=utf-8");
        res->writeHeader("Cache-Control", "public, max-age=" + std::to_string(cacheTtlSeconds_));
        res->end(sitemapXml);

        LOG_INFO("Sitemap index served successfully");

    } catch (const std::exception& e) {
        LOG_ERROR("Error generating sitemap index: " + std::string(e.what()));
        serverError(res, "Internal server error");
    }
}

void SitemapController::getProfilesSitemap(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    try {
        // Get page number from URL parameter
        std::string pageStr = std::string(req->getParameter(0));
        int page = 1;
        
        try {
            page = std::stoi(pageStr);
        } catch (...) {
            badRequest(res, "Invalid page number");
            return;
        }

        LOG_INFO("Serving profiles sitemap page " + std::to_string(page));

        // Get base URL from environment
        const char* baseUrlEnv = std::getenv("BASE_URL");
        std::string baseUrl = baseUrlEnv ? std::string(baseUrlEnv) : "http://localhost:3000";

        // Calculate offset and limit
        int profilesPerPage = 50000;
        int offset = (page - 1) * profilesPerPage;

        // Fetch profiles for this page
        auto profilesResult = getStorage()->findAll(profilesPerPage, offset);

        if (!profilesResult.success) {
            LOG_ERROR("Failed to fetch profiles for sitemap page " + std::to_string(page) + ": " + profilesResult.message);
            notFound(res, "Sitemap page not found");
            return;
        }

        // Generate sitemap
        std::string sitemapXml = search_engine::seo::SitemapGenerator::generateProfilesSitemap(
            profilesResult.value,
            baseUrl
        );

        // Send response
        res->writeHeader("Content-Type", "application/xml; charset=utf-8");
        res->writeHeader("Cache-Control", "public, max-age=" + std::to_string(cacheTtlSeconds_));
        res->end(sitemapXml);

        LOG_INFO("Profiles sitemap page " + std::to_string(page) + " served successfully");

    } catch (const std::exception& e) {
        LOG_ERROR("Error generating profiles sitemap: " + std::string(e.what()));
        serverError(res, "Internal server error");
    }
}

void SitemapController::getStaticSitemap(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    try {
        LOG_INFO("Serving static pages sitemap");

        // Check cache first
        if (isCacheValid() && !cachedStaticSitemap_.empty()) {
            LOG_DEBUG("Serving cached static sitemap");
            res->writeHeader("Content-Type", "application/xml; charset=utf-8");
            res->writeHeader("Cache-Control", "public, max-age=" + std::to_string(cacheTtlSeconds_));
            res->end(cachedStaticSitemap_);
            return;
        }

        // Get base URL from environment
        const char* baseUrlEnv = std::getenv("BASE_URL");
        std::string baseUrl = baseUrlEnv ? std::string(baseUrlEnv) : "http://localhost:3000";

        // Generate static pages sitemap
        std::string sitemapXml = search_engine::seo::SitemapGenerator::generateStaticPagesSitemap(baseUrl);

        // Cache the result
        {
            std::lock_guard<std::mutex> lock(cacheMutex_);
            cachedStaticSitemap_ = sitemapXml;
            lastCacheTime_ = std::chrono::system_clock::now();
        }

        // Send response
        res->writeHeader("Content-Type", "application/xml; charset=utf-8");
        res->writeHeader("Cache-Control", "public, max-age=" + std::to_string(cacheTtlSeconds_));
        res->end(sitemapXml);

        LOG_INFO("Static sitemap served successfully");

    } catch (const std::exception& e) {
        LOG_ERROR("Error generating static sitemap: " + std::string(e.what()));
        serverError(res, "Internal server error");
    }
}
