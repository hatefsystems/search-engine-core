#pragma once

#include "../../infrastructure.h"
#include <string>
#include <chrono>
#include <vector>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <memory>

namespace search_engine {
namespace storage {

/**
 * @brief Tier 2 Compliance - Legal requirement tracking (ENCRYPTED)
 * 
 * What gets stored (ALL ENCRYPTED):
 * - IP addresses (AES-256-GCM encrypted)
 * - User-Agent strings (encrypted)
 * - Referrer URLs (encrypted)
 * - Link to Tier 1 analytics (viewId)
 * 
 * Retention:
 * - 12 months auto-deletion
 * - Can be held longer if under investigation
 * 
 * Access:
 * - Protected by API key
 * - All access audited
 * - 2FA for admin UI (future)
 */
struct LegalComplianceLog {
    std::string logId;              // Unique log identifier
    std::string userId;             // Profile owner or viewer (if logged in)
    std::chrono::system_clock::time_point timestamp;
    
    // Encrypted sensitive data (AES-256-GCM)
    std::string ipAddress_encrypted;
    std::string userAgent_encrypted;
    std::string referrer_encrypted;
    
    // Link to Tier 1 analytics (for correlation if needed)
    std::string viewId;
    
    // Auto-management
    std::chrono::system_clock::time_point retentionExpiry;  // timestamp + 12 months
    bool isUnderInvestigation = false;  // If true, don't auto-delete
};

/**
 * @brief Storage for Tier 2 compliance logs (encrypted legal data)
 * 
 * Collection: legal_compliance_logs
 * Database: Configurable via MONGODB_COMPLIANCE_URI (separate instance for strict isolation)
 *           Default: same database as profiles (search-engine)
 * 
 * Security:
 * - All sensitive fields encrypted with AES-256-GCM
 * - Access restricted to internal APIs with API key
 * - Audit trail for all compliance data access
 * - Auto-deletion after 12 months (unless under investigation)
 */
class ComplianceStorage {
private:
    std::unique_ptr<mongocxx::client> client_;
    mongocxx::database database_;
    mongocxx::collection complianceCollection_;
    
    std::string encryptionKey_;  // Loaded from env once
    
    // Ensure indexes are created
    void ensureIndexes();
    
    // BSON conversion helpers
    bsoncxx::document::value logToBson(const LegalComplianceLog& log) const;
    LegalComplianceLog bsonToLog(const bsoncxx::document::view& doc) const;
    
    // Time conversion helpers
    static bsoncxx::types::b_date timePointToDate(const std::chrono::system_clock::time_point& tp);
    static std::chrono::system_clock::time_point dateToTimePoint(const bsoncxx::types::b_date& date);
    
public:
    /**
     * @brief Constructor with MongoDB connection
     * @param connectionString MongoDB URI (from MONGODB_COMPLIANCE_URI or MONGODB_URI env)
     * @param databaseName Database name (default: search-engine)
     */
    explicit ComplianceStorage(
        const std::string& connectionString = "mongodb://localhost:27017",
        const std::string& databaseName = "search-engine"
    );
    
    ~ComplianceStorage() = default;
    
    // Prevent copying (RAII pattern)
    ComplianceStorage(const ComplianceStorage&) = delete;
    ComplianceStorage& operator=(const ComplianceStorage&) = delete;
    
    // Allow moving
    ComplianceStorage(ComplianceStorage&&) = default;
    ComplianceStorage& operator=(ComplianceStorage&&) = default;
    
    /**
     * @brief Record a compliance log entry (Tier 2)
     * @param log Compliance log (sensitive fields will be encrypted)
     * @return Result with success status and message
     */
    Result<std::string> recordLog(const LegalComplianceLog& log);
    
    /**
     * @brief Delete expired compliance logs (auto-deletion job)
     * @return Result with number of logs deleted
     * 
     * Deletes logs where:
     * - retentionExpiry < now()
     * - isUnderInvestigation = false
     */
    Result<int64_t> deleteExpiredLogs();
    
    /**
     * @brief Mark a log as under investigation (prevents auto-deletion)
     * @param logId Log to mark
     * @return Result with success status
     */
    Result<bool> markUnderInvestigation(const std::string& logId);
    
    /**
     * @brief Count total compliance logs
     * @return Result with log count
     */
    Result<int64_t> countLogs();
    
    /**
     * @brief Count expired logs (for audit before deletion)
     * @return Result with expired log count
     */
    Result<int64_t> countExpiredLogs();
};

} // namespace storage
} // namespace search_engine
