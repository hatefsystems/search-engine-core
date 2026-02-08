#pragma once

#include "../../infrastructure.h"
#include <string>
#include <chrono>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <memory>

namespace search_engine {
namespace storage {

/**
 * @brief Audit action types
 */
enum class AuditAction {
    CREATE,
    UPDATE,
    DELETE,
    VIEW
};

// Helper to convert AuditAction to string
std::string auditActionToString(AuditAction action);
AuditAction stringToAuditAction(const std::string& str);

/**
 * @brief Profile audit log for CRUD and view operations
 * 
 * Records who did what, when, and why for compliance and debugging.
 * Separate from LegalComplianceLog (which stores encrypted IP/user-agent).
 */
struct ProfileAuditLog {
    std::string id;                                         // Unique audit log ID
    std::chrono::system_clock::time_point timestamp;        // When action occurred
    
    // Action details
    AuditAction action;                                     // CREATE, UPDATE, DELETE, VIEW
    std::string resourceType = "profile";                   // Resource type (always "profile" for now)
    std::string resourceId;                                 // Profile ID affected
    
    // Actor information
    std::string userId;                                     // Who performed the action (or "anonymous")
    std::string ipAddress;                                  // IP address of requester
    std::string userAgent;                                  // User agent string
    
    // Change details (for UPDATE)
    std::string oldValue;                                   // JSON of old state (empty for CREATE/DELETE/VIEW)
    std::string newValue;                                   // JSON of new state (empty for DELETE/VIEW)
    std::string reason;                                     // Optional reason for action
    
    // Metadata
    std::string sessionId;                                  // Session ID if available
    std::string apiVersion = "v1";                          // API version
    bool isAutomated = false;                               // Was this done by system?
};

/**
 * @brief Storage for profile audit logs
 * 
 * Collection: profile_audit_logs
 * Indexes: resourceId+timestamp, userId+timestamp
 */
class AuditStorage {
private:
    std::unique_ptr<mongocxx::client> client_;
    mongocxx::database database_;
    mongocxx::collection auditCollection_;
    
    // Ensure indexes are created
    void ensureIndexes();
    
    // BSON conversion helpers
    bsoncxx::document::value logToBson(const ProfileAuditLog& log) const;
    ProfileAuditLog bsonToLog(const bsoncxx::document::view& doc) const;
    
    // Time conversion helpers
    static bsoncxx::types::b_date timePointToDate(const std::chrono::system_clock::time_point& tp);
    static std::chrono::system_clock::time_point dateToTimePoint(const bsoncxx::types::b_date& date);
    
public:
    /**
     * @brief Constructor with MongoDB connection
     * @param connectionString MongoDB URI (from MONGODB_URI env)
     * @param databaseName Database name (default: search-engine)
     */
    explicit AuditStorage(
        const std::string& connectionString = "mongodb://localhost:27017",
        const std::string& databaseName = "search-engine"
    );
    
    ~AuditStorage() = default;
    
    // Prevent copying (RAII pattern)
    AuditStorage(const AuditStorage&) = delete;
    AuditStorage& operator=(const AuditStorage&) = delete;
    
    // Allow moving
    AuditStorage(AuditStorage&&) = default;
    AuditStorage& operator=(AuditStorage&&) = default;
    
    /**
     * @brief Record an audit log entry
     * @param log Audit log to record
     * @return Result with log ID
     */
    Result<std::string> recordAudit(const ProfileAuditLog& log);
    
    /**
     * @brief Get audit logs for a specific resource (profile)
     * @param resourceId Profile ID
     * @param limit Maximum number of logs to return
     * @return Result with vector of audit logs
     */
    Result<std::vector<ProfileAuditLog>> getLogsForResource(
        const std::string& resourceId,
        int limit = 100
    );
    
    /**
     * @brief Get audit logs for a specific user
     * @param userId User ID
     * @param limit Maximum number of logs to return
     * @return Result with vector of audit logs
     */
    Result<std::vector<ProfileAuditLog>> getLogsForUser(
        const std::string& userId,
        int limit = 100
    );
};

} // namespace storage
} // namespace search_engine
