#pragma once

#include "../../include/routing/Controller.h"
#include "../../include/routing/RouteRegistry.h"
#include "../../include/search_engine/storage/EmailTrackingStorage.h"
#include <memory>

/**
 * @brief Controller for handling email tracking pixel requests
 * 
 * This controller serves transparent 1x1 pixel images for email tracking
 * and records email open events with IP address and user agent information.
 */
class TrackingController : public routing::Controller {
public:
    TrackingController();
    ~TrackingController() = default;

    /**
     * @brief Serve tracking pixel and record email open
     * GET /track/:tracking_id.png
     */
    void trackEmailOpen(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);

    /**
     * @brief Get tracking statistics for an email address
     * GET /api/v2/tracking/stats?email=user@example.com
     */
    void getTrackingStats(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);

private:
    mutable std::unique_ptr<search_engine::storage::EmailTrackingStorage> trackingStorage_;

    /**
     * @brief Get or create EmailTrackingStorage instance (lazy initialization)
     */
    search_engine::storage::EmailTrackingStorage* getTrackingStorage() const;

    /**
     * @brief Serve a transparent 1x1 PNG pixel
     */
    void serveTrackingPixel(uWS::HttpResponse<false>* res);

    /**
     * @brief Extract client IP address from request
     */
    std::string getClientIP(uWS::HttpRequest* req);

    /**
     * @brief Extract User-Agent from request headers
     */
    std::string getUserAgent(uWS::HttpRequest* req);
};

// Route registration
ROUTE_CONTROLLER(TrackingController) {
    using namespace routing;
    REGISTER_ROUTE(HttpMethod::GET, "/track/*", trackEmailOpen, TrackingController);
    REGISTER_ROUTE(HttpMethod::GET, "/api/v2/tracking/stats", getTrackingStats, TrackingController);
}

