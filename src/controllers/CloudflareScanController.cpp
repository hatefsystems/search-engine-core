#include "CloudflareScanController.h"
#include "../../include/Logger.h"
#include "../../include/infrastructure.h"
#include <nlohmann/json.hpp>
#include <thread>
#include <cstdlib>

using json = nlohmann::json;
using namespace search_engine::cloudflare;
using namespace search_engine::storage;

// ---------------------------------------------------------------------------
// Lazy initialisation
// ---------------------------------------------------------------------------

std::string CloudflareScanController::getMongoUri() const {
    const char* env = std::getenv("MONGODB_URI");
    return env ? env : "mongodb://localhost:27017";
}

CloudflareScanStorage* CloudflareScanController::getStorage() const {
    if (!storage_) {
        try {
            LOG_INFO("Lazy initialising CloudflareScanStorage");
            storage_ = std::make_unique<CloudflareScanStorage>(getMongoUri());
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to lazy init CloudflareScanStorage: " +
                      std::string(e.what()));
            throw;
        }
    }
    return storage_.get();
}

// ---------------------------------------------------------------------------
// GET /api/cloudflare/ranges
// ---------------------------------------------------------------------------

void CloudflareScanController::getRanges(uWS::HttpResponse<false>* res,
                                          uWS::HttpRequest* /*req*/) {
    try {
        const auto ipv4 = CloudflareScanner::getDefaultIPv4Ranges();
        const auto ipv6 = CloudflareScanner::getDefaultIPv6Ranges();

        json ipv4Arr = json::array();
        for (const auto& r : ipv4) ipv4Arr.push_back(r);

        json ipv6Arr = json::array();
        for (const auto& r : ipv6) ipv6Arr.push_back(r);

        json response = {
            {"success", true},
            {"data", {
                {"ipv4", ipv4Arr},
                {"ipv6", ipv6Arr},
                {"source", "https://www.cloudflare.com/ips/"}
            }}
        };
        this->json(res, response);
    } catch (const std::exception& e) {
        LOG_ERROR("getRanges error: " + std::string(e.what()));
        serverError(res, "Failed to retrieve ranges");
    }
}

// ---------------------------------------------------------------------------
// POST /api/cloudflare/scan/start
// ---------------------------------------------------------------------------

void CloudflareScanController::startScan(uWS::HttpResponse<false>* res,
                                          uWS::HttpRequest* /*req*/) {
    std::string buffer;
    res->onData([this, res, buffer = std::move(buffer)](
                    std::string_view data, bool last) mutable {
        buffer.append(data.data(), data.length());
        if (!last) return;

        try {
            // Parse optional request body
            json body;
            try {
                body = buffer.empty() ? json::object() : json::parse(buffer);
            } catch (...) {
                body = json::object();
            }

            // Check if a scan is already running
            if (scanRunning_.load()) {
                std::lock_guard<std::mutex> lk(sessionMutex_);
                json resp = {
                    {"success", false},
                    {"message", "A scan is already running"},
                    {"sessionId", activeSessionId_}
                };
                this->json(res, resp, "409 Conflict");
                return;
            }

            // Build scan parameters
            const int threads    = body.value("threads", 10);
            const int timeoutMs  = body.value("timeoutMs", 3000);
            const int maxIPs     = body.value("maxIPsPerRange", 256);
            const int port       = body.value("port", 443);
            const bool allRanges = body.value("allRanges", true);

            std::vector<std::string> ranges;
            if (body.contains("ranges") && body["ranges"].is_array()) {
                for (const auto& r : body["ranges"]) {
                    if (r.is_string()) ranges.push_back(r.get<std::string>());
                }
            }
            if (ranges.empty()) {
                if (allRanges) {
                    ranges = CloudflareScanner::getDefaultIPv4Ranges();
                } else {
                    badRequest(res, "No CIDR ranges specified");
                    return;
                }
            }

            // Estimate total IPs
            int totalIPs = 0;
            for (const auto& cidr : ranges) {
                const auto ips = CloudflareScanner::expandCIDRv4(
                    cidr, static_cast<size_t>(maxIPs));
                totalIPs += static_cast<int>(ips.size());
            }

            // Create session in DB
            CloudflareScanSession session;
            session.status        = "running";
            session.startedAt     = CloudflareScanner::nowISO8601();
            session.cidrRanges    = ranges;
            session.totalIPs      = totalIPs;
            session.threads       = threads;
            session.timeoutMs     = timeoutMs;
            session.maxIPsPerRange = static_cast<size_t>(maxIPs);

            auto createResult = getStorage()->createSession(session);
            if (!createResult.success) {
                serverError(res, "Failed to create scan session: " +
                                 createResult.message);
                return;
            }

            const std::string sessionId = createResult.value;
            {
                std::lock_guard<std::mutex> lk(sessionMutex_);
                activeSessionId_ = sessionId;
            }
            scanRunning_.store(true);

            // Note: the controller instance is static (created by REGISTER_ROUTE macro),
            // so `this` remains valid for the entire application lifetime. The storage
            // pointer is captured explicitly to make the lifetime guarantee clear.
            CloudflareScanStorage* storage = getStorage();
            std::atomic<bool>* running    = &scanRunning_;

            // Launch background scan thread (detached – updates DB directly)
            std::thread([storage, running, sessionId, ranges, port, threads, timeoutMs,
                         maxIPs]() mutable {
                LOG_INFO("Cloudflare scan started: session " + sessionId);
                int scanned = 0;
                int open    = 0;

                for (const auto& cidr : ranges) {
                    if (!running->load()) break;

                    CloudflareScanner::scanRange(
                        cidr, port,
                        [storage, &sessionId, &scanned, &open](const ScanResult& r) {
                            ++scanned;
                            if (r.open) ++open;

                            // Persist result
                            CloudflareScanResult stored;
                            stored.ip            = r.ip;
                            stored.port          = r.port;
                            stored.open          = r.open;
                            stored.responseTimeMs = r.responseTimeMs;
                            stored.hostname      = r.hostname;
                            stored.cidr          = r.cidr;
                            stored.scannedAt     = r.scannedAt;
                            storage->storeResult(sessionId, stored);

                            // Periodically update session counters
                            if (scanned % 50 == 0) {
                                CloudflareScanSession upd;
                                upd.scannedIPs = scanned;
                                upd.openPorts  = open;
                                upd.status     = "running";
                                storage->updateSession(sessionId, upd);
                            }
                        },
                        threads, timeoutMs,
                        static_cast<size_t>(maxIPs),
                        true,    // reportClosed = true (store all)
                        running);
                }

                // Finalise session
                CloudflareScanSession final;
                final.scannedIPs  = scanned;
                final.openPorts   = open;
                final.completedAt = CloudflareScanner::nowISO8601();
                final.status      = running->load() ? "completed" : "stopped";
                running->store(false);
                storage->updateSession(sessionId, final);
                LOG_INFO("Cloudflare scan finished: session " + sessionId +
                         " scanned=" + std::to_string(scanned) +
                         " open=" + std::to_string(open));
            }).detach();

            json resp = {
                {"success",   true},
                {"message",   "Scan started"},
                {"sessionId", sessionId},
                {"totalIPs",  totalIPs},
                {"ranges",    ranges}
            };
            this->json(res, resp);

        } catch (const std::exception& e) {
            LOG_ERROR("startScan error: " + std::string(e.what()));
            serverError(res, "Failed to start scan");
        }
    });
    res->onAborted([]() {
        LOG_WARNING("startScan: client disconnected during request processing");
    });
}

// ---------------------------------------------------------------------------
// GET /api/cloudflare/scan/status?session_id=<id>
// ---------------------------------------------------------------------------

void CloudflareScanController::getScanStatus(uWS::HttpResponse<false>* res,
                                              uWS::HttpRequest* req) {
    try {
        auto params    = parseQuery(req);
        auto it        = params.find("session_id");

        std::string sessionId;
        if (it != params.end() && !it->second.empty()) {
            sessionId = it->second;
        } else {
            std::lock_guard<std::mutex> lk(sessionMutex_);
            sessionId = activeSessionId_;
        }

        if (sessionId.empty()) {
            json resp = {
                {"success", false},
                {"message", "No active or specified scan session"}
            };
            this->json(res, resp, "400 Bad Request");
            return;
        }

        auto result = getStorage()->getSession(sessionId);
        if (!result.success) {
            serverError(res, result.message);
            return;
        }
        if (!result.value.has_value()) {
            json resp = {{"success", false}, {"message", "Session not found"}};
            this->json(res, resp, "404 Not Found");
            return;
        }

        const auto& s = result.value.value();
        json rangesArr = json::array();
        for (const auto& r : s.cidrRanges) rangesArr.push_back(r);

        json resp = {
            {"success", true},
            {"data", {
                {"sessionId",    s.id},
                {"status",       s.status},
                {"startedAt",    s.startedAt},
                {"completedAt",  s.completedAt},
                {"totalIPs",     s.totalIPs},
                {"scannedIPs",   s.scannedIPs},
                {"openPorts",    s.openPorts},
                {"threads",      s.threads},
                {"timeoutMs",    s.timeoutMs},
                {"maxIPsPerRange", s.maxIPsPerRange},
                {"cidrRanges",   rangesArr},
                {"errorMessage", s.errorMessage}
            }}
        };
        this->json(res, resp);

    } catch (const std::exception& e) {
        LOG_ERROR("getScanStatus error: " + std::string(e.what()));
        serverError(res, "Failed to get scan status");
    }
}

// ---------------------------------------------------------------------------
// GET /api/cloudflare/scan/results?session_id=<id>&page=1&limit=50&open_only=true
// ---------------------------------------------------------------------------

void CloudflareScanController::getScanResults(uWS::HttpResponse<false>* res,
                                               uWS::HttpRequest* req) {
    try {
        auto params    = parseQuery(req);

        std::string sessionId;
        if (auto it = params.find("session_id"); it != params.end()) {
            sessionId = it->second;
        }
        if (sessionId.empty()) {
            std::lock_guard<std::mutex> lk(sessionMutex_);
            sessionId = activeSessionId_;
        }
        if (sessionId.empty()) {
            this->json(res, json{{"success", false}, {"message", "session_id required"}},
                       "400 Bad Request");
            return;
        }

        int page  = 1;
        int limit = 50;
        bool openOnly = false;

        if (auto it = params.find("page");  it != params.end()) {
            try { page  = std::stoi(it->second); } catch (...) {}
        }
        if (auto it = params.find("limit"); it != params.end()) {
            try { limit = std::stoi(it->second); } catch (...) {}
        }
        if (auto it = params.find("open_only"); it != params.end()) {
            openOnly = (it->second == "true" || it->second == "1");
        }

        page  = std::max(1, page);
        limit = std::max(1, std::min(limit, 500));

        auto result = openOnly
            ? getStorage()->getOpenPortResults(sessionId, page, limit)
            : getStorage()->getResults(sessionId, page, limit);

        if (!result.success) {
            serverError(res, result.message);
            return;
        }

        json items = json::array();
        for (const auto& r : result.value) {
            items.push_back({
                {"ip",            r.ip},
                {"port",          r.port},
                {"open",          r.open},
                {"responseTimeMs", r.responseTimeMs},
                {"hostname",      r.hostname},
                {"cidr",          r.cidr},
                {"scannedAt",     r.scannedAt}
            });
        }

        json resp = {
            {"success", true},
            {"data", {
                {"sessionId", sessionId},
                {"page",      page},
                {"limit",     limit},
                {"openOnly",  openOnly},
                {"count",     static_cast<int>(result.value.size())},
                {"results",   items}
            }}
        };
        this->json(res, resp);

    } catch (const std::exception& e) {
        LOG_ERROR("getScanResults error: " + std::string(e.what()));
        serverError(res, "Failed to get scan results");
    }
}

// ---------------------------------------------------------------------------
// POST /api/cloudflare/scan/stop
// ---------------------------------------------------------------------------

void CloudflareScanController::stopScan(uWS::HttpResponse<false>* res,
                                         uWS::HttpRequest* /*req*/) {
    std::string buffer;
    res->onData([this, res, buffer = std::move(buffer)](
                    std::string_view data, bool last) mutable {
        buffer.append(data.data(), data.length());
        if (!last) return;

        try {
            if (!scanRunning_.load()) {
                json resp = {
                    {"success", false},
                    {"message", "No scan is currently running"}
                };
                this->json(res, resp, "400 Bad Request");
                return;
            }

            scanRunning_.store(false);

            std::string sessionId;
            {
                std::lock_guard<std::mutex> lk(sessionMutex_);
                sessionId = activeSessionId_;
            }

            json resp = {
                {"success",   true},
                {"message",   "Stop signal sent"},
                {"sessionId", sessionId}
            };
            this->json(res, resp);

        } catch (const std::exception& e) {
            LOG_ERROR("stopScan error: " + std::string(e.what()));
            serverError(res, "Failed to stop scan");
        }
    });
    res->onAborted([]() {
        LOG_WARNING("stopScan: client disconnected");
    });
}
