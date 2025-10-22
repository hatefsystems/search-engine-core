#pragma once

#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/instance.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/document/view.hpp>
#include <bsoncxx/oid.hpp>
#include <chrono>
#include <string>
#include <memory>
#include <vector>

namespace search_engine::storage {

/**
 * Email Logs Storage - Manages email sending logs in MongoDB
 * Tracks all email attempts with status, timestamps, and details
 */
class EmailLogsStorage {
public:
    // Email log status enumeration
    enum class EmailStatus {
        QUEUED = 0,     // Email queued for sending
        SENT = 1,       // Email sent successfully
        FAILED = 2,     // Email failed to send
        PENDING = 3     // Email is being processed
    };

    // Email log data structure
    struct EmailLog {
        std::string id;                    // MongoDB ObjectId as string
        std::string toEmail;               // Recipient email address
        std::string fromEmail;             // Sender email address
        std::string recipientName;         // Recipient name
        std::string domainName;            // Domain name (for crawling notifications)
        std::string subject;               // Email subject
        std::string language;              // Email language code
        std::string emailType;             // Type of email (crawling_notification, generic, etc.)
        EmailStatus status;                // Current status
        std::string errorMessage;          // Error message if failed
        std::string crawlSessionId;        // Crawl session ID (for crawling notifications)
        int crawledPagesCount;             // Number of pages crawled (for crawling notifications)
        
        // Timestamps
        std::chrono::system_clock::time_point queuedAt;    // When email was queued
        std::chrono::system_clock::time_point sentAt;      // When email was sent (if successful)
        std::chrono::system_clock::time_point failedAt;    // When email failed (if failed)
        
        // Constructor for easy initialization
        EmailLog() : status(EmailStatus::QUEUED), crawledPagesCount(0) {}
    };

    EmailLogsStorage();
    ~EmailLogsStorage() = default;

    // Database operations
    bool initializeDatabase();
    
    // Email log CRUD operations
    std::string createEmailLog(const EmailLog& emailLog);
    bool updateEmailLogStatus(const std::string& logId, EmailStatus status, const std::string& errorMessage = "");
    bool updateEmailLogSent(const std::string& logId);
    bool updateEmailLogFailed(const std::string& logId, const std::string& errorMessage);
    
    // Query operations
    std::vector<EmailLog> getEmailLogsByStatus(EmailStatus status);
    std::vector<EmailLog> getEmailLogsByRecipient(const std::string& recipientEmail);
    std::vector<EmailLog> getEmailLogsByDomain(const std::string& domainName);
    std::vector<EmailLog> getEmailLogsByDateRange(
        std::chrono::system_clock::time_point startDate,
        std::chrono::system_clock::time_point endDate
    );
    EmailLog getEmailLogById(const std::string& logId);
    
    // Statistics
    int getTotalEmailCount();
    int getEmailCountByStatus(EmailStatus status);
    int getEmailCountByDomain(const std::string& domainName);
    int getEmailCountByLanguage(const std::string& language);
    
    // Cleanup operations
    bool deleteOldLogs(int daysToKeep = 90);
    
    // Utility functions
    std::string statusToString(EmailStatus status);
    EmailStatus stringToStatus(const std::string& statusStr);
    
    // Connection management
    bool isConnected() const;
    std::string getLastError() const;

private:
    std::unique_ptr<mongocxx::client> client_;
    mongocxx::database database_;
    mongocxx::collection collection_;
    std::string lastError_;
    
    // Helper functions
    bsoncxx::document::value emailLogToDocument(const EmailLog& emailLog);
    EmailLog documentToEmailLog(const bsoncxx::document::view& doc);
    std::chrono::system_clock::time_point bsonDateToTimePoint(const bsoncxx::types::b_date& date);
    bsoncxx::types::b_date timePointToBsonDate(const std::chrono::system_clock::time_point& timePoint);
};

} // namespace search_engine::storage
