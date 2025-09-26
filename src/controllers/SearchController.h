#pragma once
#include "../../include/routing/Controller.h"
#include "../../include/search_core/SearchClient.hpp"
#include <memory>
#include <nlohmann/json.hpp>

class SearchController : public routing::Controller {
public:
    SearchController();
    
    // Search functionality
    void search(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    void searchSiteProfiles(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    
    // Search results page (web interface)
    void searchResultsPage(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    
    // Crawl management
    void addSiteToCrawl(uWS::HttpResponse<false>* res, uWS::HttpRequest* req); // Supports 'force' parameter
    void getCrawlStatus(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    void getCrawlDetails(uWS::HttpResponse<false>* res, uWS::HttpRequest* req); // New endpoint
    
    // SPA detection
    void detectSpa(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    void renderPage(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);

private:
    nlohmann::json parseRedisSearchResponse(const std::string& rawResponse, int page, int limit);
    
    // Helper methods for template rendering
    std::string loadFile(const std::string& path);
    std::string renderTemplate(const std::string& templateName, const nlohmann::json& data);
    std::string getDefaultLocale();
    
    // Helper method for logging API request errors
    void logApiRequestError(const std::string& endpoint, const std::string& method, 
                           const std::string& ipAddress, const std::string& userAgent,
                           const std::chrono::system_clock::time_point& requestStartTime,
                           const std::string& requestBody, const std::string& status, 
                           const std::string& errorMessage);
};

// Route registration
ROUTE_CONTROLLER(SearchController) {
    using namespace routing;
    REGISTER_ROUTE(HttpMethod::GET, "/api/search", search, SearchController);
    REGISTER_ROUTE(HttpMethod::GET, "/api/search/sites", searchSiteProfiles, SearchController);
    REGISTER_ROUTE(HttpMethod::GET, "/search", searchResultsPage, SearchController);
    REGISTER_ROUTE(HttpMethod::POST, "/api/crawl/add-site", addSiteToCrawl, SearchController);
    REGISTER_ROUTE(HttpMethod::GET, "/api/crawl/status", getCrawlStatus, SearchController);
    REGISTER_ROUTE(HttpMethod::GET, "/api/crawl/details", getCrawlDetails, SearchController); // New endpoint
    REGISTER_ROUTE(HttpMethod::POST, "/api/spa/detect", detectSpa, SearchController);
} 