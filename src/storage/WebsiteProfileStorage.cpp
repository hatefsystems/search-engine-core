#include "WebsiteProfileStorage.h"
#include "../../include/mongodb.h"
#include "../../include/Logger.h"
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <mongocxx/exception/exception.hpp>
#include <chrono>
#include <iomanip>
#include <sstream>

using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::array;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_document;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::close_array;

namespace search_engine {
namespace storage {

WebsiteProfileStorage::WebsiteProfileStorage() {
    try {
        // Use MongoDB singleton instance
        [[maybe_unused]] mongocxx::instance& instance = MongoDBInstance::getInstance();
        
        // Read MongoDB URI from environment or use default
        const char* mongoUri = std::getenv("MONGODB_URI");
        std::string uri = mongoUri ? mongoUri : "mongodb://admin:password123@mongodb:27017";
        
        LOG_INFO("Initializing WebsiteProfileStorage with MongoDB URI: " + uri);
        
        mongocxx::uri mongo_uri{uri};
        client_ = std::make_unique<mongocxx::client>(mongo_uri);
        
        // Test connection
        auto db = (*client_)["search-engine"];
        auto collection = db["website_profile"];
        
        LOG_INFO("WebsiteProfileStorage initialized successfully");
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("Failed to initialize WebsiteProfileStorage: " + std::string(e.what()));
        throw;
    }
}

std::string WebsiteProfileStorage::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    
    return ss.str();
}

bsoncxx::document::value WebsiteProfileStorage::profileToBson(const WebsiteProfile& profile) {
    auto builder = document{};
    
    builder << "business_name" << profile.business_name
            << "website_url" << profile.website_url
            << "owner_name" << profile.owner_name
            << "grant_date" << open_document
                << "persian" << profile.grant_date.persian
                << "gregorian" << profile.grant_date.gregorian
            << close_document
            << "expiry_date" << open_document
                << "persian" << profile.expiry_date.persian
                << "gregorian" << profile.expiry_date.gregorian
            << close_document
            << "address" << profile.address
            << "phone" << profile.phone
            << "email" << profile.email
            << "location" << open_document
                << "latitude" << profile.location.latitude
                << "longitude" << profile.location.longitude
            << close_document
            << "business_experience" << profile.business_experience
            << "business_hours" << profile.business_hours;
    
    // Add business_services array
    auto services_array = bsoncxx::builder::stream::array{};
    for (const auto& service : profile.business_services) {
        services_array << open_document
            << "row_number" << service.row_number
            << "service_title" << service.service_title
            << "permit_issuer" << service.permit_issuer
            << "permit_number" << service.permit_number
            << "validity_start_date" << service.validity_start_date
            << "validity_end_date" << service.validity_end_date
            << "status" << service.status
        << close_document;
    }
    builder << "business_services" << services_array;
    
    builder << "extraction_timestamp" << profile.extraction_timestamp
            << "domain_info" << open_document
                << "page_number" << profile.domain_info.page_number
                << "row_index" << profile.domain_info.row_index
                << "row_number" << profile.domain_info.row_number
                << "province" << profile.domain_info.province
                << "city" << profile.domain_info.city
                << "domain_url" << profile.domain_info.domain_url
            << close_document
            << "created_at" << profile.created_at;
    
    return builder << finalize;
}

WebsiteProfile WebsiteProfileStorage::bsonToProfile(const bsoncxx::document::view& doc) {
    WebsiteProfile profile;
    
    if (doc["business_name"]) {
        profile.business_name = std::string(doc["business_name"].get_string().value);
    }
    if (doc["website_url"]) {
        profile.website_url = std::string(doc["website_url"].get_string().value);
    }
    if (doc["owner_name"]) {
        profile.owner_name = std::string(doc["owner_name"].get_string().value);
    }
    
    // Parse grant_date
    if (doc["grant_date"]) {
        auto grant_date_doc = doc["grant_date"].get_document().view();
        if (grant_date_doc["persian"]) {
            profile.grant_date.persian = std::string(grant_date_doc["persian"].get_string().value);
        }
        if (grant_date_doc["gregorian"]) {
            profile.grant_date.gregorian = std::string(grant_date_doc["gregorian"].get_string().value);
        }
    }
    
    // Parse expiry_date
    if (doc["expiry_date"]) {
        auto expiry_date_doc = doc["expiry_date"].get_document().view();
        if (expiry_date_doc["persian"]) {
            profile.expiry_date.persian = std::string(expiry_date_doc["persian"].get_string().value);
        }
        if (expiry_date_doc["gregorian"]) {
            profile.expiry_date.gregorian = std::string(expiry_date_doc["gregorian"].get_string().value);
        }
    }
    
    if (doc["address"]) {
        profile.address = std::string(doc["address"].get_string().value);
    }
    if (doc["phone"]) {
        profile.phone = std::string(doc["phone"].get_string().value);
    }
    if (doc["email"]) {
        profile.email = std::string(doc["email"].get_string().value);
    }
    
    // Parse location
    if (doc["location"]) {
        auto location_doc = doc["location"].get_document().view();
        if (location_doc["latitude"]) {
            profile.location.latitude = location_doc["latitude"].get_double().value;
        }
        if (location_doc["longitude"]) {
            profile.location.longitude = location_doc["longitude"].get_double().value;
        }
    }
    
    if (doc["business_experience"]) {
        profile.business_experience = std::string(doc["business_experience"].get_string().value);
    }
    if (doc["business_hours"]) {
        profile.business_hours = std::string(doc["business_hours"].get_string().value);
    }
    
    // Parse business_services array
    if (doc["business_services"]) {
        auto services_array = doc["business_services"].get_array().value;
        for (const auto& service_element : services_array) {
            if (service_element.type() == bsoncxx::type::k_document) {
                auto service_doc = service_element.get_document().view();
                BusinessService service;
                
                if (service_doc["row_number"]) {
                    service.row_number = std::string(service_doc["row_number"].get_string().value);
                }
                if (service_doc["service_title"]) {
                    service.service_title = std::string(service_doc["service_title"].get_string().value);
                }
                if (service_doc["permit_issuer"]) {
                    service.permit_issuer = std::string(service_doc["permit_issuer"].get_string().value);
                }
                if (service_doc["permit_number"]) {
                    service.permit_number = std::string(service_doc["permit_number"].get_string().value);
                }
                if (service_doc["validity_start_date"]) {
                    service.validity_start_date = std::string(service_doc["validity_start_date"].get_string().value);
                }
                if (service_doc["validity_end_date"]) {
                    service.validity_end_date = std::string(service_doc["validity_end_date"].get_string().value);
                }
                if (service_doc["status"]) {
                    service.status = std::string(service_doc["status"].get_string().value);
                }
                
                profile.business_services.push_back(service);
            }
        }
    }
    
    if (doc["extraction_timestamp"]) {
        profile.extraction_timestamp = std::string(doc["extraction_timestamp"].get_string().value);
    }
    
    // Parse domain_info
    if (doc["domain_info"]) {
        auto domain_info_doc = doc["domain_info"].get_document().view();
        if (domain_info_doc["page_number"]) {
            profile.domain_info.page_number = domain_info_doc["page_number"].get_int32().value;
        }
        if (domain_info_doc["row_index"]) {
            profile.domain_info.row_index = domain_info_doc["row_index"].get_int32().value;
        }
        if (domain_info_doc["row_number"]) {
            profile.domain_info.row_number = std::string(domain_info_doc["row_number"].get_string().value);
        }
        if (domain_info_doc["province"]) {
            profile.domain_info.province = std::string(domain_info_doc["province"].get_string().value);
        }
        if (domain_info_doc["city"]) {
            profile.domain_info.city = std::string(domain_info_doc["city"].get_string().value);
        }
        if (domain_info_doc["domain_url"]) {
            profile.domain_info.domain_url = std::string(domain_info_doc["domain_url"].get_string().value);
        }
    }
    
    if (doc["created_at"]) {
        profile.created_at = std::string(doc["created_at"].get_string().value);
    }
    
    return profile;
}

Result<std::string> WebsiteProfileStorage::saveProfile(const WebsiteProfile& profile) {
    try {
        auto db = (*client_)["search-engine"];
        auto collection = db["website_profile"];
        
        // Check if profile already exists
        auto filter = document{} << "website_url" << profile.website_url << finalize;
        auto existing = collection.find_one(filter.view());
        
        if (existing) {
            LOG_WARNING("Profile already exists for website_url: " + profile.website_url);
            return Result<std::string>::Failure("Profile with this website URL already exists");
        }
        
        // Create profile with timestamp
        WebsiteProfile profileWithTimestamp = profile;
        if (profileWithTimestamp.created_at.empty()) {
            profileWithTimestamp.created_at = getCurrentTimestamp();
        }
        
        // Convert to BSON
        auto doc = profileToBson(profileWithTimestamp);
        
        // Insert into database
        auto result = collection.insert_one(doc.view());
        
        if (result) {
            LOG_INFO("Website profile saved successfully: " + profile.website_url);
            return Result<std::string>::Success(profile.website_url, "Profile saved successfully");
        } else {
            LOG_ERROR("Failed to save website profile: " + profile.website_url);
            return Result<std::string>::Failure("Failed to save profile to database");
        }
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error while saving profile: " + std::string(e.what()));
        return Result<std::string>::Failure("Database error: " + std::string(e.what()));
    } catch (const std::exception& e) {
        LOG_ERROR("Error saving profile: " + std::string(e.what()));
        return Result<std::string>::Failure("Error: " + std::string(e.what()));
    }
}

Result<WebsiteProfile> WebsiteProfileStorage::getProfileByUrl(const std::string& website_url) {
    try {
        auto db = (*client_)["search-engine"];
        auto collection = db["website_profile"];
        
        auto filter = document{} << "website_url" << website_url << finalize;
        auto result = collection.find_one(filter.view());
        
        if (result) {
            auto profile = bsonToProfile(result->view());
            LOG_DEBUG("Found website profile: " + website_url);
            return Result<WebsiteProfile>::Success(profile, "Profile found");
        } else {
            LOG_DEBUG("Website profile not found: " + website_url);
            return Result<WebsiteProfile>::Failure("Profile not found");
        }
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error while getting profile: " + std::string(e.what()));
        return Result<WebsiteProfile>::Failure("Database error: " + std::string(e.what()));
    } catch (const std::exception& e) {
        LOG_ERROR("Error getting profile: " + std::string(e.what()));
        return Result<WebsiteProfile>::Failure("Error: " + std::string(e.what()));
    }
}

Result<std::vector<WebsiteProfile>> WebsiteProfileStorage::getAllProfiles(int limit, int skip) {
    try {
        auto db = (*client_)["search-engine"];
        auto collection = db["website_profile"];
        
        mongocxx::options::find opts{};
        opts.limit(limit);
        opts.skip(skip);
        opts.sort(document{} << "created_at" << -1 << finalize);
        
        auto cursor = collection.find({}, opts);
        
        std::vector<WebsiteProfile> profiles;
        for (const auto& doc : cursor) {
            profiles.push_back(bsonToProfile(doc));
        }
        
        LOG_DEBUG("Retrieved " + std::to_string(profiles.size()) + " website profiles");
        return Result<std::vector<WebsiteProfile>>::Success(profiles, "Profiles retrieved successfully");
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error while getting profiles: " + std::string(e.what()));
        return Result<std::vector<WebsiteProfile>>::Failure("Database error: " + std::string(e.what()));
    } catch (const std::exception& e) {
        LOG_ERROR("Error getting profiles: " + std::string(e.what()));
        return Result<std::vector<WebsiteProfile>>::Failure("Error: " + std::string(e.what()));
    }
}

Result<bool> WebsiteProfileStorage::updateProfile(const std::string& website_url, const WebsiteProfile& profile) {
    try {
        auto db = (*client_)["search-engine"];
        auto collection = db["website_profile"];
        
        auto filter = document{} << "website_url" << website_url << finalize;
        auto update_doc = document{} << "$set" << profileToBson(profile) << finalize;
        
        auto result = collection.update_one(filter.view(), update_doc.view());
        
        if (result && result->modified_count() > 0) {
            LOG_INFO("Website profile updated successfully: " + website_url);
            return Result<bool>::Success(true, "Profile updated successfully");
        } else {
            LOG_WARNING("No profile found to update: " + website_url);
            return Result<bool>::Failure("Profile not found or no changes made");
        }
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error while updating profile: " + std::string(e.what()));
        return Result<bool>::Failure("Database error: " + std::string(e.what()));
    } catch (const std::exception& e) {
        LOG_ERROR("Error updating profile: " + std::string(e.what()));
        return Result<bool>::Failure("Error: " + std::string(e.what()));
    }
}

Result<bool> WebsiteProfileStorage::deleteProfile(const std::string& website_url) {
    try {
        auto db = (*client_)["search-engine"];
        auto collection = db["website_profile"];
        
        auto filter = document{} << "website_url" << website_url << finalize;
        auto result = collection.delete_one(filter.view());
        
        if (result && result->deleted_count() > 0) {
            LOG_INFO("Website profile deleted successfully: " + website_url);
            return Result<bool>::Success(true, "Profile deleted successfully");
        } else {
            LOG_WARNING("No profile found to delete: " + website_url);
            return Result<bool>::Failure("Profile not found");
        }
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error while deleting profile: " + std::string(e.what()));
        return Result<bool>::Failure("Database error: " + std::string(e.what()));
    } catch (const std::exception& e) {
        LOG_ERROR("Error deleting profile: " + std::string(e.what()));
        return Result<bool>::Failure("Error: " + std::string(e.what()));
    }
}

Result<bool> WebsiteProfileStorage::profileExists(const std::string& website_url) {
    try {
        auto db = (*client_)["search-engine"];
        auto collection = db["website_profile"];
        
        auto filter = document{} << "website_url" << website_url << finalize;
        auto count = collection.count_documents(filter.view());
        
        bool exists = count > 0;
        LOG_DEBUG("Profile exists check for " + website_url + ": " + (exists ? "true" : "false"));
        return Result<bool>::Success(exists, exists ? "Profile exists" : "Profile does not exist");
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error while checking profile existence: " + std::string(e.what()));
        return Result<bool>::Failure("Database error: " + std::string(e.what()));
    } catch (const std::exception& e) {
        LOG_ERROR("Error checking profile existence: " + std::string(e.what()));
        return Result<bool>::Failure("Error: " + std::string(e.what()));
    }
}

} // namespace storage
} // namespace search_engine

