#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <bsoncxx/document/value.hpp>
#include <bsoncxx/document/view.hpp>
#include "../../infrastructure.h"

namespace search_engine {
namespace storage {

// -----------------------------------------------------------------------
// Data structures
// -----------------------------------------------------------------------

struct CloudflareScanResult {
    std::string ip;
    int port = 443;
    bool open = false;
    int responseTimeMs = 0;
    std::string hostname;  // from TLS certificate, empty if unavailable
    std::string cidr;      // CIDR range this IP belongs to
    std::string scannedAt; // ISO-8601 UTC timestamp
};

struct CloudflareScanSession {
    std::string id;          // MongoDB ObjectId (empty until persisted)
    std::string startedAt;   // ISO-8601 UTC
    std::string completedAt; // ISO-8601 UTC (empty while running)
    std::string status;      // "running" | "completed" | "stopped" | "error"
    std::vector<std::string> cidrRanges; // ranges included in this session
    int totalIPs = 0;        // estimated IPs to scan
    int scannedIPs = 0;      // IPs probed so far
    int openPorts = 0;       // open port 443 hits
    int threads = 10;
    int timeoutMs = 3000;
    size_t maxIPsPerRange = 256; // maximum IPs probed per CIDR block
    std::string errorMessage;
};

// -----------------------------------------------------------------------
// Storage class
// -----------------------------------------------------------------------

class CloudflareScanStorage {
public:
    explicit CloudflareScanStorage(
        const std::string& connectionString = "mongodb://localhost:27017",
        const std::string& databaseName = "search-engine");

    ~CloudflareScanStorage() = default;

    CloudflareScanStorage(CloudflareScanStorage&&) noexcept = default;
    CloudflareScanStorage& operator=(CloudflareScanStorage&&) noexcept = default;
    CloudflareScanStorage(const CloudflareScanStorage&) = delete;
    CloudflareScanStorage& operator=(const CloudflareScanStorage&) = delete;

    // Session management
    Result<std::string>                     createSession(const CloudflareScanSession& session);
    Result<bool>                            updateSession(const std::string& sessionId,
                                                          const CloudflareScanSession& session);
    Result<std::optional<CloudflareScanSession>> getSession(const std::string& sessionId);
    Result<std::vector<CloudflareScanSession>>   listSessions(int limit = 20, int skip = 0);

    // Result management
    Result<bool>                            storeResult(const std::string& sessionId,
                                                        const CloudflareScanResult& result);
    Result<std::vector<CloudflareScanResult>> getResults(const std::string& sessionId,
                                                          int page = 1, int limit = 100);
    Result<std::vector<CloudflareScanResult>> getOpenPortResults(
        const std::string& sessionId, int page = 1, int limit = 100);

    Result<bool> testConnection();

private:
    std::unique_ptr<mongocxx::client> client_;
    mongocxx::database database_;

    // Collection accessors (created lazily, cached after first access)
    mongocxx::collection getSessions();
    mongocxx::collection getResults();

    void ensureIndexes();

    // BSON conversion helpers
    bsoncxx::document::value sessionToBson(const CloudflareScanSession& s) const;
    CloudflareScanSession    bsonToSession(const bsoncxx::document::view& doc) const;

    bsoncxx::document::value resultToBson(const std::string& sessionId,
                                          const CloudflareScanResult& r) const;
    CloudflareScanResult     bsonToResult(const bsoncxx::document::view& doc) const;
};

} // namespace storage
} // namespace search_engine
