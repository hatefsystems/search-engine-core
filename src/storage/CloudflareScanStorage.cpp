#include "../../include/search_engine/storage/CloudflareScanStorage.h"
#include "../../include/Logger.h"
#include "../../include/mongodb.h"

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/exception/exception.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>
#include <bsoncxx/oid.hpp>

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;
using bsoncxx::builder::basic::document;
using bsoncxx::builder::basic::array;

namespace search_engine {
namespace storage {

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

CloudflareScanStorage::CloudflareScanStorage(const std::string& connectionString,
                                             const std::string& databaseName) {
    LOG_DEBUG("CloudflareScanStorage constructor called with database: " + databaseName);
    try {
        mongocxx::instance& instance = MongoDBInstance::getInstance();
        (void)instance;

        mongocxx::uri uri{connectionString};
        client_   = std::make_unique<mongocxx::client>(uri);
        database_ = (*client_)[databaseName];

        ensureIndexes();
        LOG_INFO("CloudflareScanStorage connected to MongoDB: " + databaseName);
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("Failed to initialize CloudflareScanStorage: " + std::string(e.what()));
        throw std::runtime_error("CloudflareScanStorage init failed: " +
                                 std::string(e.what()));
    }
}

// ---------------------------------------------------------------------------
// Collection accessors
// ---------------------------------------------------------------------------

mongocxx::collection CloudflareScanStorage::getSessions() {
    return database_["cloudflare_scan_sessions"];
}

mongocxx::collection CloudflareScanStorage::getResults() {
    return database_["cloudflare_scan_results"];
}

// ---------------------------------------------------------------------------
// Index creation
// ---------------------------------------------------------------------------

void CloudflareScanStorage::ensureIndexes() {
    try {
        auto sessions = getSessions();
        auto res  = getResults();

        // Sessions: index on status and startedAt
        sessions.create_index(make_document(kvp("status", 1)));
        sessions.create_index(make_document(kvp("startedAt", -1)));

        // Results: compound index on sessionId + open (for fast filtering)
        res.create_index(make_document(kvp("sessionId", 1), kvp("open", 1)));
        res.create_index(make_document(kvp("sessionId", 1), kvp("scannedAt", -1)));
    } catch (const mongocxx::exception& e) {
        LOG_WARNING("CloudflareScanStorage ensureIndexes warning: " + std::string(e.what()));
    }
}

// ---------------------------------------------------------------------------
// BSON conversions – sessions
// ---------------------------------------------------------------------------

bsoncxx::document::value CloudflareScanStorage::sessionToBson(
    const CloudflareScanSession& s) const {

    auto builder = bsoncxx::builder::basic::document{};
    builder.append(kvp("status",        s.status));
    builder.append(kvp("startedAt",     s.startedAt));
    builder.append(kvp("completedAt",   s.completedAt));
    builder.append(kvp("totalIPs",      s.totalIPs));
    builder.append(kvp("scannedIPs",    s.scannedIPs));
    builder.append(kvp("openPorts",     s.openPorts));
    builder.append(kvp("threads",       s.threads));
    builder.append(kvp("timeoutMs",     s.timeoutMs));
    builder.append(kvp("maxIPsPerRange", static_cast<int64_t>(s.maxIPsPerRange)));
    builder.append(kvp("errorMessage",  s.errorMessage));

    auto rangesArr = bsoncxx::builder::basic::array{};
    for (const auto& r : s.cidrRanges) {
        rangesArr.append(r);
    }
    builder.append(kvp("cidrRanges", rangesArr.extract()));

    return builder.extract();
}

CloudflareScanSession CloudflareScanStorage::bsonToSession(
    const bsoncxx::document::view& doc) const {

    CloudflareScanSession s;

    if (auto el = doc["_id"]) {
        s.id = el.get_oid().value.to_string();
    }
    if (auto el = doc["status"])       s.status       = std::string(el.get_string().value);
    if (auto el = doc["startedAt"])    s.startedAt    = std::string(el.get_string().value);
    if (auto el = doc["completedAt"])  s.completedAt  = std::string(el.get_string().value);
    if (auto el = doc["totalIPs"])     s.totalIPs     = el.get_int32().value;
    if (auto el = doc["scannedIPs"])   s.scannedIPs   = el.get_int32().value;
    if (auto el = doc["openPorts"])    s.openPorts    = el.get_int32().value;
    if (auto el = doc["threads"])      s.threads      = el.get_int32().value;
    if (auto el = doc["timeoutMs"])    s.timeoutMs    = el.get_int32().value;
    if (auto el = doc["maxIPsPerRange"]) {
        s.maxIPsPerRange = static_cast<size_t>(el.get_int64().value);
    }
    if (auto el = doc["errorMessage"]) s.errorMessage = std::string(el.get_string().value);
    if (auto el = doc["cidrRanges"]) {
        for (const auto& item : el.get_array().value) {
            s.cidrRanges.push_back(std::string(item.get_string().value));
        }
    }
    return s;
}

// ---------------------------------------------------------------------------
// BSON conversions – results
// ---------------------------------------------------------------------------

bsoncxx::document::value CloudflareScanStorage::resultToBson(
    const std::string& sessionId,
    const CloudflareScanResult& r) const {

    auto builder = bsoncxx::builder::basic::document{};
    builder.append(kvp("sessionId",     sessionId));
    builder.append(kvp("ip",            r.ip));
    builder.append(kvp("port",          r.port));
    builder.append(kvp("open",          r.open));
    builder.append(kvp("responseTimeMs", r.responseTimeMs));
    builder.append(kvp("hostname",      r.hostname));
    builder.append(kvp("cidr",          r.cidr));
    builder.append(kvp("scannedAt",     r.scannedAt));
    return builder.extract();
}

CloudflareScanResult CloudflareScanStorage::bsonToResult(
    const bsoncxx::document::view& doc) const {

    CloudflareScanResult r;
    if (auto el = doc["ip"])            r.ip            = std::string(el.get_string().value);
    if (auto el = doc["port"])          r.port          = el.get_int32().value;
    if (auto el = doc["open"])          r.open          = el.get_bool().value;
    if (auto el = doc["responseTimeMs"]) r.responseTimeMs = el.get_int32().value;
    if (auto el = doc["hostname"])      r.hostname      = std::string(el.get_string().value);
    if (auto el = doc["cidr"])          r.cidr          = std::string(el.get_string().value);
    if (auto el = doc["scannedAt"])     r.scannedAt     = std::string(el.get_string().value);
    return r;
}

// ---------------------------------------------------------------------------
// Session management
// ---------------------------------------------------------------------------

Result<std::string> CloudflareScanStorage::createSession(
    const CloudflareScanSession& session) {
    try {
        auto bsonDoc = sessionToBson(session);
        auto result  = getSessions().insert_one(bsonDoc.view());
        if (!result) {
            return Result<std::string>::Failure("insert_one returned no result");
        }
        const std::string id = result->inserted_id().get_oid().value.to_string();
        LOG_INFO("CloudflareScanStorage: created session " + id);
        return Result<std::string>::Success(id, "Session created");
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("CloudflareScanStorage::createSession: " + std::string(e.what()));
        return Result<std::string>::Failure(e.what());
    }
}

Result<bool> CloudflareScanStorage::updateSession(
    const std::string& sessionId,
    const CloudflareScanSession& session) {
    try {
        bsoncxx::oid oid{sessionId};
        auto filter = make_document(kvp("_id", oid));

        auto setFields = document{};
        setFields.append(kvp("status",        session.status));
        setFields.append(kvp("completedAt",   session.completedAt));
        setFields.append(kvp("scannedIPs",    session.scannedIPs));
        setFields.append(kvp("openPorts",     session.openPorts));
        setFields.append(kvp("totalIPs",      session.totalIPs));
        setFields.append(kvp("errorMessage",  session.errorMessage));

        auto updateDoc = document{};
        updateDoc.append(kvp("$set", setFields.extract()));

        auto result = getSessions().update_one(filter.view(), updateDoc.extract());
        if (!result || result->modified_count() == 0) {
            return Result<bool>::Failure("Session not found or not modified");
        }
        return Result<bool>::Success(true, "Session updated");
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("CloudflareScanStorage::updateSession: " + std::string(e.what()));
        return Result<bool>::Failure(e.what());
    }
}

Result<std::optional<CloudflareScanSession>> CloudflareScanStorage::getSession(
    const std::string& sessionId) {
    try {
        bsoncxx::oid oid{sessionId};
        auto filter = make_document(kvp("_id", oid));
        auto doc    = getSessions().find_one(filter.view());
        if (!doc.has_value()) {
            return Result<std::optional<CloudflareScanSession>>::Success(
                std::nullopt, "Not found");
        }
        return Result<std::optional<CloudflareScanSession>>::Success(
            bsonToSession(doc->view()), "Found");
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("CloudflareScanStorage::getSession: " + std::string(e.what()));
        return Result<std::optional<CloudflareScanSession>>::Failure(e.what());
    }
}

Result<std::vector<CloudflareScanSession>> CloudflareScanStorage::listSessions(
    int limit, int skip) {
    try {
        mongocxx::options::find opts;
        opts.limit(limit);
        opts.skip(skip);
        opts.sort(make_document(kvp("startedAt", -1)));

        auto cursor = getSessions().find({}, opts);
        std::vector<CloudflareScanSession> sessions;
        for (const auto& doc : cursor) {
            sessions.push_back(bsonToSession(doc));
        }
        return Result<std::vector<CloudflareScanSession>>::Success(
            sessions, "Listed sessions");
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("CloudflareScanStorage::listSessions: " + std::string(e.what()));
        return Result<std::vector<CloudflareScanSession>>::Failure(e.what());
    }
}

// ---------------------------------------------------------------------------
// Result management
// ---------------------------------------------------------------------------

Result<bool> CloudflareScanStorage::storeResult(const std::string& sessionId,
                                                  const CloudflareScanResult& result) {
    try {
        auto bsonDoc  = resultToBson(sessionId, result);
        auto insertResult = getResults().insert_one(bsonDoc.view());
        if (!insertResult) {
            return Result<bool>::Failure("insert_one returned no result");
        }
        return Result<bool>::Success(true, "Result stored");
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("CloudflareScanStorage::storeResult: " + std::string(e.what()));
        return Result<bool>::Failure(e.what());
    }
}

Result<std::vector<CloudflareScanResult>> CloudflareScanStorage::getResults(
    const std::string& sessionId, int page, int limit) {
    try {
        auto filter = make_document(kvp("sessionId", sessionId));

        mongocxx::options::find opts;
        opts.limit(limit);
        opts.skip((page - 1) * limit);
        opts.sort(make_document(kvp("scannedAt", -1)));

        auto cursor = getResults().find(filter.view(), opts);
        std::vector<CloudflareScanResult> results;
        for (const auto& doc : cursor) {
            results.push_back(bsonToResult(doc));
        }
        return Result<std::vector<CloudflareScanResult>>::Success(
            results, "Results fetched");
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("CloudflareScanStorage::getResults: " + std::string(e.what()));
        return Result<std::vector<CloudflareScanResult>>::Failure(e.what());
    }
}

Result<std::vector<CloudflareScanResult>> CloudflareScanStorage::getOpenPortResults(
    const std::string& sessionId, int page, int limit) {
    try {
        auto filter = make_document(kvp("sessionId", sessionId), kvp("open", true));

        mongocxx::options::find opts;
        opts.limit(limit);
        opts.skip((page - 1) * limit);
        opts.sort(make_document(kvp("scannedAt", -1)));

        auto cursor = getResults().find(filter.view(), opts);
        std::vector<CloudflareScanResult> results;
        for (const auto& doc : cursor) {
            results.push_back(bsonToResult(doc));
        }
        return Result<std::vector<CloudflareScanResult>>::Success(
            results, "Open-port results fetched");
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("CloudflareScanStorage::getOpenPortResults: " + std::string(e.what()));
        return Result<std::vector<CloudflareScanResult>>::Failure(e.what());
    }
}

// ---------------------------------------------------------------------------
// Connection test
// ---------------------------------------------------------------------------

Result<bool> CloudflareScanStorage::testConnection() {
    try {
        auto pingResult = database_.run_command(make_document(kvp("ping", 1)));
        if (pingResult.empty()) {
            return Result<bool>::Failure("Ping returned empty result");
        }
        return Result<bool>::Success(true, "Connection OK");
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("CloudflareScanStorage::testConnection: " + std::string(e.what()));
        return Result<bool>::Failure(e.what());
    }
}

} // namespace storage
} // namespace search_engine
