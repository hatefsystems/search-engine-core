#include "../../include/search_engine/storage/ProfileAuditLog.h"
#include "../../include/Logger.h"
#include "../../include/mongodb.h"
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/exception/exception.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/helpers.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>

using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;

namespace search_engine {
namespace storage {

// Helper functions for AuditAction enum
std::string auditActionToString(AuditAction action) {
    switch (action) {
        case AuditAction::CREATE: return "CREATE";
        case AuditAction::UPDATE: return "UPDATE";
        case AuditAction::DELETE: return "DELETE";
        case AuditAction::VIEW: return "VIEW";
        default: return "UNKNOWN";
    }
}

AuditAction stringToAuditAction(const std::string& str) {
    if (str == "CREATE") return AuditAction::CREATE;
    if (str == "UPDATE") return AuditAction::UPDATE;
    if (str == "DELETE") return AuditAction::DELETE;
    if (str == "VIEW") return AuditAction::VIEW;
    return AuditAction::VIEW; // Default
}

// Time conversion helpers
bsoncxx::types::b_date AuditStorage::timePointToDate(
    const std::chrono::system_clock::time_point& tp) {
    auto duration = tp.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    return bsoncxx::types::b_date{millis};
}

std::chrono::system_clock::time_point AuditStorage::dateToTimePoint(
    const bsoncxx::types::b_date& date) {
    return std::chrono::system_clock::time_point{date.value};
}

AuditStorage::AuditStorage(
    const std::string& connectionString,
    const std::string& databaseName) {
    
    LOG_DEBUG("AuditStorage constructor called");
    
    try {
        // Use MONGODB_URI environment variable if available
        std::string actualConnectionString = connectionString;
        const char* envUri = std::getenv("MONGODB_URI");
        if (envUri) {
            actualConnectionString = std::string(envUri);
            LOG_DEBUG("Using MONGODB_URI from environment: " + actualConnectionString);
        }
        
        LOG_INFO("Initializing audit MongoDB connection to: " + actualConnectionString);
        
        // Use existing MongoDB instance singleton
        mongocxx::instance& instance = MongoDBInstance::getInstance();
        (void)instance;
        
        // Create client and connect to database
        mongocxx::uri uri{actualConnectionString};
        client_ = std::make_unique<mongocxx::client>(uri);
        database_ = (*client_)[databaseName];
        auditCollection_ = database_["profile_audit_logs"];
        
        LOG_INFO("Connected to audit database: " + databaseName);
        
        // Ensure indexes are created
        ensureIndexes();
        LOG_DEBUG("Audit indexes ensured");
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("Failed to initialize AuditStorage: " + std::string(e.what()));
        throw std::runtime_error("Failed to initialize AuditStorage: " + std::string(e.what()));
    }
}

void AuditStorage::ensureIndexes() {
    try {
        using bsoncxx::builder::basic::kvp;
        using bsoncxx::builder::basic::make_document;
        
        // 1. audit_resource_timeline: Index on resourceId + timestamp
        {
            mongocxx::options::index opts{};
            opts.name("audit_resource_timeline");
            auto resourceTimestampIndex = make_document(
                kvp("resourceId", 1),
                kvp("timestamp", -1)
            );
            auditCollection_.create_index(resourceTimestampIndex.view(), opts);
        }
        
        // 2. audit_user_timeline: Index on userId + timestamp
        {
            mongocxx::options::index opts{};
            opts.name("audit_user_timeline");
            auto userTimestampIndex = make_document(
                kvp("userId", 1),
                kvp("timestamp", -1)
            );
            auditCollection_.create_index(userTimestampIndex.view(), opts);
        }
        
        // 3. audit_action: Index on action for filtering by action type
        {
            mongocxx::options::index opts{};
            opts.name("audit_action");
            auto actionIndex = make_document(kvp("action", 1));
            auditCollection_.create_index(actionIndex.view(), opts);
        }
        
        LOG_INFO("AuditStorage indexes created successfully with named indexes");
        
    } catch (const mongocxx::exception& e) {
        LOG_WARNING("Failed to create audit indexes (may already exist): " + std::string(e.what()));
    }
}

bsoncxx::document::value AuditStorage::logToBson(const ProfileAuditLog& log) const {
    using bsoncxx::builder::basic::kvp;
    using bsoncxx::builder::basic::make_document;
    
    auto builder = bsoncxx::builder::basic::document{};
    
    // Basic fields
    if (!log.id.empty()) {
        builder.append(kvp("_id", log.id));
    }
    
    builder.append(kvp("timestamp", timePointToDate(log.timestamp)));
    builder.append(kvp("action", auditActionToString(log.action)));
    builder.append(kvp("resourceType", log.resourceType));
    builder.append(kvp("resourceId", log.resourceId));
    
    // Actor information
    builder.append(kvp("userId", log.userId));
    builder.append(kvp("ipAddress", log.ipAddress));
    builder.append(kvp("userAgent", log.userAgent));
    
    // Change details
    builder.append(kvp("oldValue", log.oldValue));
    builder.append(kvp("newValue", log.newValue));
    builder.append(kvp("reason", log.reason));
    
    // Metadata
    builder.append(kvp("sessionId", log.sessionId));
    builder.append(kvp("apiVersion", log.apiVersion));
    builder.append(kvp("isAutomated", log.isAutomated));
    
    return builder.extract();
}

ProfileAuditLog AuditStorage::bsonToLog(const bsoncxx::document::view& doc) const {
    ProfileAuditLog log;
    
    // Basic fields
    if (doc["_id"]) {
        log.id = std::string(doc["_id"].get_string().value);
    }
    
    log.timestamp = dateToTimePoint(doc["timestamp"].get_date());
    log.action = stringToAuditAction(std::string(doc["action"].get_string().value));
    log.resourceType = std::string(doc["resourceType"].get_string().value);
    log.resourceId = std::string(doc["resourceId"].get_string().value);
    
    // Actor information
    log.userId = std::string(doc["userId"].get_string().value);
    log.ipAddress = std::string(doc["ipAddress"].get_string().value);
    log.userAgent = std::string(doc["userAgent"].get_string().value);
    
    // Change details
    log.oldValue = std::string(doc["oldValue"].get_string().value);
    log.newValue = std::string(doc["newValue"].get_string().value);
    log.reason = std::string(doc["reason"].get_string().value);
    
    // Metadata
    log.sessionId = std::string(doc["sessionId"].get_string().value);
    log.apiVersion = std::string(doc["apiVersion"].get_string().value);
    log.isAutomated = doc["isAutomated"].get_bool().value;
    
    return log;
}

Result<std::string> AuditStorage::recordAudit(const ProfileAuditLog& log) {
    try {
        auto doc = logToBson(log);
        auto result = auditCollection_.insert_one(doc.view());
        
        if (result) {
            // The ID is already a string in the log, use it directly
            std::string id = log.id.empty() ? "generated" : log.id;
            LOG_DEBUG("Recorded audit log: action=" + auditActionToString(log.action) + 
                     ", resourceId=" + log.resourceId + ", userId=" + log.userId);
            return Result<std::string>::Success(id, "Audit log recorded successfully");
        } else {
            LOG_ERROR("Failed to record audit log");
            return Result<std::string>::Failure("Failed to record audit log");
        }
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error recording audit log: " + std::string(e.what()));
        return Result<std::string>::Failure("Database error: " + std::string(e.what()));
    }
}

Result<std::vector<ProfileAuditLog>> AuditStorage::getLogsForResource(
    const std::string& resourceId,
    int limit) {
    
    try {
        using bsoncxx::builder::basic::kvp;
        using bsoncxx::builder::basic::make_document;
        
        // Query: filter by resourceId, sort by timestamp descending
        auto filter = make_document(kvp("resourceId", resourceId));
        
        mongocxx::options::find opts{};
        opts.sort(make_document(kvp("timestamp", -1)));
        opts.limit(limit);
        
        auto cursor = auditCollection_.find(filter.view(), opts);
        
        std::vector<ProfileAuditLog> logs;
        for (const auto& doc : cursor) {
            logs.push_back(bsonToLog(doc));
        }
        
        return Result<std::vector<ProfileAuditLog>>::Success(
            logs,
            "Found " + std::to_string(logs.size()) + " audit logs"
        );
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error fetching audit logs: " + std::string(e.what()));
        return Result<std::vector<ProfileAuditLog>>::Failure(
            "Database error: " + std::string(e.what())
        );
    }
}

Result<std::vector<ProfileAuditLog>> AuditStorage::getLogsForUser(
    const std::string& userId,
    int limit) {
    
    try {
        using bsoncxx::builder::basic::kvp;
        using bsoncxx::builder::basic::make_document;
        
        // Query: filter by userId, sort by timestamp descending
        auto filter = make_document(kvp("userId", userId));
        
        mongocxx::options::find opts{};
        opts.sort(make_document(kvp("timestamp", -1)));
        opts.limit(limit);
        
        auto cursor = auditCollection_.find(filter.view(), opts);
        
        std::vector<ProfileAuditLog> logs;
        for (const auto& doc : cursor) {
            logs.push_back(bsonToLog(doc));
        }
        
        return Result<std::vector<ProfileAuditLog>>::Success(
            logs,
            "Found " + std::to_string(logs.size()) + " audit logs"
        );
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error fetching user audit logs: " + std::string(e.what()));
        return Result<std::vector<ProfileAuditLog>>::Failure(
            "Database error: " + std::string(e.what())
        );
    }
}

} // namespace storage
} // namespace search_engine
