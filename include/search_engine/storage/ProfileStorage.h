#pragma once

#include "Profile.h"
#include "../../infrastructure.h"
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <bsoncxx/document/value.hpp>
#include <bsoncxx/document/view.hpp>
#include <memory>
#include <vector>
#include <optional>
#include <regex>

namespace search_engine {
namespace storage {

class ProfileStorage {
private:
    std::unique_ptr<mongocxx::client> client_;
    mongocxx::database database_;
    mongocxx::collection profileCollection_;
    std::string encryptionKey_;  // Loaded from COMPLIANCE_ENCRYPTION_KEY env

    // Conversion methods between Profile and BSON
    bsoncxx::document::value profileToBson(const Profile& profile) const;
    Profile bsonToProfile(const bsoncxx::document::view& doc) const;

    // Conversion methods for PersonProfile and BSON
    bsoncxx::document::value profileToBson(const PersonProfile& profile) const;
    PersonProfile bsonToPersonProfile(const bsoncxx::document::view& doc) const;

    // Conversion methods for BusinessProfile and BSON
    bsoncxx::document::value profileToBson(const BusinessProfile& profile) const;
    BusinessProfile bsonToBusinessProfile(const bsoncxx::document::view& doc) const;

    // Helper methods for BSON conversion
    static std::string profileTypeToString(ProfileType type);
    static ProfileType stringToProfileType(const std::string& type);

    // Ensure indexes are created
    void ensureIndexes();

    // Helper to convert system_clock::time_point to bsoncxx::types::b_date
    static bsoncxx::types::b_date timePointToDate(const std::chrono::system_clock::time_point& tp);

    // Helper to convert bsoncxx::types::b_date to system_clock::time_point
    static std::chrono::system_clock::time_point dateToTimePoint(const bsoncxx::types::b_date& date);

public:
    // Constructor with connection string
    explicit ProfileStorage(const std::string& connectionString = "mongodb://localhost:27017",
                           const std::string& databaseName = "search-engine");

    // Destructor
    ~ProfileStorage() = default;

    // Move constructor and assignment (delete copy operations for RAII)
    ProfileStorage(ProfileStorage&& other) noexcept = default;
    ProfileStorage& operator=(ProfileStorage&& other) noexcept = default;
    ProfileStorage(const ProfileStorage&) = delete;
    ProfileStorage& operator=(const ProfileStorage&) = delete;

    // Core storage operations (base Profile)
    Result<std::string> store(const Profile& profile);
    Result<Profile> findById(const std::string& id);
    Result<std::optional<Profile>> findBySlug(const std::string& slug);
    Result<bool> update(const Profile& profile);
    Result<bool> deleteProfile(const std::string& id);

    // PersonProfile storage operations
    Result<std::string> store(const PersonProfile& profile);
    Result<std::optional<PersonProfile>> findPersonById(const std::string& id);
    Result<std::optional<PersonProfile>> findPersonBySlug(const std::string& slug);
    Result<bool> update(const PersonProfile& profile);

    // BusinessProfile storage operations
    Result<std::string> store(const BusinessProfile& profile);
    Result<std::optional<BusinessProfile>> findBusinessById(const std::string& id);
    Result<std::optional<BusinessProfile>> findBusinessBySlug(const std::string& slug);
    Result<bool> update(const BusinessProfile& profile);

    // Validation operations
    static bool isValidSlug(const std::string& slug);

    // Slug management operations
    Result<bool> checkSlugAvailability(const std::string& slug);
    Result<std::optional<Profile>> findByHandle(const std::string& handle);
    Result<bool> updateSlug(const std::string& profileId, const std::string& newSlug);

    // Query operations
    Result<std::vector<Profile>> findAll(int limit = 100, int skip = 0);
    Result<std::vector<Profile>> findByType(ProfileType type, int limit = 100, int skip = 0);
    Result<long> countByType(ProfileType type);

    // Connection test
    Result<bool> testConnection();
};

} // namespace storage
} // namespace search_engine
