#ifndef WEBSITE_PROFILE_STORAGE_H
#define WEBSITE_PROFILE_STORAGE_H

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <mongocxx/client.hpp>
#include <mongocxx/pool.hpp>
#include <bsoncxx/json.hpp>
#include <nlohmann/json.hpp>
#include "../../include/infrastructure.h"

namespace search_engine {
namespace storage {

struct DateInfo {
    std::string persian;
    std::string gregorian;
};

struct Location {
    double latitude;
    double longitude;
};

struct BusinessService {
    std::string row_number;
    std::string service_title;
    std::string permit_issuer;
    std::string permit_number;
    std::string validity_start_date;
    std::string validity_end_date;
    std::string status;
};

struct DomainInfo {
    int page_number;
    int row_index;
    std::string row_number;
    std::string province;
    std::string city;
    std::string domain_url;
};

struct WebsiteProfile {
    std::string business_name;
    std::string website_url;
    std::string owner_name;
    DateInfo grant_date;
    DateInfo expiry_date;
    std::string address;
    std::string phone;
    std::string email;
    Location location;
    std::string business_experience;
    std::string business_hours;
    std::vector<BusinessService> business_services;
    std::string extraction_timestamp;
    DomainInfo domain_info;
    std::string created_at;
};

class WebsiteProfileStorage {
public:
    WebsiteProfileStorage();
    ~WebsiteProfileStorage() = default;

    // Save website profile to database
    Result<std::string> saveProfile(const WebsiteProfile& profile);
    
    // Get profile by website URL
    Result<WebsiteProfile> getProfileByUrl(const std::string& website_url);
    
    // Get all profiles
    Result<std::vector<WebsiteProfile>> getAllProfiles(int limit = 100, int skip = 0);
    
    // Update profile by website URL
    Result<bool> updateProfile(const std::string& website_url, const WebsiteProfile& profile);
    
    // Delete profile by website URL
    Result<bool> deleteProfile(const std::string& website_url);
    
    // Check if profile exists
    Result<bool> profileExists(const std::string& website_url);

private:
    std::unique_ptr<mongocxx::client> client_;
    
    // Convert WebsiteProfile to BSON document
    bsoncxx::document::value profileToBson(const WebsiteProfile& profile);
    
    // Convert BSON document to WebsiteProfile
    WebsiteProfile bsonToProfile(const bsoncxx::document::view& doc);
    
    // Helper to get current timestamp
    std::string getCurrentTimestamp();
};

} // namespace storage
} // namespace search_engine

#endif // WEBSITE_PROFILE_STORAGE_H

