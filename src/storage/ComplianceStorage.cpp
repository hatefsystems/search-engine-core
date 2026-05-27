#include "../../include/search_engine/storage/LegalComplianceLog.h"
#include "../../include/search_engine/storage/DataEncryption.h"
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

bsoncxx::types::b_date ComplianceStorage::timePointToDate(
    const std::chrono::system_clock::time_point& tp) {
    auto duration = tp.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    return bsoncxx::types::b_date{millis};
}

std::chrono::system_clock::time_point ComplianceStorage::dateToTimePoint(
    const bsoncxx::types::b_date& date) {
    return std::chrono::system_clock::time_point{date.value};
}

ComplianceStorage::ComplianceStorage(
    const std::string& connectionString,
    const std::string& databaseName) {
    
    LOG_DEBUG("ComplianceStorage constructor called");
    
    try {
        // Load encryption key from environment
        encryptionKey_ = DataEncryption::getEncryptionKey();
        LOG_INFO("Encryption key loaded successfully");
        
        // Use MONGODB_COMPLIANCE_URI if available, otherwise fall back to MONGODB_URI
        std::string actualConnectionString = connectionString;
        const char* complianceUri = std::getenv("MONGODB_COMPLIANCE_URI");
        if (complianceUri) {
            actualConnectionString = std::string(complianceUri);
            LOG_INFO("Using MONGODB_COMPLIANCE_URI for compliance database");
        } else {
            const char* envUri = std::getenv("MONGODB_URI");
            if (envUri) {
                actualConnectionString = std::string(envUri);
                LOG_DEBUG("Using MONGODB_URI for compliance database");
            }
        }
        
        LOG_INFO("Initializing compliance MongoDB connection to: " + actualConnectionString);
        
        // Use existing MongoDB instance singleton
        mongocxx::instance& instance = MongoDBInstance::getInstance();
        (void)instance;
        
        // Create client and connect to database
        mongocxx::uri uri{actualConnectionString};
        client_ = std::make_unique<mongocxx::client>(uri);
        database_ = (*client_)[databaseName];
        complianceCollection_ = database_["legal_compliance_logs"];
        
        LOG_INFO("Connected to compliance database: " + databaseName);
        
        // Ensure indexes are created
        ensureIndexes();
        LOG_DEBUG("Compliance indexes ensured");
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("Failed to initialize ComplianceStorage: " + std::string(e.what()));
        throw std::runtime_error("Failed to initialize ComplianceStorage: " + std::string(e.what()));
    } catch (const std::runtime_error& e) {
        LOG_ERROR("Failed to load encryption key: " + std::string(e.what()));
        throw;
    }
}

void ComplianceStorage::ensureIndexes() {
    try {
        using bsoncxx::builder::basic::kvp;
        using bsoncxx::builder::basic::make_document;
        
        // 1. user_compliance_history: Index on userId + timestamp for user compliance history
        {
            mongocxx::options::index opts{};
            opts.name("user_compliance_history");
            auto userTimestampIndex = make_document(
                kvp("userId", 1),
                kvp("timestamp", -1)
            );
            complianceCollection_.create_index(userTimestampIndex.view(), opts);
        }
        
        // 2. auto_deletion_index: Index on retentionExpiry for auto-deletion queries
        {
            mongocxx::options::index opts{};
            opts.name("auto_deletion_index");
            auto expiryIndex = make_document(kvp("retentionExpiry", 1));
            complianceCollection_.create_index(expiryIndex.view(), opts);
        }
        
        // 3. analytics_link: Index on viewId for linking to Tier 1 analytics
        {
            mongocxx::options::index opts{};
            opts.name("analytics_link");
            auto viewIdIndex = make_document(kvp("viewId", 1));
            complianceCollection_.create_index(viewIdIndex.view(), opts);
        }
        
        // Index on timestamp for audit queries (unnamed helper index)
        {
            auto timestampIndex = make_document(kvp("timestamp", -1));
            complianceCollection_.create_index(timestampIndex.view());
        }
        
        LOG_INFO("ComplianceStorage indexes created successfully with named indexes");
        
    } catch (const mongocxx::exception& e) {
        LOG_WARNING("Failed to create compliance indexes (may already exist): " + std::string(e.what()));
    }
}

bsoncxx::document::value ComplianceStorage::logToBson(const LegalComplianceLog& log) const {
    using bsoncxx::builder::basic::kvp;
    using bsoncxx::builder::basic::make_document;
    
    auto builder = bsoncxx::builder::basic::document{};
    
    builder.append(kvp("logId", log.logId));
    builder.append(kvp("userId", log.userId));
    builder.append(kvp("timestamp", timePointToDate(log.timestamp)));
    
    // Store encrypted sensitive data as-is (already encrypted)
    builder.append(kvp("ipAddress_encrypted", log.ipAddress_encrypted));
    builder.append(kvp("userAgent_encrypted", log.userAgent_encrypted));
    builder.append(kvp("referrer_encrypted", log.referrer_encrypted));
    
    // Link to Tier 1 analytics
    builder.append(kvp("viewId", log.viewId));
    
    // Auto-management fields
    builder.append(kvp("retentionExpiry", timePointToDate(log.retentionExpiry)));
    builder.append(kvp("isUnderInvestigation", log.isUnderInvestigation));
    
    return builder.extract();
}

LegalComplianceLog ComplianceStorage::bsonToLog(const bsoncxx::document::view& doc) const {
    LegalComplianceLog log;
    
    log.logId = std::string(doc["logId"].get_string().value);
    log.userId = std::string(doc["userId"].get_string().value);
    log.timestamp = dateToTimePoint(doc["timestamp"].get_date());
    
    // Encrypted fields (remain encrypted in memory for security)
    log.ipAddress_encrypted = std::string(doc["ipAddress_encrypted"].get_string().value);
    log.userAgent_encrypted = std::string(doc["userAgent_encrypted"].get_string().value);
    log.referrer_encrypted = std::string(doc["referrer_encrypted"].get_string().value);
    
    log.viewId = std::string(doc["viewId"].get_string().value);
    
    log.retentionExpiry = dateToTimePoint(doc["retentionExpiry"].get_date());
    log.isUnderInvestigation = doc["isUnderInvestigation"].get_bool().value;
    
    return log;
}

Result<std::string> ComplianceStorage::recordLog(const LegalComplianceLog& log) {
    try {
        auto doc = logToBson(log);
        auto result = complianceCollection_.insert_one(doc.view());
        
        if (result) {
            LOG_INFO("Recorded compliance log: logId=" + log.logId + ", viewId=" + log.viewId);
            return Result<std::string>::Success(log.logId, "Compliance log recorded successfully");
        } else {
            LOG_ERROR("Failed to record compliance log");
            return Result<std::string>::Failure("Failed to record compliance log");
        }
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error recording compliance log: " + std::string(e.what()));
        return Result<std::string>::Failure("Database error: " + std::string(e.what()));
    }
}

Result<int64_t> ComplianceStorage::deleteExpiredLogs() {
    try {
        auto now = std::chrono::system_clock::now();
        
        // Query: retentionExpiry < now AND isUnderInvestigation = false
        auto filter = document{}
            << "retentionExpiry" << bsoncxx::builder::stream::open_document
                << "$lt" << timePointToDate(now)
            << bsoncxx::builder::stream::close_document
            << "isUnderInvestigation" << false
            << finalize;
        
        // Count before deletion for audit
        int64_t expiredCount = complianceCollection_.count_documents(filter.view());
        
        if (expiredCount == 0) {
            LOG_INFO("No expired compliance logs to delete");
            return Result<int64_t>::Success(0, "No expired logs");
        }
        
        // Delete expired logs
        auto deleteResult = complianceCollection_.delete_many(filter.view());
        
        if (deleteResult) {
            int64_t deletedCount = deleteResult->deleted_count();
            LOG_INFO("Auto-deleted " + std::to_string(deletedCount) + " expired compliance logs");
            return Result<int64_t>::Success(deletedCount, 
                "Deleted " + std::to_string(deletedCount) + " expired logs");
        } else {
            LOG_ERROR("Failed to delete expired compliance logs");
            return Result<int64_t>::Failure("Failed to delete expired logs");
        }
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error deleting expired logs: " + std::string(e.what()));
        return Result<int64_t>::Failure("Database error: " + std::string(e.what()));
    }
}

Result<bool> ComplianceStorage::markUnderInvestigation(const std::string& logId) {
    try {
        auto filter = document{} << "logId" << logId << finalize;
        
        using bsoncxx::builder::basic::kvp;
        auto update = bsoncxx::builder::basic::document{};
        auto setFields = bsoncxx::builder::basic::document{};
        setFields.append(kvp("isUnderInvestigation", true));
        update.append(kvp("$set", setFields.extract()));
        
        auto result = complianceCollection_.update_one(filter.view(), update.extract().view());
        
        if (result && result->modified_count() > 0) {
            LOG_INFO("Marked compliance log under investigation: " + logId);
            return Result<bool>::Success(true, "Log marked under investigation");
        } else {
            LOG_WARNING("Failed to mark log under investigation (not found?): " + logId);
            return Result<bool>::Failure("Log not found");
        }
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error marking log: " + std::string(e.what()));
        return Result<bool>::Failure("Database error: " + std::string(e.what()));
    }
}

Result<int64_t> ComplianceStorage::countLogs() {
    try {
        int64_t count = complianceCollection_.count_documents({});
        LOG_INFO("Total compliance logs: " + std::to_string(count));
        return Result<int64_t>::Success(count, "Count retrieved successfully");
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error counting logs: " + std::string(e.what()));
        return Result<int64_t>::Failure("Database error: " + std::string(e.what()));
    }
}

Result<int64_t> ComplianceStorage::countExpiredLogs() {
    try {
        auto now = std::chrono::system_clock::now();
        
        auto filter = document{}
            << "retentionExpiry" << bsoncxx::builder::stream::open_document
                << "$lt" << timePointToDate(now)
            << bsoncxx::builder::stream::close_document
            << "isUnderInvestigation" << false
            << finalize;
        
        int64_t count = complianceCollection_.count_documents(filter.view());
        LOG_INFO("Expired compliance logs: " + std::to_string(count));
        return Result<int64_t>::Success(count, "Expired count retrieved successfully");
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error counting expired logs: " + std::string(e.what()));
        return Result<int64_t>::Failure("Database error: " + std::string(e.what()));
    }
}

} // namespace storage
} // namespace search_engine
