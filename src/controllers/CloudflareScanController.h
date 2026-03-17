#pragma once

#include "../../include/routing/Controller.h"
#include "../../include/routing/RouteRegistry.h"
#include "../../include/search_engine/storage/CloudflareScanStorage.h"
#include "../../include/search_engine/cloudflare/CloudflareScanner.h"
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>

/**
 * CloudflareScanController
 *
 * Provides API endpoints for scanning Cloudflare IP ranges for open port 443
 * (HTTPS) hosts.
 *
 * Endpoints:
 *   GET  /api/cloudflare/ranges               – list known Cloudflare CIDR ranges
 *   POST /api/cloudflare/scan/start           – start a background port scan
 *   GET  /api/cloudflare/scan/status          – query scan session status
 *   GET  /api/cloudflare/scan/results         – retrieve scan results (paged)
 *   POST /api/cloudflare/scan/stop            – stop a running scan
 */
class CloudflareScanController : public routing::Controller {
public:
    CloudflareScanController() = default;

    void getRanges(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    void startScan(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    void getScanStatus(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    void getScanResults(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    void stopScan(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);

private:
    // Lazy-initialised storage
    mutable std::unique_ptr<search_engine::storage::CloudflareScanStorage> storage_;
    search_engine::storage::CloudflareScanStorage* getStorage() const;

    // Active scan state (one concurrent scan per controller instance)
    mutable std::atomic<bool> scanRunning_{false};
    mutable std::string       activeSessionId_;
    mutable std::mutex        sessionMutex_;

    std::string getMongoUri() const;
};

ROUTE_CONTROLLER(CloudflareScanController) {
    using namespace routing;
    REGISTER_ROUTE(HttpMethod::GET,  "/api/cloudflare/ranges",        getRanges,      CloudflareScanController);
    REGISTER_ROUTE(HttpMethod::POST, "/api/cloudflare/scan/start",    startScan,      CloudflareScanController);
    REGISTER_ROUTE(HttpMethod::GET,  "/api/cloudflare/scan/status",   getScanStatus,  CloudflareScanController);
    REGISTER_ROUTE(HttpMethod::GET,  "/api/cloudflare/scan/results",  getScanResults, CloudflareScanController);
    REGISTER_ROUTE(HttpMethod::POST, "/api/cloudflare/scan/stop",     stopScan,       CloudflareScanController);
}
