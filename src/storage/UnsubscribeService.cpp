#include "../../include/search_engine/storage/UnsubscribeService.h"
#include "../../include/Logger.h"
#include "../../include/mongodb.h"
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/exception/exception.hpp>
#include <random>
#include <iomanip>
#include <sstream>
#include <openssl/sha.h>

using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;

namespace search_engine {
namespace storage {

UnsubscribeService::UnsubscribeService() {
    try {
        initializeDatabase();
        LOG_INFO("UnsubscribeService: Initialized successfully");
    } catch (const std::exception& e) {
        LOG_ERROR("UnsubscribeService: Failed to initialize: " + std::string(e.what()));
        throw;
    }
}

void UnsubscribeService::initializeDatabase() {
    // CRITICAL: Initialize MongoDB instance before creating client
    mongocxx::instance& instance = MongoDBInstance::getInstance();
    
    const char* mongoUri = std::getenv("MONGODB_URI");
    std::string uri = mongoUri ? mongoUri : "mongodb://admin:password123@mongodb:27017";
    
    LOG_DEBUG("UnsubscribeService: Connecting to MongoDB: " + uri);
    
    mongocxx::uri mongoUri_obj{uri};
    client_ = std::make_unique<mongocxx::client>(mongoUri_obj);
    
    // Test connection
    auto db = (*client_)["search-engine"];
    auto result = db.run_command(document{} << "ping" << 1 << finalize);
    if (!result.empty()) {
        LOG_INFO("UnsubscribeService: MongoDB connection established");
    }
    
    // Get unsubscribes collection
    collection_ = db["unsubscribes"];
    
    // Create indexes for better performance
    try {
        // Index on email for fast lookups
        collection_.create_index(document{} << "email" << 1 << finalize);
        
        // Index on token for unsubscribe link processing
        collection_.create_index(document{} << "token" << 1 << finalize);
        
        // Index on unsubscribedAt for analytics
        collection_.create_index(document{} << "unsubscribedAt" << -1 << finalize);
        
        // Compound index for active unsubscribes
        collection_.create_index(document{} << "email" << 1 << "isActive" << 1 << finalize);
        
        LOG_INFO("UnsubscribeService: Database indexes created successfully");
    } catch (const mongocxx::exception& e) {
        LOG_WARNING("UnsubscribeService: Failed to create indexes (may already exist): " + std::string(e.what()));
    }
}

std::string UnsubscribeService::generateSecureToken() {
    // Generate cryptographically secure random token
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    // Generate 32 random bytes
    std::vector<unsigned char> randomBytes(32);
    for (auto& byte : randomBytes) {
        byte = static_cast<unsigned char>(dis(gen));
    }
    
    // Hash with SHA-256 for additional security
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(randomBytes.data(), randomBytes.size(), hash);
    
    // Convert to hex string
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    
    return ss.str();
}

std::string UnsubscribeService::generateUnsubscribeToken(const std::string& email) {
    LOG_DEBUG("UnsubscribeService: Generating unsubscribe token for: " + email);
    
    try {
        // Check if email already has an active unsubscribe record
        auto existing = getUnsubscribeByEmail(email);
        if (existing.has_value() && existing->isActive) {
            LOG_DEBUG("UnsubscribeService: Email already unsubscribed, returning existing token");
            return existing->token;
        }
        
        // Generate new secure token
        std::string token = generateSecureToken();
        
        LOG_INFO("UnsubscribeService: Generated new token for: " + email);
        return token;
        
    } catch (const std::exception& e) {
        LOG_ERROR("UnsubscribeService: Failed to generate token for " + email + ": " + std::string(e.what()));
        return "";
    }
}

std::string UnsubscribeService::createUnsubscribeToken(const std::string& email,
                                                      const std::string& ipAddress,
                                                      const std::string& userAgent) {
    LOG_DEBUG("UnsubscribeService: Creating unsubscribe token for: " + email);
    
    try {
        // Check if email already has an active unsubscribe record
        auto existing = getUnsubscribeByEmail(email);
        if (existing.has_value() && existing->isActive) {
            LOG_DEBUG("UnsubscribeService: Email already unsubscribed, returning existing token");
            return existing->token;
        }
        
        // Generate new token
        std::string token = generateSecureToken();
        
        // Create unsubscribe record (but don't mark as unsubscribed yet)
        UnsubscribeRecord record;
        record.email = email;
        record.token = token;
        record.ipAddress = ipAddress;
        record.userAgent = userAgent;
        record.createdAt = std::chrono::system_clock::now();
        record.unsubscribedAt = std::chrono::system_clock::time_point{}; // Not unsubscribed yet
        record.isActive = false; // Will be activated when token is used
        
        // Store in database
        auto doc = recordToBson(record);
        auto result = collection_.insert_one(doc.view());
        
        if (result.has_value()) {
            LOG_INFO("UnsubscribeService: Token created successfully for: " + email);
            return token;
        } else {
            LOG_ERROR("UnsubscribeService: Failed to insert token record for: " + email);
            return "";
        }
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("UnsubscribeService: MongoDB error creating token for " + email + ": " + std::string(e.what()));
        return "";
    } catch (const std::exception& e) {
        LOG_ERROR("UnsubscribeService: Exception creating token for " + email + ": " + std::string(e.what()));
        return "";
    }
}

bool UnsubscribeService::processUnsubscribe(const std::string& token,
                                           const std::string& ipAddress,
                                           const std::string& userAgent,
                                           const std::optional<std::string>& reason) {
    LOG_INFO("UnsubscribeService: Processing unsubscribe for token: " + token.substr(0, 8) + "...");
    
    try {
        // Find the token record
        auto tokenRecord = getUnsubscribeByToken(token);
        if (!tokenRecord.has_value()) {
            LOG_WARNING("UnsubscribeService: Token not found: " + token.substr(0, 8) + "...");
            return false;
        }
        
        // Check if already unsubscribed
        if (tokenRecord->isActive) {
            LOG_INFO("UnsubscribeService: Email already unsubscribed: " + tokenRecord->email);
            return true; // Already unsubscribed, consider it successful
        }
        
        // Update the record to mark as unsubscribed
        auto filter = document{} << "token" << token << finalize;
        auto updateBuilder = document{};
        updateBuilder << "$set" << bsoncxx::builder::stream::open_document
                << "isActive" << true
                << "unsubscribedAt" << bsoncxx::types::b_date{std::chrono::system_clock::now()}
                << "ipAddress" << ipAddress
                << "userAgent" << userAgent;
        
        if (reason.has_value()) {
            updateBuilder << "reason" << reason.value();
        }
        
        updateBuilder << bsoncxx::builder::stream::close_document;
        auto update = updateBuilder << finalize;
        
        auto result = collection_.update_one(filter.view(), update.view());
        
        if (result.has_value() && result->modified_count() > 0) {
            LOG_INFO("UnsubscribeService: Successfully unsubscribed: " + tokenRecord->email);
            return true;
        } else {
            LOG_ERROR("UnsubscribeService: Failed to update unsubscribe record for token: " + token.substr(0, 8) + "...");
            return false;
        }
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("UnsubscribeService: MongoDB error processing unsubscribe: " + std::string(e.what()));
        return false;
    } catch (const std::exception& e) {
        LOG_ERROR("UnsubscribeService: Exception processing unsubscribe: " + std::string(e.what()));
        return false;
    }
}

bool UnsubscribeService::isEmailUnsubscribed(const std::string& email) {
    LOG_DEBUG("UnsubscribeService: Checking unsubscribe status for: " + email);
    
    try {
        auto filter = document{} 
            << "email" << email 
            << "isActive" << true 
            << finalize;
        
        auto result = collection_.find_one(filter.view());
        bool isUnsubscribed = result.has_value();
        
        LOG_DEBUG("UnsubscribeService: Email " + email + " unsubscribe status: " + (isUnsubscribed ? "UNSUBSCRIBED" : "SUBSCRIBED"));
        return isUnsubscribed;
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("UnsubscribeService: MongoDB error checking unsubscribe status: " + std::string(e.what()));
        return false; // Default to subscribed on error
    } catch (const std::exception& e) {
        LOG_ERROR("UnsubscribeService: Exception checking unsubscribe status: " + std::string(e.what()));
        return false; // Default to subscribed on error
    }
}

std::optional<UnsubscribeRecord> UnsubscribeService::getUnsubscribeByToken(const std::string& token) {
    LOG_DEBUG("UnsubscribeService: Looking up unsubscribe by token: " + token.substr(0, 8) + "...");
    
    try {
        auto filter = document{} << "token" << token << finalize;
        auto result = collection_.find_one(filter.view());
        
        if (result.has_value()) {
            auto record = bsonToRecord(result.value());
            LOG_DEBUG("UnsubscribeService: Found unsubscribe record for email: " + record.email);
            return record;
        } else {
            LOG_DEBUG("UnsubscribeService: No unsubscribe record found for token");
            return std::nullopt;
        }
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("UnsubscribeService: MongoDB error getting unsubscribe by token: " + std::string(e.what()));
        return std::nullopt;
    } catch (const std::exception& e) {
        LOG_ERROR("UnsubscribeService: Exception getting unsubscribe by token: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::optional<UnsubscribeRecord> UnsubscribeService::getUnsubscribeByEmail(const std::string& email) {
    LOG_DEBUG("UnsubscribeService: Looking up unsubscribe by email: " + email);
    
    try {
        auto filter = document{} 
            << "email" << email 
            << "isActive" << true 
            << finalize;
        
        auto result = collection_.find_one(filter.view());
        
        if (result.has_value()) {
            auto record = bsonToRecord(result.value());
            LOG_DEBUG("UnsubscribeService: Found active unsubscribe record for: " + email);
            return record;
        } else {
            LOG_DEBUG("UnsubscribeService: No active unsubscribe record found for: " + email);
            return std::nullopt;
        }
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("UnsubscribeService: MongoDB error getting unsubscribe by email: " + std::string(e.what()));
        return std::nullopt;
    } catch (const std::exception& e) {
        LOG_ERROR("UnsubscribeService: Exception getting unsubscribe by email: " + std::string(e.what()));
        return std::nullopt;
    }
}

bool UnsubscribeService::reactivateEmail(const std::string& email) {
    LOG_INFO("UnsubscribeService: Reactivating email: " + email);
    
    try {
        auto filter = document{} 
            << "email" << email 
            << "isActive" << true 
            << finalize;
        
        auto update = document{} 
            << "$set" << bsoncxx::builder::stream::open_document
                << "isActive" << false
            << bsoncxx::builder::stream::close_document 
            << finalize;
        
        auto result = collection_.update_many(filter.view(), update.view());
        
        if (result.has_value() && result->modified_count() > 0) {
            LOG_INFO("UnsubscribeService: Successfully reactivated email: " + email);
            return true;
        } else {
            LOG_WARNING("UnsubscribeService: No active unsubscribe records found for: " + email);
            return true; // Consider it successful if email wasn't unsubscribed
        }
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("UnsubscribeService: MongoDB error reactivating email: " + std::string(e.what()));
        return false;
    } catch (const std::exception& e) {
        LOG_ERROR("UnsubscribeService: Exception reactivating email: " + std::string(e.what()));
        return false;
    }
}

bsoncxx::document::value UnsubscribeService::recordToBson(const UnsubscribeRecord& record) {
    auto builder = document{};
    
    if (record.id.has_value()) {
        builder << "_id" << bsoncxx::oid{record.id.value()};
    }
    
    builder << "email" << record.email
            << "token" << record.token
            << "ipAddress" << record.ipAddress
            << "userAgent" << record.userAgent
            << "createdAt" << bsoncxx::types::b_date{record.createdAt}
            << "isActive" << record.isActive;
    
    // Only add unsubscribedAt if it's not default (zero time point)
    if (record.unsubscribedAt != std::chrono::system_clock::time_point{}) {
        builder << "unsubscribedAt" << bsoncxx::types::b_date{record.unsubscribedAt};
    }
    
    if (record.reason.has_value()) {
        builder << "reason" << record.reason.value();
    }
    
    if (record.source.has_value()) {
        builder << "source" << record.source.value();
    }
    
    return builder << finalize;
}

UnsubscribeRecord UnsubscribeService::bsonToRecord(const bsoncxx::document::view& doc) {
    UnsubscribeRecord record;
    
    if (doc["_id"]) {
        record.id = doc["_id"].get_oid().value.to_string();
    }
    
    record.email = std::string(doc["email"].get_string().value);
    record.token = std::string(doc["token"].get_string().value);
    record.ipAddress = std::string(doc["ipAddress"].get_string().value);
    record.userAgent = std::string(doc["userAgent"].get_string().value);
    
    record.createdAt = std::chrono::system_clock::time_point{doc["createdAt"].get_date().value};
    
    if (doc["unsubscribedAt"]) {
        record.unsubscribedAt = std::chrono::system_clock::time_point{doc["unsubscribedAt"].get_date().value};
    }
    
    record.isActive = doc["isActive"].get_bool().value;
    
    if (doc["reason"]) {
        record.reason = std::string(doc["reason"].get_string().value);
    }
    
    if (doc["source"]) {
        record.source = std::string(doc["source"].get_string().value);
    }
    
    return record;
}

} // namespace storage
} // namespace search_engine
