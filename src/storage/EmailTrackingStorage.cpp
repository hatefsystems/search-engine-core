#include "../../include/search_engine/storage/EmailTrackingStorage.h"
#include "../../include/Logger.h"
#include "../../include/mongodb.h"
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>
#include <mongocxx/exception/exception.hpp>
#include <random>
#include <sstream>
#include <iomanip>

namespace search_engine { namespace storage {

EmailTrackingStorage::EmailTrackingStorage() {
    try {
        // Initialize MongoDB instance
        MongoDBInstance::getInstance();
        
        // Get MongoDB URI from environment or use default
        const char* mongoUri = std::getenv("MONGODB_URI");
        std::string uri = mongoUri ? mongoUri : "mongodb://admin:password123@mongodb:27017";
        
        // Create MongoDB client
        client_ = std::make_unique<mongocxx::client>(mongocxx::uri{uri});
        
        LOG_INFO("EmailTrackingStorage initialized successfully");
    } catch (const std::exception& e) {
        lastError_ = "Failed to initialize EmailTrackingStorage: " + std::string(e.what());
        LOG_ERROR("EmailTrackingStorage: " + lastError_);
        throw;
    }
}

Result<std::string> EmailTrackingStorage::createTrackingRecord(const std::string& emailAddress, 
                                                               const std::string& emailType) {
    try {
        LOG_DEBUG("Creating tracking record for email: " + emailAddress + ", type: " + emailType);
        
        // Generate unique tracking ID
        std::string trackingId = generateTrackingId();
        
        // Get database and collection
        auto db = (*client_)["search-engine"];
        auto collection = db["track_email"];
        
        // Get current timestamp in milliseconds
        auto now = std::chrono::system_clock::now();
        auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        
        // Create tracking document
        using bsoncxx::builder::stream::document;
        using bsoncxx::builder::stream::finalize;
        
        auto doc = document{}
            << "tracking_id" << trackingId
            << "email_address" << emailAddress
            << "email_type" << emailType
            << "is_opened" << false
            << "open_count" << 0
            << "sent_at" << bsoncxx::types::b_date{std::chrono::milliseconds{nowMs}}
            << "created_at" << bsoncxx::types::b_date{std::chrono::milliseconds{nowMs}}
            << finalize;
        
        // Insert document
        auto result = collection.insert_one(doc.view());
        
        if (result) {
            LOG_INFO("Created tracking record with ID: " + trackingId);
            return Result<std::string>::Success(trackingId, "Tracking record created successfully");
        } else {
            lastError_ = "Failed to insert tracking record";
            LOG_ERROR("EmailTrackingStorage: " + lastError_);
            return Result<std::string>::Failure(lastError_);
        }
        
    } catch (const mongocxx::exception& e) {
        lastError_ = "MongoDB error: " + std::string(e.what());
        LOG_ERROR("EmailTrackingStorage: " + lastError_);
        return Result<std::string>::Failure(lastError_);
    } catch (const std::exception& e) {
        lastError_ = "Error creating tracking record: " + std::string(e.what());
        LOG_ERROR("EmailTrackingStorage: " + lastError_);
        return Result<std::string>::Failure(lastError_);
    }
}

Result<bool> EmailTrackingStorage::recordEmailOpen(const std::string& trackingId, 
                                                   const std::string& ipAddress,
                                                   const std::string& userAgent) {
    try {
        LOG_DEBUG("Recording email open for tracking ID: " + trackingId + ", IP: " + ipAddress);
        
        // Get database and collection
        auto db = (*client_)["search-engine"];
        auto collection = db["track_email"];
        
        // Get current timestamp in milliseconds
        auto now = std::chrono::system_clock::now();
        auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        
        using bsoncxx::builder::stream::document;
        using bsoncxx::builder::stream::finalize;
        
        // Find existing tracking record
        auto filter = document{} << "tracking_id" << trackingId << finalize;
        auto existingDoc = collection.find_one(filter.view());
        
        if (!existingDoc) {
            lastError_ = "Tracking ID not found: " + trackingId;
            LOG_WARNING("EmailTrackingStorage: " + lastError_);
            return Result<bool>::Failure(lastError_);
        }
        
        // Check if this is the first open
        auto view = existingDoc->view();
        bool wasOpened = false;
        if (view["is_opened"]) {
            wasOpened = view["is_opened"].get_bool().value;
        }
        
        int currentOpenCount = 0;
        if (view["open_count"]) {
            currentOpenCount = view["open_count"].get_int32().value;
        }
        
        // Build update document
        document updateDoc{};
        
        // Set fields
        updateDoc << "$set" << bsoncxx::builder::stream::open_document
            << "is_opened" << true
            << "open_count" << (currentOpenCount + 1)
            << "last_opened_at" << bsoncxx::types::b_date{std::chrono::milliseconds{nowMs}}
            << "last_ip_address" << ipAddress
            << "last_user_agent" << userAgent;
        
        // If first open, also set opened_at
        if (!wasOpened) {
            updateDoc << "opened_at" << bsoncxx::types::b_date{std::chrono::milliseconds{nowMs}};
        }
        
        updateDoc << bsoncxx::builder::stream::close_document;
        
        // Add to open history array
        updateDoc << "$push" << bsoncxx::builder::stream::open_document
            << "open_history" << bsoncxx::builder::stream::open_document
                << "ip_address" << ipAddress
                << "user_agent" << userAgent
                << "opened_at" << bsoncxx::types::b_date{std::chrono::milliseconds{nowMs}}
                << bsoncxx::builder::stream::close_document
            << bsoncxx::builder::stream::close_document
            << finalize;
        
        auto result = collection.update_one(filter.view(), updateDoc.view());
        
        if (result && result->modified_count() > 0) {
            LOG_INFO("Recorded email open for tracking ID: " + trackingId + " (open #" + std::to_string(currentOpenCount + 1) + ")");
            return Result<bool>::Success(true, "Email open recorded successfully");
        } else {
            lastError_ = "Failed to update tracking record";
            LOG_ERROR("EmailTrackingStorage: " + lastError_);
            return Result<bool>::Failure(lastError_);
        }
        
    } catch (const mongocxx::exception& e) {
        lastError_ = "MongoDB error: " + std::string(e.what());
        LOG_ERROR("EmailTrackingStorage: " + lastError_);
        return Result<bool>::Failure(lastError_);
    } catch (const std::exception& e) {
        lastError_ = "Error recording email open: " + std::string(e.what());
        LOG_ERROR("EmailTrackingStorage: " + lastError_);
        return Result<bool>::Failure(lastError_);
    }
}

Result<EmailTrackingStorage::TrackingEvent> EmailTrackingStorage::getTrackingEvent(const std::string& trackingId) {
    try {
        LOG_DEBUG("Getting tracking event for ID: " + trackingId);
        
        // Get database and collection
        auto db = (*client_)["search-engine"];
        auto collection = db["track_email"];
        
        using bsoncxx::builder::stream::document;
        using bsoncxx::builder::stream::finalize;
        
        auto filter = document{} << "tracking_id" << trackingId << finalize;
        auto doc = collection.find_one(filter.view());
        
        if (!doc) {
            lastError_ = "Tracking event not found for ID: " + trackingId;
            LOG_WARNING("EmailTrackingStorage: " + lastError_);
            return Result<TrackingEvent>::Failure(lastError_);
        }
        
        TrackingEvent event = parseTrackingEvent(doc->view());
        return Result<TrackingEvent>::Success(event, "Tracking event retrieved successfully");
        
    } catch (const mongocxx::exception& e) {
        lastError_ = "MongoDB error: " + std::string(e.what());
        LOG_ERROR("EmailTrackingStorage: " + lastError_);
        return Result<TrackingEvent>::Failure(lastError_);
    } catch (const std::exception& e) {
        lastError_ = "Error getting tracking event: " + std::string(e.what());
        LOG_ERROR("EmailTrackingStorage: " + lastError_);
        return Result<TrackingEvent>::Failure(lastError_);
    }
}

Result<std::vector<EmailTrackingStorage::TrackingEvent>> EmailTrackingStorage::getTrackingEventsByEmail(
    const std::string& emailAddress, int limit) {
    try {
        LOG_DEBUG("Getting tracking events for email: " + emailAddress);
        
        // Get database and collection
        auto db = (*client_)["search-engine"];
        auto collection = db["track_email"];
        
        using bsoncxx::builder::stream::document;
        using bsoncxx::builder::stream::finalize;
        
        auto filter = document{} << "email_address" << emailAddress << finalize;
        
        mongocxx::options::find opts;
        opts.sort(document{} << "sent_at" << -1 << finalize);
        opts.limit(limit);
        
        auto cursor = collection.find(filter.view(), opts);
        
        std::vector<TrackingEvent> events;
        for (auto&& doc : cursor) {
            events.push_back(parseTrackingEvent(doc));
        }
        
        LOG_INFO("Retrieved " + std::to_string(events.size()) + " tracking events for email: " + emailAddress);
        return Result<std::vector<TrackingEvent>>::Success(events, "Tracking events retrieved successfully");
        
    } catch (const mongocxx::exception& e) {
        lastError_ = "MongoDB error: " + std::string(e.what());
        LOG_ERROR("EmailTrackingStorage: " + lastError_);
        return Result<std::vector<TrackingEvent>>::Failure(lastError_);
    } catch (const std::exception& e) {
        lastError_ = "Error getting tracking events: " + std::string(e.what());
        LOG_ERROR("EmailTrackingStorage: " + lastError_);
        return Result<std::vector<TrackingEvent>>::Failure(lastError_);
    }
}

Result<std::string> EmailTrackingStorage::getTrackingStats(const std::string& emailAddress) {
    try {
        LOG_DEBUG("Getting tracking stats for email: " + emailAddress);
        
        // Get database and collection
        auto db = (*client_)["search-engine"];
        auto collection = db["track_email"];
        
        using bsoncxx::builder::stream::document;
        using bsoncxx::builder::stream::finalize;
        
        // Count total emails sent
        auto filter = document{} << "email_address" << emailAddress << finalize;
        int64_t totalSent = collection.count_documents(filter.view());
        
        // Count opened emails
        auto openedFilter = document{} 
            << "email_address" << emailAddress 
            << "is_opened" << true 
            << finalize;
        int64_t totalOpened = collection.count_documents(openedFilter.view());
        
        // Calculate open rate
        double openRate = (totalSent > 0) ? (static_cast<double>(totalOpened) / totalSent * 100.0) : 0.0;
        
        // Build JSON response
        nlohmann::json stats;
        stats["email_address"] = emailAddress;
        stats["total_sent"] = totalSent;
        stats["total_opened"] = totalOpened;
        stats["open_rate"] = std::round(openRate * 100.0) / 100.0; // Round to 2 decimal places
        stats["unopened"] = totalSent - totalOpened;
        
        std::string jsonStr = stats.dump();
        
        LOG_INFO("Retrieved tracking stats for email: " + emailAddress + 
                 " (sent: " + std::to_string(totalSent) + 
                 ", opened: " + std::to_string(totalOpened) + 
                 ", rate: " + std::to_string(openRate) + "%)");
        
        return Result<std::string>::Success(jsonStr, "Tracking stats retrieved successfully");
        
    } catch (const mongocxx::exception& e) {
        lastError_ = "MongoDB error: " + std::string(e.what());
        LOG_ERROR("EmailTrackingStorage: " + lastError_);
        return Result<std::string>::Failure(lastError_);
    } catch (const std::exception& e) {
        lastError_ = "Error getting tracking stats: " + std::string(e.what());
        LOG_ERROR("EmailTrackingStorage: " + lastError_);
        return Result<std::string>::Failure(lastError_);
    }
}

std::string EmailTrackingStorage::generateTrackingId() {
    // Generate a unique tracking ID using random hex string
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;
    
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    oss << std::setw(16) << dis(gen);
    oss << std::setw(16) << dis(gen);
    
    return oss.str();
}

EmailTrackingStorage::TrackingEvent EmailTrackingStorage::parseTrackingEvent(const bsoncxx::document::view& doc) {
    TrackingEvent event;
    
    // Parse tracking ID
    if (doc["tracking_id"]) {
        event.trackingId = std::string(doc["tracking_id"].get_string().value);
    }
    
    // Parse email address
    if (doc["email_address"]) {
        event.emailAddress = std::string(doc["email_address"].get_string().value);
    }
    
    // Parse email type
    if (doc["email_type"]) {
        event.emailType = std::string(doc["email_type"].get_string().value);
    }
    
    // Parse IP address (from last open)
    if (doc["last_ip_address"]) {
        event.ipAddress = std::string(doc["last_ip_address"].get_string().value);
    }
    
    // Parse user agent (from last open)
    if (doc["last_user_agent"]) {
        event.userAgent = std::string(doc["last_user_agent"].get_string().value);
    }
    
    // Parse sent_at timestamp
    if (doc["sent_at"]) {
        auto sentMs = doc["sent_at"].get_date().to_int64();
        event.sentAt = std::chrono::system_clock::time_point(std::chrono::milliseconds(sentMs));
    }
    
    // Parse opened_at timestamp
    if (doc["opened_at"]) {
        auto openedMs = doc["opened_at"].get_date().to_int64();
        event.openedAt = std::chrono::system_clock::time_point(std::chrono::milliseconds(openedMs));
    }
    
    // Parse is_opened flag
    if (doc["is_opened"]) {
        event.isOpened = doc["is_opened"].get_bool().value;
    }
    
    // Parse open_count
    if (doc["open_count"]) {
        event.openCount = doc["open_count"].get_int32().value;
    }
    
    return event;
}

} } // namespace search_engine::storage

