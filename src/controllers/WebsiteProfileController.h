#ifndef WEBSITE_PROFILE_CONTROLLER_H
#define WEBSITE_PROFILE_CONTROLLER_H

#include "../../include/routing/Controller.h"
#include "../../include/routing/RouteRegistry.h"
#include "../storage/WebsiteProfileStorage.h"
#include <memory>

class WebsiteProfileController : public routing::Controller {
public:
    WebsiteProfileController();
    ~WebsiteProfileController() = default;

    // API Endpoints
    void saveProfile(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    void getProfile(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    void getAllProfiles(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    void updateProfile(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    void deleteProfile(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    void checkProfile(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);

private:
    mutable std::unique_ptr<search_engine::storage::WebsiteProfileStorage> storage_;

    // Lazy initialization helper
    search_engine::storage::WebsiteProfileStorage* getStorage() const;
    
    // Helper to parse JSON request body
    search_engine::storage::WebsiteProfile parseProfileFromJson(const nlohmann::json& json);
    
    // Helper to trigger crawl for a website URL
    void triggerCrawlForWebsite(const std::string& websiteUrl, const std::string& email = "", const std::string& ownerName = "");
};

// Route registration
ROUTE_CONTROLLER(WebsiteProfileController) {
    using namespace routing;
    REGISTER_ROUTE(HttpMethod::POST, "/api/v2/website-profile", saveProfile, WebsiteProfileController);
    REGISTER_ROUTE(HttpMethod::GET, "/api/v2/website-profile/:url", getProfile, WebsiteProfileController);
    REGISTER_ROUTE(HttpMethod::GET, "/api/v2/website-profiles", getAllProfiles, WebsiteProfileController);
    REGISTER_ROUTE(HttpMethod::PUT, "/api/v2/website-profile/:url", updateProfile, WebsiteProfileController);
    REGISTER_ROUTE(HttpMethod::DELETE, "/api/v2/website-profile/:url", deleteProfile, WebsiteProfileController);
    REGISTER_ROUTE(HttpMethod::GET, "/api/v2/website-profile/check/:url", checkProfile, WebsiteProfileController);
}

#endif // WEBSITE_PROFILE_CONTROLLER_H

