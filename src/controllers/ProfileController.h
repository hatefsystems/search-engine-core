#ifndef PROFILE_CONTROLLER_H
#define PROFILE_CONTROLLER_H

#include "../../include/routing/Controller.h"
#include "../../include/routing/RouteRegistry.h"
#include "../../include/search_engine/storage/ProfileStorage.h"
#include "../../include/search_engine/storage/ProfileViewAnalytics.h"
#include "../../include/search_engine/storage/LegalComplianceLog.h"
#include "../../include/search_engine/storage/GeoIPService.h"
#include "../../include/search_engine/storage/UserAgentParser.h"
#include "../../include/search_engine/storage/DataEncryption.h"
#include "../../include/search_engine/common/SlugCache.h"
#include "../../include/Logger.h"
#include <memory>

class ProfileController : public routing::Controller {
public:
    ProfileController();
    ~ProfileController() = default;

    // API Endpoints
    void createProfile(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    void getProfileById(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    void getPublicProfile(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    void updateProfile(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    void deleteProfile(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    void listProfiles(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);

    // Root-level URL endpoints
    void getPublicProfileBySlug(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);

    // Slug management API endpoints
    void checkSlugAvailability(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    void changeSlug(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    
    // Privacy & compliance endpoints
    void getPrivacyDashboard(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    void cleanupExpiredComplianceLogs(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);

private:
    mutable std::unique_ptr<search_engine::storage::ProfileStorage> storage_;
    mutable std::unique_ptr<search_engine::common::SlugCache> slugCache_;
    mutable std::unique_ptr<search_engine::storage::ProfileViewAnalyticsStorage> analyticsStorage_;
    mutable std::unique_ptr<search_engine::storage::ComplianceStorage> complianceStorage_;

    // Lazy initialization helpers
    search_engine::storage::ProfileStorage* getStorage() const;
    search_engine::common::SlugCache* getSlugCache() const;
    search_engine::storage::ProfileViewAnalyticsStorage* getAnalyticsStorage() const;
    search_engine::storage::ComplianceStorage* getComplianceStorage() const;

    // Helper to parse JSON request body
    search_engine::storage::Profile parseProfileFromJson(const nlohmann::json& json);

    // Helper to convert Profile to JSON response
    nlohmann::json profileToJson(const search_engine::storage::Profile& profile);

    // Helper to convert ProfileType enum to string
    static std::string profileTypeToString(search_engine::storage::ProfileType type);

    // Helper to parse ProfileType enum from string
    static search_engine::storage::ProfileType stringToProfileType(const std::string& type);

    // Helper for SEO redirects
    bool checkAndRedirectOldSlug(uWS::HttpResponse<false>* res, const std::string& requestedSlug);
    
    // Privacy & tracking helpers
    std::string getClientIP(uWS::HttpRequest* req);
    std::string getUserAgent(uWS::HttpRequest* req);
    std::string getReferrer(uWS::HttpRequest* req);
    void recordProfileView(const std::string& profileId, uWS::HttpRequest* req);
};

// Route registration
ROUTE_CONTROLLER(ProfileController) {
    using namespace routing;
    LOG_INFO("ProfileController::registerRoutes() called - registering routes");

    // API routes
    REGISTER_ROUTE(HttpMethod::POST, "/api/profiles", createProfile, ProfileController);
    REGISTER_ROUTE(HttpMethod::GET, "/api/profiles/:id", getProfileById, ProfileController);
    REGISTER_ROUTE(HttpMethod::PUT, "/api/profiles/:id", updateProfile, ProfileController);
    REGISTER_ROUTE(HttpMethod::DELETE, "/api/profiles/:id", deleteProfile, ProfileController);
    REGISTER_ROUTE(HttpMethod::GET, "/api/profiles", listProfiles, ProfileController);

    // Slug management API routes
    REGISTER_ROUTE(HttpMethod::GET, "/api/profiles/check-slug", checkSlugAvailability, ProfileController);
    REGISTER_ROUTE(HttpMethod::POST, "/api/profiles/:id/change-slug", changeSlug, ProfileController);
    
    // Privacy & compliance API routes
    REGISTER_ROUTE(HttpMethod::GET, "/api/profiles/:id/privacy-dashboard", getPrivacyDashboard, ProfileController);
    REGISTER_ROUTE(HttpMethod::POST, "/api/internal/compliance/cleanup", cleanupExpiredComplianceLogs, ProfileController);

    // Legacy profile route
    REGISTER_ROUTE(HttpMethod::GET, "/profiles/:slug", getPublicProfile, ProfileController);

    // Root-level routes (must come after static routes to avoid conflicts)
    REGISTER_ROUTE(HttpMethod::GET, "/:slug", getPublicProfileBySlug, ProfileController);

    LOG_INFO("ProfileController::registerRoutes() completed - routes registered");
}

#endif // PROFILE_CONTROLLER_H
