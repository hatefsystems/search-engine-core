#include "../../include/search_engine/storage/EmailLogsStorage.h"
#include "../../include/Logger.h"
#include "../../include/mongodb.h"
#include <sstream>
#include <iomanip>

namespace search_engine::storage {

EmailLogsStorage::EmailLogsStorage() {
    try {
        // Initialize MongoDB instance
        MongoDBInstance::getInstance();
        
        // Connect to MongoDB
        const char* mongoUri = std::getenv("MONGODB_URI");
        std::string mongoConnectionString = mongoUri ? mongoUri : "mongodb://admin:password123@mongodb:27017";
        
        mongocxx::uri uri(mongoConnectionString);
        client_ = std::make_unique<mongocxx::client>(uri);
        database_ = client_->database("search-engine");
        collection_ = database_.collection("email_logs");
        
        LOG_INFO("EmailLogsStorage: Connected to MongoDB successfully");
        
        // Initialize database indexes
        initializeDatabase();
        
    } catch (const std::exception& e) {
        lastError_ = "Failed to initialize EmailLogsStorage: " + std::string(e.what());
        LOG_ERROR("EmailLogsStorage: " + lastError_);
    }
}

bool EmailLogsStorage::initializeDatabase() {
    try {
        // Create indexes for better query performance
        mongocxx::options::index indexOptions{};
        
        // Index on status for status-based queries
        auto statusIndex = bsoncxx::builder::stream::document{}
            << "status" << 1
            << bsoncxx::builder::stream::finalize;
        collection_.create_index(statusIndex.view(), indexOptions);
        
        // Index on toEmail for recipient-based queries
        auto emailIndex = bsoncxx::builder::stream::document{}
            << "toEmail" << 1
            << bsoncxx::builder::stream::finalize;
        collection_.create_index(emailIndex.view(), indexOptions);
        
        // Index on domainName for domain-based queries
        auto domainIndex = bsoncxx::builder::stream::document{}
            << "domainName" << 1
            << bsoncxx::builder::stream::finalize;
        collection_.create_index(domainIndex.view(), indexOptions);
        
        // Index on language for language-based queries
        auto languageIndex = bsoncxx::builder::stream::document{}
            << "language" << 1
            << bsoncxx::builder::stream::finalize;
        collection_.create_index(languageIndex.view(), indexOptions);
        
        // Compound index on queuedAt for date range queries
        auto dateIndex = bsoncxx::builder::stream::document{}
            << "queuedAt" << 1
            << "status" << 1
            << bsoncxx::builder::stream::finalize;
        collection_.create_index(dateIndex.view(), indexOptions);
        
        // TTL index to automatically delete old logs after 90 days
        auto ttlIndex = bsoncxx::builder::stream::document{}
            << "queuedAt" << 1
            << bsoncxx::builder::stream::finalize;
        
        mongocxx::options::index ttlOptions{};
        ttlOptions.expire_after(std::chrono::seconds(90 * 24 * 60 * 60)); // 90 days
        
        collection_.create_index(ttlIndex.view(), ttlOptions);
        
        LOG_INFO("EmailLogsStorage: Database indexes created successfully");
        return true;
        
    } catch (const std::exception& e) {
        lastError_ = "Failed to initialize database indexes: " + std::string(e.what());
        LOG_ERROR("EmailLogsStorage: " + lastError_);
        return false;
    }
}

std::string EmailLogsStorage::createEmailLog(const EmailLog& emailLog) {
    try {
        auto doc = emailLogToDocument(emailLog);
        auto result = collection_.insert_one(doc.view());
        
        if (result) {
            std::string logId = result->inserted_id().get_oid().value.to_string();
            LOG_DEBUG("EmailLogsStorage: Created email log with ID: " + logId);
            return logId;
        } else {
            lastError_ = "Failed to insert email log into database";
            LOG_ERROR("EmailLogsStorage: " + lastError_);
            return "";
        }
        
    } catch (const std::exception& e) {
        lastError_ = "Exception in createEmailLog: " + std::string(e.what());
        LOG_ERROR("EmailLogsStorage: " + lastError_);
        return "";
    }
}

bool EmailLogsStorage::updateEmailLogStatus(const std::string& logId, EmailStatus status, const std::string& errorMessage) {
    try {
        bsoncxx::oid oid;
        try {
            oid = bsoncxx::oid(logId);
        } catch (const std::exception&) {
            lastError_ = "Invalid ObjectId format: " + logId;
            return false;
        }
        
        auto filter = bsoncxx::builder::stream::document{}
            << "_id" << oid
            << bsoncxx::builder::stream::finalize;
        
        auto now = std::chrono::system_clock::now();
        bsoncxx::builder::stream::document updateBuilder;
        auto setBuilder = updateBuilder << "$set" << bsoncxx::builder::stream::open_document
            << "status" << static_cast<int>(status)
            << "errorMessage" << errorMessage;
        
        // Add timestamp based on status
        if (status == EmailStatus::SENT) {
            setBuilder << "sentAt" << timePointToBsonDate(now);
        } else if (status == EmailStatus::FAILED) {
            setBuilder << "failedAt" << timePointToBsonDate(now);
        }
        
        auto update = setBuilder << bsoncxx::builder::stream::close_document
            << bsoncxx::builder::stream::finalize;
        
        auto result = collection_.update_one(std::move(filter), std::move(update));
        
        if (result && result->modified_count() > 0) {
            LOG_DEBUG("EmailLogsStorage: Updated email log status for ID: " + logId + 
                     " to status: " + statusToString(status));
            return true;
        } else {
            lastError_ = "No email log found with ID: " + logId;
            return false;
        }
        
    } catch (const std::exception& e) {
        lastError_ = "Exception in updateEmailLogStatus: " + std::string(e.what());
        LOG_ERROR("EmailLogsStorage: " + lastError_);
        return false;
    }
}

bool EmailLogsStorage::updateEmailLogSent(const std::string& logId) {
    return updateEmailLogStatus(logId, EmailStatus::SENT);
}

bool EmailLogsStorage::updateEmailLogFailed(const std::string& logId, const std::string& errorMessage) {
    return updateEmailLogStatus(logId, EmailStatus::FAILED, errorMessage);
}

std::vector<EmailLogsStorage::EmailLog> EmailLogsStorage::getEmailLogsByStatus(EmailStatus status) {
    std::vector<EmailLogsStorage::EmailLog> logs;
    
    try {
        auto filter = bsoncxx::builder::stream::document{}
            << "status" << static_cast<int>(status)
            << bsoncxx::builder::stream::finalize;
        
        auto cursor = collection_.find(filter.view());
        
        for (auto&& doc : cursor) {
            logs.push_back(documentToEmailLog(doc));
        }
        
        LOG_DEBUG("EmailLogsStorage: Found " + std::to_string(logs.size()) + 
                 " email logs with status: " + statusToString(status));
        
    } catch (const std::exception& e) {
        lastError_ = "Exception in getEmailLogsByStatus: " + std::string(e.what());
        LOG_ERROR("EmailLogsStorage: " + lastError_);
    }
    
    return logs;
}

std::vector<EmailLogsStorage::EmailLog> EmailLogsStorage::getEmailLogsByRecipient(const std::string& recipientEmail) {
    std::vector<EmailLogsStorage::EmailLog> logs;
    
    try {
        auto filter = bsoncxx::builder::stream::document{}
            << "toEmail" << recipientEmail
            << bsoncxx::builder::stream::finalize;
        
        auto cursor = collection_.find(filter.view());
        
        for (auto&& doc : cursor) {
            logs.push_back(documentToEmailLog(doc));
        }
        
        LOG_DEBUG("EmailLogsStorage: Found " + std::to_string(logs.size()) + 
                 " email logs for recipient: " + recipientEmail);
        
    } catch (const std::exception& e) {
        lastError_ = "Exception in getEmailLogsByRecipient: " + std::string(e.what());
        LOG_ERROR("EmailLogsStorage: " + lastError_);
    }
    
    return logs;
}

std::vector<EmailLogsStorage::EmailLog> EmailLogsStorage::getEmailLogsByDomain(const std::string& domainName) {
    std::vector<EmailLogsStorage::EmailLog> logs;
    
    try {
        auto filter = bsoncxx::builder::stream::document{}
            << "domainName" << domainName
            << bsoncxx::builder::stream::finalize;
        
        auto cursor = collection_.find(filter.view());
        
        for (auto&& doc : cursor) {
            logs.push_back(documentToEmailLog(doc));
        }
        
        LOG_DEBUG("EmailLogsStorage: Found " + std::to_string(logs.size()) + 
                 " email logs for domain: " + domainName);
        
    } catch (const std::exception& e) {
        lastError_ = "Exception in getEmailLogsByDomain: " + std::string(e.what());
        LOG_ERROR("EmailLogsStorage: " + lastError_);
    }
    
    return logs;
}

std::vector<EmailLogsStorage::EmailLog> EmailLogsStorage::getEmailLogsByDateRange(
    std::chrono::system_clock::time_point startDate,
    std::chrono::system_clock::time_point endDate) {
    
    std::vector<EmailLogsStorage::EmailLog> logs;
    
    try {
        auto filter = bsoncxx::builder::stream::document{}
            << "queuedAt" << bsoncxx::builder::stream::open_document
            << "$gte" << timePointToBsonDate(startDate)
            << "$lte" << timePointToBsonDate(endDate)
            << bsoncxx::builder::stream::close_document
            << bsoncxx::builder::stream::finalize;
        
        auto cursor = collection_.find(filter.view());
        
        for (auto&& doc : cursor) {
            logs.push_back(documentToEmailLog(doc));
        }
        
        LOG_DEBUG("EmailLogsStorage: Found " + std::to_string(logs.size()) + 
                 " email logs in date range");
        
    } catch (const std::exception& e) {
        lastError_ = "Exception in getEmailLogsByDateRange: " + std::string(e.what());
        LOG_ERROR("EmailLogsStorage: " + lastError_);
    }
    
    return logs;
}

EmailLogsStorage::EmailLog EmailLogsStorage::getEmailLogById(const std::string& logId) {
    EmailLogsStorage::EmailLog emailLog;
    
    try {
        bsoncxx::oid oid;
        try {
            oid = bsoncxx::oid(logId);
        } catch (const std::exception&) {
            lastError_ = "Invalid ObjectId format: " + logId;
            return emailLog;
        }
        
        auto filter = bsoncxx::builder::stream::document{}
            << "_id" << oid
            << bsoncxx::builder::stream::finalize;
        
        auto result = collection_.find_one(filter.view());
        
        if (result) {
            emailLog = documentToEmailLog(result->view());
            LOG_DEBUG("EmailLogsStorage: Found email log with ID: " + logId);
        } else {
            lastError_ = "No email log found with ID: " + logId;
        }
        
    } catch (const std::exception& e) {
        lastError_ = "Exception in getEmailLogById: " + std::string(e.what());
        LOG_ERROR("EmailLogsStorage: " + lastError_);
    }
    
    return emailLog;
}

int EmailLogsStorage::getTotalEmailCount() {
    try {
        auto result = collection_.count_documents({});
        return static_cast<int>(result);
    } catch (const std::exception& e) {
        lastError_ = "Exception in getTotalEmailCount: " + std::string(e.what());
        LOG_ERROR("EmailLogsStorage: " + lastError_);
        return 0;
    }
}

int EmailLogsStorage::getEmailCountByStatus(EmailStatus status) {
    try {
        auto filter = bsoncxx::builder::stream::document{}
            << "status" << static_cast<int>(status)
            << bsoncxx::builder::stream::finalize;
        
        auto result = collection_.count_documents(filter.view());
        return static_cast<int>(result);
    } catch (const std::exception& e) {
        lastError_ = "Exception in getEmailCountByStatus: " + std::string(e.what());
        LOG_ERROR("EmailLogsStorage: " + lastError_);
        return 0;
    }
}

int EmailLogsStorage::getEmailCountByDomain(const std::string& domainName) {
    try {
        auto filter = bsoncxx::builder::stream::document{}
            << "domainName" << domainName
            << bsoncxx::builder::stream::finalize;
        
        auto result = collection_.count_documents(filter.view());
        return static_cast<int>(result);
    } catch (const std::exception& e) {
        lastError_ = "Exception in getEmailCountByDomain: " + std::string(e.what());
        LOG_ERROR("EmailLogsStorage: " + lastError_);
        return 0;
    }
}

int EmailLogsStorage::getEmailCountByLanguage(const std::string& language) {
    try {
        auto filter = bsoncxx::builder::stream::document{}
            << "language" << language
            << bsoncxx::builder::stream::finalize;
        
        auto result = collection_.count_documents(filter.view());
        return static_cast<int>(result);
    } catch (const std::exception& e) {
        lastError_ = "Exception in getEmailCountByLanguage: " + std::string(e.what());
        LOG_ERROR("EmailLogsStorage: " + lastError_);
        return 0;
    }
}

bool EmailLogsStorage::deleteOldLogs(int daysToKeep) {
    try {
        auto cutoffDate = std::chrono::system_clock::now() - 
                         std::chrono::hours(24 * daysToKeep);
        
        auto filter = bsoncxx::builder::stream::document{}
            << "queuedAt" << bsoncxx::builder::stream::open_document
            << "$lt" << timePointToBsonDate(cutoffDate)
            << bsoncxx::builder::stream::close_document
            << bsoncxx::builder::stream::finalize;
        
        auto result = collection_.delete_many(filter.view());
        
        LOG_INFO("EmailLogsStorage: Deleted " + std::to_string(result->deleted_count()) + 
                " old email logs (older than " + std::to_string(daysToKeep) + " days)");
        
        return true;
        
    } catch (const std::exception& e) {
        lastError_ = "Exception in deleteOldLogs: " + std::string(e.what());
        LOG_ERROR("EmailLogsStorage: " + lastError_);
        return false;
    }
}

std::string EmailLogsStorage::statusToString(EmailStatus status) {
    switch (status) {
        case EmailStatus::QUEUED: return "queued";
        case EmailStatus::SENT: return "sent";
        case EmailStatus::FAILED: return "failed";
        case EmailStatus::PENDING: return "pending";
        default: return "unknown";
    }
}

EmailLogsStorage::EmailStatus EmailLogsStorage::stringToStatus(const std::string& statusStr) {
    if (statusStr == "queued") return EmailStatus::QUEUED;
    if (statusStr == "sent") return EmailStatus::SENT;
    if (statusStr == "failed") return EmailStatus::FAILED;
    if (statusStr == "pending") return EmailStatus::PENDING;
    return EmailStatus::QUEUED; // Default
}

bool EmailLogsStorage::isConnected() const {
    return client_ != nullptr;
}

std::string EmailLogsStorage::getLastError() const {
    return lastError_;
}

// Helper function implementations

bsoncxx::document::value EmailLogsStorage::emailLogToDocument(const EmailLog& emailLog) {
    auto builder = bsoncxx::builder::stream::document{}
        << "toEmail" << emailLog.toEmail
        << "fromEmail" << emailLog.fromEmail
        << "recipientName" << emailLog.recipientName
        << "domainName" << emailLog.domainName
        << "subject" << emailLog.subject
        << "language" << emailLog.language
        << "emailType" << emailLog.emailType
        << "status" << static_cast<int>(emailLog.status)
        << "errorMessage" << emailLog.errorMessage
        << "crawlSessionId" << emailLog.crawlSessionId
        << "crawledPagesCount" << emailLog.crawledPagesCount
        << "queuedAt" << timePointToBsonDate(emailLog.queuedAt)
        << "sentAt" << timePointToBsonDate(emailLog.sentAt)
        << "failedAt" << timePointToBsonDate(emailLog.failedAt)
        << bsoncxx::builder::stream::finalize;
    
    return builder;
}

EmailLogsStorage::EmailLog EmailLogsStorage::documentToEmailLog(const bsoncxx::document::view& doc) {
    EmailLogsStorage::EmailLog emailLog;
    
    try {
        emailLog.id = std::string(doc["_id"].get_oid().value.to_string());
        emailLog.toEmail = std::string(doc["toEmail"].get_string().value);
        emailLog.fromEmail = std::string(doc["fromEmail"].get_string().value);
        emailLog.recipientName = std::string(doc["recipientName"].get_string().value);
        emailLog.domainName = std::string(doc["domainName"].get_string().value);
        emailLog.subject = std::string(doc["subject"].get_string().value);
        emailLog.language = std::string(doc["language"].get_string().value);
        emailLog.emailType = std::string(doc["emailType"].get_string().value);
        emailLog.status = static_cast<EmailStatus>(doc["status"].get_int32().value);
        emailLog.errorMessage = std::string(doc["errorMessage"].get_string().value);
        emailLog.crawlSessionId = std::string(doc["crawlSessionId"].get_string().value);
        emailLog.crawledPagesCount = doc["crawledPagesCount"].get_int32().value;
        emailLog.queuedAt = bsonDateToTimePoint(doc["queuedAt"].get_date());
        
        // Optional timestamps
        if (doc["sentAt"]) {
            emailLog.sentAt = bsonDateToTimePoint(doc["sentAt"].get_date());
        }
        
        if (doc["failedAt"]) {
            emailLog.failedAt = bsonDateToTimePoint(doc["failedAt"].get_date());
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("EmailLogsStorage: Error parsing document to EmailLog: " + std::string(e.what()));
    }
    
    return emailLog;
}

std::chrono::system_clock::time_point EmailLogsStorage::bsonDateToTimePoint(const bsoncxx::types::b_date& date) {
    return std::chrono::system_clock::time_point(
        std::chrono::milliseconds(date.to_int64())
    );
}

bsoncxx::types::b_date EmailLogsStorage::timePointToBsonDate(const std::chrono::system_clock::time_point& timePoint) {
    auto duration = timePoint.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    return bsoncxx::types::b_date(std::chrono::milliseconds(millis));
}

} // namespace search_engine::storage
