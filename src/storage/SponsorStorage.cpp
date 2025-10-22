#include "../../include/search_engine/storage/SponsorStorage.h"
#include "../../include/Logger.h"
#include "../../include/mongodb.h"
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/exception/exception.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/helpers.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>

using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_document;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::close_array;

namespace search_engine {
namespace storage {

// Helper to convert system_clock::time_point to bsoncxx::types::b_date
static bsoncxx::types::b_date timePointToDate(const std::chrono::system_clock::time_point& tp) {
    auto duration = tp.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    return bsoncxx::types::b_date{millis};
}

// Helper to convert bsoncxx::types::b_date to system_clock::time_point
static std::chrono::system_clock::time_point dateToTimePoint(const bsoncxx::types::b_date& date) {
    return std::chrono::system_clock::time_point{date.value};
}

SponsorStorage::SponsorStorage(const std::string& connectionString, const std::string& databaseName) {
    LOG_DEBUG("SponsorStorage constructor called with database: " + databaseName);
    try {
        LOG_INFO("Initializing MongoDB connection to: " + connectionString);
        
        // Use the existing MongoDB instance singleton
        mongocxx::instance& instance = MongoDBInstance::getInstance();
        (void)instance; // Suppress unused variable warning
        
        // Create client and connect to database
        mongocxx::uri uri{connectionString};
        client_ = std::make_unique<mongocxx::client>(uri);
        database_ = (*client_)[databaseName];
        sponsorCollection_ = database_["sponsors"]; // Use the same collection name as imported data
        LOG_INFO("Connected to MongoDB database: " + databaseName);
        
        // Ensure indexes are created
        ensureIndexes();
        LOG_DEBUG("SponsorStorage indexes ensured");
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("Failed to initialize SponsorStorage MongoDB connection: " + std::string(e.what()));
        throw std::runtime_error("Failed to initialize SponsorStorage MongoDB connection: " + std::string(e.what()));
    }
}

std::string SponsorStorage::sponsorStatusToString(SponsorStatus status) {
    switch (status) {
        case SponsorStatus::PENDING: return "PENDING";
        case SponsorStatus::VERIFIED: return "VERIFIED";
        case SponsorStatus::REJECTED: return "REJECTED";
        case SponsorStatus::CANCELLED: return "CANCELLED";
        default: return "UNKNOWN";
    }
}

SponsorStatus SponsorStorage::stringToSponsorStatus(const std::string& status) {
    if (status == "PENDING") return SponsorStatus::PENDING;
    if (status == "VERIFIED") return SponsorStatus::VERIFIED;
    if (status == "REJECTED") return SponsorStatus::REJECTED;
    if (status == "CANCELLED") return SponsorStatus::CANCELLED;
    return SponsorStatus::PENDING; // Default
}

bsoncxx::document::value SponsorStorage::sponsorProfileToBson(const SponsorProfile& page) const {
    auto builder = document{};
    
    // Add ID if it exists
    if (page.id) {
        builder << "_id" << bsoncxx::oid{page.id.value()};
    }
    
    // Required fields
    builder << "fullName" << page.fullName
            << "email" << page.email
            << "mobile" << page.mobile
            << "plan" << page.plan
            << "amount" << page.amount;
    
    // Optional company field
    if (page.company) {
        builder << "company" << page.company.value();
    }
    
    // Backend tracking data
    builder << "ipAddress" << page.ipAddress
            << "userAgent" << page.userAgent
            << "submissionTime" << timePointToDate(page.submissionTime)
            << "lastModified" << timePointToDate(page.lastModified);
    
    // Status and processing
    builder << "status" << sponsorStatusToString(page.status);
    
    if (page.notes) {
        builder << "notes" << page.notes.value();
    }
    
    if (page.paymentReference) {
        builder << "paymentReference" << page.paymentReference.value();
    }
    
    if (page.paymentDate) {
        builder << "paymentDate" << timePointToDate(page.paymentDate.value());
    }
    
    // Financial tracking
    builder << "currency" << page.currency;
    
    if (page.bankAccountInfo) {
        builder << "bankAccountInfo" << page.bankAccountInfo.value();
    }
    
    if (page.transactionId) {
        builder << "transactionId" << page.transactionId.value();
    }
    
    return builder << finalize;
}

SponsorProfile SponsorStorage::bsonToSponsorProfile(const bsoncxx::document::view& doc) const {
    SponsorProfile page;
    
    // ID
    if (doc["_id"]) {
        page.id = doc["_id"].get_oid().value.to_string();
    }
    
    // Required fields
    page.fullName = std::string(doc["fullName"].get_string().value);
    page.email = std::string(doc["email"].get_string().value);
    page.mobile = std::string(doc["mobile"].get_string().value);
    page.plan = std::string(doc["plan"].get_string().value);
    
    if (doc["amount"].type() == bsoncxx::type::k_double) {
        page.amount = doc["amount"].get_double().value;
    } else if (doc["amount"].type() == bsoncxx::type::k_int32) {
        page.amount = static_cast<double>(doc["amount"].get_int32().value);
    } else if (doc["amount"].type() == bsoncxx::type::k_int64) {
        page.amount = static_cast<double>(doc["amount"].get_int64().value);
    }
    
    // Optional company field
    if (doc["company"]) {
        page.company = std::string(doc["company"].get_string().value);
    }
    
    // Backend tracking data
    page.ipAddress = std::string(doc["ipAddress"].get_string().value);
    page.userAgent = std::string(doc["userAgent"].get_string().value);
    page.submissionTime = dateToTimePoint(doc["submissionTime"].get_date());
    page.lastModified = dateToTimePoint(doc["lastModified"].get_date());
    
    // Status
    page.status = stringToSponsorStatus(std::string(doc["status"].get_string().value));
    
    // Optional fields
    if (doc["notes"]) {
        page.notes = std::string(doc["notes"].get_string().value);
    }
    
    if (doc["paymentReference"]) {
        page.paymentReference = std::string(doc["paymentReference"].get_string().value);
    }
    
    if (doc["paymentDate"]) {
        page.paymentDate = dateToTimePoint(doc["paymentDate"].get_date());
    }
    
    // Financial tracking
    page.currency = std::string(doc["currency"].get_string().value);
    
    if (doc["bankAccountInfo"]) {
        page.bankAccountInfo = std::string(doc["bankAccountInfo"].get_string().value);
    }
    
    if (doc["transactionId"]) {
        page.transactionId = std::string(doc["transactionId"].get_string().value);
    }
    
    return page;
}

void SponsorStorage::ensureIndexes() {
    try {
        // Create index on email (unique)
        sponsorCollection_.create_index(document{} << "email" << 1 << finalize);
        
        // Create index on status
        sponsorCollection_.create_index(document{} << "status" << 1 << finalize);
        
        // Create index on submission time
        sponsorCollection_.create_index(document{} << "submissionTime" << -1 << finalize);
        
        // Create compound index on status and submission time
        sponsorCollection_.create_index(document{} << "status" << 1 << "submissionTime" << -1 << finalize);
        
        LOG_INFO("SponsorStorage indexes created successfully");
    } catch (const mongocxx::exception& e) {
        LOG_WARNING("Failed to create SponsorStorage indexes (may already exist): " + std::string(e.what()));
    }
}

Result<std::string> SponsorStorage::store(const SponsorProfile& page) {
    try {
        auto doc = sponsorProfileToBson(page);
        auto result = sponsorCollection_.insert_one(doc.view());
        
        if (result) {
            std::string id = result->inserted_id().get_oid().value.to_string();
            LOG_INFO("Stored sponsor page with ID: " + id);
            return Result<std::string>::Success(id, "Sponsor page stored successfully");
        } else {
            LOG_ERROR("Failed to store sponsor page");
            return Result<std::string>::Failure("Failed to store sponsor page");
        }
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error storing sponsor page: " + std::string(e.what()));
        return Result<std::string>::Failure("Database error: " + std::string(e.what()));
    }
}

Result<SponsorProfile> SponsorStorage::findById(const std::string& id) {
    try {
        auto filter = document{} << "_id" << bsoncxx::oid{id} << finalize;
        auto result = sponsorCollection_.find_one(filter.view());
        
        if (result) {
            return Result<SponsorProfile>::Success(bsonToSponsorProfile(result->view()), "Sponsor page found");
        } else {
            return Result<SponsorProfile>::Failure("Sponsor page not found");
        }
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error finding sponsor page: " + std::string(e.what()));
        return Result<SponsorProfile>::Failure("Database error: " + std::string(e.what()));
    }
}

Result<std::optional<SponsorProfile>> SponsorStorage::findByEmail(const std::string& email) {
    try {
        auto filter = document{} << "email" << email << finalize;
        auto result = sponsorCollection_.find_one(filter.view());
        
        if (result) {
            return Result<std::optional<SponsorProfile>>::Success(bsonToSponsorProfile(result->view()), "Sponsor page found");
        } else {
            return Result<std::optional<SponsorProfile>>::Success(std::nullopt, "No sponsor found with this email");
        }
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error finding sponsor by email: " + std::string(e.what()));
        return Result<std::optional<SponsorProfile>>::Failure("Database error: " + std::string(e.what()));
    }
}

Result<bool> SponsorStorage::testConnection() {
    try {
        // Simple ping test
        auto result = database_.run_command(document{} << "ping" << 1 << finalize);
        return Result<bool>::Success(true, "Connection test successful");
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("SponsorStorage connection test failed: " + std::string(e.what()));
        return Result<bool>::Failure("Connection test failed: " + std::string(e.what()));
    }
}

Result<std::vector<SponsorProfile>> SponsorStorage::findByStatus(SponsorStatus status) {
    try {
        auto filter = document{} << "status" << sponsorStatusToString(status) << finalize;
        auto cursor = sponsorCollection_.find(filter.view());
        
        std::vector<SponsorProfile> profiles;
        for (auto&& doc : cursor) {
            profiles.push_back(bsonToSponsorProfile(doc));
        }
        
        return Result<std::vector<SponsorProfile>>::Success(profiles, "Found " + std::to_string(profiles.size()) + " sponsors");
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error finding sponsors by status: " + std::string(e.what()));
        return Result<std::vector<SponsorProfile>>::Failure("Database error: " + std::string(e.what()));
    }
}

Result<bool> SponsorStorage::updateStatus(const std::string& id, SponsorStatus status, const std::string& notes) {
    try {
        auto filter = document{} << "_id" << bsoncxx::oid{id} << finalize;
        
        // Build the update document
        auto updateBuilder = document{} << "status" << sponsorStatusToString(status)
                                       << "lastModified" << timePointToDate(std::chrono::system_clock::now());
        
        if (!notes.empty()) {
            updateBuilder << "notes" << notes;
        }
        
        auto updateDoc = updateBuilder << finalize;
        auto update = document{} << "$set" << updateDoc << finalize;
        
        auto result = sponsorCollection_.update_one(filter.view(), update.view());
        
        if (result && result->modified_count() > 0) {
            LOG_INFO("Updated sponsor status for ID: " + id);
            return Result<bool>::Success(true, "Sponsor status updated successfully");
        } else {
            return Result<bool>::Failure("No sponsor found with given ID or no changes made");
        }
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error updating sponsor status: " + std::string(e.what()));
        return Result<bool>::Failure("Database error: " + std::string(e.what()));
    }
}

} // namespace storage
} // namespace search_engine
