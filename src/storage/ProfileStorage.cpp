#include "../../include/search_engine/storage/ProfileStorage.h"
#include "../../include/search_engine/storage/DataEncryption.h"
#include "../../include/search_engine/common/SlugGenerator.h"
#include "../../include/Logger.h"
#include "../../include/mongodb.h"
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/exception/exception.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/helpers.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>
#include <regex>

using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_document;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::close_array;

namespace search_engine {
namespace storage {

// Helper to convert system_clock::time_point to bsoncxx::types::b_date
bsoncxx::types::b_date ProfileStorage::timePointToDate(const std::chrono::system_clock::time_point& tp) {
    auto duration = tp.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    return bsoncxx::types::b_date{millis};
}

// Helper to convert bsoncxx::types::b_date to system_clock::time_point
std::chrono::system_clock::time_point ProfileStorage::dateToTimePoint(const bsoncxx::types::b_date& date) {
    return std::chrono::system_clock::time_point{date.value};
}

ProfileStorage::ProfileStorage(const std::string& connectionString, const std::string& databaseName) {
    LOG_DEBUG("ProfileStorage constructor called with database: " + databaseName);
    try {
        // Load encryption key for sensitive PII fields
        encryptionKey_ = DataEncryption::getEncryptionKey();
        LOG_INFO("Encryption key loaded for PII protection");
        
        // Use MONGODB_URI environment variable if available (for Docker), otherwise use provided connectionString
        std::string actualConnectionString = connectionString;
        const char* envUri = std::getenv("MONGODB_URI");
        if (envUri) {
            actualConnectionString = std::string(envUri);
            LOG_DEBUG("Using MONGODB_URI from environment: " + actualConnectionString);
        }

        LOG_INFO("Initializing MongoDB connection to: " + actualConnectionString);

        // Use the existing MongoDB instance singleton
        mongocxx::instance& instance = MongoDBInstance::getInstance();
        (void)instance; // Suppress unused variable warning

        // Create client and connect to database
        mongocxx::uri uri{actualConnectionString};
        client_ = std::make_unique<mongocxx::client>(uri);
        database_ = (*client_)[databaseName];
        profileCollection_ = database_["profiles"];
        LOG_INFO("Connected to MongoDB database: " + databaseName);

        // Ensure indexes are created
        ensureIndexes();
        LOG_DEBUG("ProfileStorage indexes ensured");

    } catch (const mongocxx::exception& e) {
        LOG_ERROR("Failed to initialize ProfileStorage MongoDB connection: " + std::string(e.what()));
        throw std::runtime_error("Failed to initialize ProfileStorage MongoDB connection: " + std::string(e.what()));
    } catch (const std::runtime_error& e) {
        LOG_ERROR("Failed to load encryption key: " + std::string(e.what()));
        throw;
    }
}

std::string ProfileStorage::profileTypeToString(ProfileType type) {
    switch (type) {
        case ProfileType::PERSON: return "PERSON";
        case ProfileType::BUSINESS: return "BUSINESS";
        default: return "UNKNOWN";
    }
}

ProfileType ProfileStorage::stringToProfileType(const std::string& type) {
    if (type == "PERSON") return ProfileType::PERSON;
    if (type == "BUSINESS") return ProfileType::BUSINESS;
    return ProfileType::PERSON; // Default
}

bool ProfileStorage::isValidSlug(const std::string& slug) {
    if (slug.empty()) return false;

    // Enforce maximum length
    if (slug.length() > 100) return false;

    // Regex pattern for Persian letters (U+0600-U+06FF) + English letters + numbers + hyphens
    // Pattern: ^[\u0600-\u06FFa-zA-Z0-9-]+$
    static const std::regex slugRegex("^[\u0600-\u06FFa-zA-Z0-9-]+$", std::regex_constants::extended);

    return std::regex_match(slug, slugRegex);
}

bsoncxx::document::value ProfileStorage::profileToBson(const Profile& profile) const {
    using bsoncxx::builder::basic::kvp;
    using bsoncxx::builder::basic::make_array;
    
    auto builder = bsoncxx::builder::basic::document{};

    // Add ID if it exists
    if (profile.id) {
        builder.append(kvp("_id", bsoncxx::oid{profile.id.value()}));
    }

    // Required fields (support UTF-8)
    builder.append(kvp("slug", profile.slug));
    builder.append(kvp("name", profile.name));
    builder.append(kvp("type", profileTypeToString(profile.type)));
    builder.append(kvp("isPublic", profile.isPublic));
    builder.append(kvp("createdAt", timePointToDate(profile.createdAt)));

    // Optional bio field
    if (profile.bio) {
        builder.append(kvp("bio", profile.bio.value()));
    }

    // Optional previousSlugs array for SEO redirects
    if (profile.previousSlugs && !profile.previousSlugs->empty()) {
        auto slugsArray = bsoncxx::builder::basic::array{};
        for (const auto& slug : profile.previousSlugs.value()) {
            slugsArray.append(slug);
        }
        builder.append(kvp("previousSlugs", slugsArray));
    }

    // Optional slugChangedAt timestamp
    if (profile.slugChangedAt) {
        builder.append(kvp("slugChangedAt", timePointToDate(profile.slugChangedAt.value())));
    }

    // Optional updatedAt timestamp
    if (profile.updatedAt) {
        builder.append(kvp("updatedAt", timePointToDate(profile.updatedAt.value())));
    }

    // Optional ownership fields
    if (profile.ownerToken) {
        builder.append(kvp("ownerToken", profile.ownerToken.value()));
    }
    if (profile.ownerId) {
        builder.append(kvp("ownerId", profile.ownerId.value()));
    }

    // Optional deletedAt timestamp (soft delete)
    if (profile.deletedAt) {
        builder.append(kvp("deletedAt", timePointToDate(profile.deletedAt.value())));
    }

    return builder.extract();
}

Profile ProfileStorage::bsonToProfile(const bsoncxx::document::view& doc) const {
    Profile profile;

    // ID
    if (doc["_id"]) {
        profile.id = doc["_id"].get_oid().value.to_string();
    }

    // Required fields (UTF-8 strings)
    profile.slug = std::string(doc["slug"].get_string().value);
    profile.name = std::string(doc["name"].get_string().value);
    profile.type = stringToProfileType(std::string(doc["type"].get_string().value));
    profile.isPublic = doc["isPublic"].get_bool().value;
    profile.createdAt = dateToTimePoint(doc["createdAt"].get_date());

    // Optional bio field
    if (doc["bio"]) {
        profile.bio = std::string(doc["bio"].get_string().value);
    }

    // Optional previousSlugs array for SEO redirects
    if (doc["previousSlugs"] && doc["previousSlugs"].type() == bsoncxx::type::k_array) {
        std::vector<std::string> slugs;
        auto slugsArray = doc["previousSlugs"].get_array();
        for (const auto& element : slugsArray.value) {
            if (element.type() == bsoncxx::type::k_string) {
                slugs.push_back(std::string(element.get_string().value));
            }
        }
        if (!slugs.empty()) {
            profile.previousSlugs = slugs;
        }
    }

    // Optional slugChangedAt timestamp
    if (doc["slugChangedAt"]) {
        profile.slugChangedAt = dateToTimePoint(doc["slugChangedAt"].get_date());
    }

    // Optional updatedAt timestamp
    if (doc["updatedAt"]) {
        profile.updatedAt = dateToTimePoint(doc["updatedAt"].get_date());
    }

    // Optional ownership fields
    if (doc["ownerToken"]) {
        profile.ownerToken = std::string(doc["ownerToken"].get_string().value);
    }
    if (doc["ownerId"]) {
        profile.ownerId = std::string(doc["ownerId"].get_string().value);
    }

    // Optional deletedAt timestamp (soft delete)
    if (doc["deletedAt"]) {
        profile.deletedAt = dateToTimePoint(doc["deletedAt"].get_date());
    }

    return profile;
}

void ProfileStorage::ensureIndexes() {
    try {
        using bsoncxx::builder::basic::kvp;
        using bsoncxx::builder::basic::make_document;
        
        // 1. slug_unique: Unique index on slug for fast lookups
        {
            mongocxx::options::index opts{};
            opts.unique(true);
            opts.name("slug_unique");
            profileCollection_.create_index(document{} << "slug" << 1 << finalize, opts);
        }

        // 2. type_index: Filter profiles by type
        {
            mongocxx::options::index opts{};
            opts.name("type_index");
            profileCollection_.create_index(document{} << "type" << 1 << finalize, opts);
        }

        // 3. created_at_index: Sort profiles by creation date (descending for recent first)
        {
            mongocxx::options::index opts{};
            opts.name("created_at_index");
            profileCollection_.create_index(document{} << "createdAt" << -1 << finalize, opts);
        }

        // 4. public_filter: Filter for public profiles
        {
            mongocxx::options::index opts{};
            opts.name("public_filter");
            profileCollection_.create_index(document{} << "isPublic" << 1 << finalize, opts);
        }

        // 5. type_public_recent: Compound index for listing public profiles by type, sorted by recency
        {
            mongocxx::options::index opts{};
            opts.name("type_public_recent");
            profileCollection_.create_index(
                document{} << "type" << 1 << "isPublic" << 1 << "createdAt" << -1 << finalize,
                opts
            );
        }

        // 6. person_skills: Partial index for PERSON profiles with skills
        {
            mongocxx::options::index opts{};
            opts.name("person_skills");
            // Partial filter: only index documents where type is PERSON
            auto partialFilter = make_document(kvp("type", "PERSON"));
            opts.partial_filter_expression(partialFilter.view());
            profileCollection_.create_index(document{} << "skills" << 1 << finalize, opts);
        }

        // 7. business_location_industry: Partial index for BUSINESS profiles by industry + city
        {
            mongocxx::options::index opts{};
            opts.name("business_location_industry");
            // Partial filter: only index documents where type is BUSINESS
            auto partialFilter = make_document(kvp("type", "BUSINESS"));
            opts.partial_filter_expression(partialFilter.view());
            profileCollection_.create_index(
                document{} << "industry" << 1 << "city" << 1 << finalize,
                opts
            );
        }

        // 8. previous_slugs: Index for SEO redirect lookups on previousSlugs array
        {
            mongocxx::options::index opts{};
            opts.name("previous_slugs");
            profileCollection_.create_index(document{} << "previousSlugs" << 1 << finalize, opts);
        }

        LOG_INFO("ProfileStorage indexes created successfully with named indexes");
    } catch (const mongocxx::exception& e) {
        LOG_WARNING("Failed to create ProfileStorage indexes (may already exist): " + std::string(e.what()));
    }
}

Result<std::string> ProfileStorage::store(const Profile& profile) {
    try {
        // Validate slug format
        if (!isValidSlug(profile.slug)) {
            return Result<std::string>::Failure("Invalid slug format. Slug must contain only Persian letters, English letters, numbers, and hyphens.");
        }

        // Check if slug is reserved
        if (search_engine::common::SlugGenerator::isReservedSlug(profile.slug)) {
            return Result<std::string>::Failure("Slug '" + profile.slug + "' is reserved and cannot be used.");
        }

        // Check for slug uniqueness
        auto existingResult = findBySlug(profile.slug);
        if (existingResult.success && existingResult.value.has_value()) {
            return Result<std::string>::Failure("Slug '" + profile.slug + "' is already taken.");
        }

        auto doc = profileToBson(profile);

        try {
            auto result = profileCollection_.insert_one(doc.view());

            if (result) {
                std::string id = result->inserted_id().get_oid().value.to_string();
                LOG_INFO("Stored profile with ID: " + id + ", slug: " + profile.slug);
                return Result<std::string>::Success(id, "Profile stored successfully");
            } else {
                LOG_ERROR("Failed to store profile");
                return Result<std::string>::Failure("Failed to store profile");
            }
        } catch (const mongocxx::exception& e) {
            // Handle duplicate key error from unique index (TOCTOU race condition)
            std::string errMsg = e.what();
            if (errMsg.find("duplicate key") != std::string::npos ||
                errMsg.find("E11000") != std::string::npos) {
                LOG_WARNING("Duplicate slug detected via unique index: " + profile.slug);
                return Result<std::string>::Failure("Slug '" + profile.slug + "' is already taken.");
            }
            throw; // Re-throw non-duplicate-key exceptions
        }
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error storing profile: " + std::string(e.what()));
        return Result<std::string>::Failure("Database error: " + std::string(e.what()));
    }
}

Result<Profile> ProfileStorage::findById(const std::string& id) {
    try {
        // Filter: match ID and exclude soft-deleted profiles
        auto filter = document{} 
            << "_id" << bsoncxx::oid{id}
            << "deletedAt" << open_document << "$exists" << false << close_document
            << finalize;
        auto result = profileCollection_.find_one(filter.view());

        if (result) {
            return Result<Profile>::Success(bsonToProfile(result->view()), "Profile found");
        } else {
            return Result<Profile>::Failure("Profile not found");
        }
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error finding profile: " + std::string(e.what()));
        return Result<Profile>::Failure("Database error: " + std::string(e.what()));
    }
}

Result<std::optional<Profile>> ProfileStorage::findBySlug(const std::string& slug) {
    try {
        // Filter: match slug and exclude soft-deleted profiles
        auto filter = document{} 
            << "slug" << slug
            << "deletedAt" << open_document << "$exists" << false << close_document
            << finalize;
        auto result = profileCollection_.find_one(filter.view());

        if (result) {
            return Result<std::optional<Profile>>::Success(std::optional<Profile>(bsonToProfile(result->view())), "Profile found");
        } else {
            return Result<std::optional<Profile>>::Success(std::nullopt, "No profile found with this slug");
        }
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error finding profile by slug: " + std::string(e.what()));
        return Result<std::optional<Profile>>::Failure("Database error: " + std::string(e.what()));
    }
}

Result<bool> ProfileStorage::update(const Profile& profile) {
    try {
        if (!profile.id) {
            return Result<bool>::Failure("Profile ID is required for update");
        }

        // Validate slug format if slug is being updated
        if (!isValidSlug(profile.slug)) {
            return Result<bool>::Failure("Invalid slug format. Slug must contain only Persian letters, English letters, numbers, and hyphens.");
        }

        // Check if slug is reserved
        if (search_engine::common::SlugGenerator::isReservedSlug(profile.slug)) {
            return Result<bool>::Failure("Slug '" + profile.slug + "' is reserved and cannot be used.");
        }

        auto filter = document{} << "_id" << bsoncxx::oid{profile.id.value()} << finalize;

        // For update, we need to check slug uniqueness (excluding current profile)
        auto existingResult = findBySlug(profile.slug);
        if (existingResult.success && existingResult.value.has_value() &&
            existingResult.value.value().id != profile.id) {
            return Result<bool>::Failure("Slug '" + profile.slug + "' is already taken by another profile.");
        }

        // Create mutable copy and set updatedAt timestamp
        Profile updatedProfile = profile;
        updatedProfile.updatedAt = std::chrono::system_clock::now();

        auto updateDoc = profileToBson(updatedProfile);
        
        // Use basic builder for $set to prevent data corruption
        using bsoncxx::builder::basic::kvp;
        auto update = bsoncxx::builder::basic::document{};
        update.append(kvp("$set", updateDoc));

        auto result = profileCollection_.update_one(filter.view(), update.extract());

        if (result && result->modified_count() > 0) {
            LOG_INFO("Updated profile with ID: " + profile.id.value());
            return Result<bool>::Success(true, "Profile updated successfully");
        } else {
            return Result<bool>::Failure("No profile found with given ID or no changes made");
        }
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error updating profile: " + std::string(e.what()));
        return Result<bool>::Failure("Database error: " + std::string(e.what()));
    }
}

Result<bool> ProfileStorage::deleteProfile(const std::string& id) {
    try {
        auto filter = document{} << "_id" << bsoncxx::oid{id} << finalize;
        
        // Soft delete: set deletedAt and updatedAt timestamps
        auto now = std::chrono::system_clock::now();
        auto update = document{} 
            << "$set" << open_document
                << "deletedAt" << timePointToDate(now)
                << "updatedAt" << timePointToDate(now)
            << close_document
            << finalize;
        
        auto result = profileCollection_.update_one(filter.view(), update.view());

        if (result && result->modified_count() > 0) {
            LOG_INFO("Soft deleted profile with ID: " + id);
            return Result<bool>::Success(true, "Profile deleted successfully");
        } else {
            return Result<bool>::Failure("No profile found with given ID");
        }
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error deleting profile: " + std::string(e.what()));
        return Result<bool>::Failure("Database error: " + std::string(e.what()));
    }
}

Result<bool> ProfileStorage::restoreProfile(const std::string& id) {
    try {
        auto filter = document{} << "_id" << bsoncxx::oid{id} << finalize;
        
        // Restore: unset deletedAt and update updatedAt
        auto now = std::chrono::system_clock::now();
        auto update = document{} 
            << "$unset" << open_document
                << "deletedAt" << ""
            << close_document
            << "$set" << open_document
                << "updatedAt" << timePointToDate(now)
            << close_document
            << finalize;
        
        auto result = profileCollection_.update_one(filter.view(), update.view());

        if (result && result->modified_count() > 0) {
            LOG_INFO("Restored profile with ID: " + id);
            return Result<bool>::Success(true, "Profile restored successfully");
        } else {
            return Result<bool>::Failure("No profile found with given ID or profile was not deleted");
        }
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error restoring profile: " + std::string(e.what()));
        return Result<bool>::Failure("Database error: " + std::string(e.what()));
    }
}

Result<std::vector<Profile>> ProfileStorage::findAll(int limit, int skip) {
    try {
        // Filter out soft-deleted profiles
        auto filter = document{} 
            << "deletedAt" << open_document << "$exists" << false << close_document
            << finalize;
        
        mongocxx::options::find findOptions{};
        findOptions.limit(limit).skip(skip);

        auto cursor = profileCollection_.find(filter.view(), findOptions);

        std::vector<Profile> profiles;
        for (auto&& doc : cursor) {
            profiles.push_back(bsonToProfile(doc));
        }

        return Result<std::vector<Profile>>::Success(profiles, "Found " + std::to_string(profiles.size()) + " profiles");
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error finding all profiles: " + std::string(e.what()));
        return Result<std::vector<Profile>>::Failure("Database error: " + std::string(e.what()));
    }
}

Result<std::vector<Profile>> ProfileStorage::findByType(ProfileType type, int limit, int skip) {
    try {
        // Filter: match type and exclude soft-deleted profiles
        auto filter = document{} 
            << "type" << profileTypeToString(type)
            << "deletedAt" << open_document << "$exists" << false << close_document
            << finalize;

        mongocxx::options::find findOptions{};
        findOptions.limit(limit).skip(skip);

        auto cursor = profileCollection_.find(filter.view(), findOptions);

        std::vector<Profile> profiles;
        for (auto&& doc : cursor) {
            profiles.push_back(bsonToProfile(doc));
        }

        return Result<std::vector<Profile>>::Success(profiles, "Found " + std::to_string(profiles.size()) + " profiles");
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error finding profiles by type: " + std::string(e.what()));
        return Result<std::vector<Profile>>::Failure("Database error: " + std::string(e.what()));
    }
}

Result<long> ProfileStorage::countByType(ProfileType type) {
    try {
        auto filter = document{} << "type" << profileTypeToString(type) << finalize;
        auto count = profileCollection_.count_documents(filter.view());

        return Result<long>::Success(count, "Count successful");
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error counting profiles by type: " + std::string(e.what()));
        return Result<long>::Failure("Database error: " + std::string(e.what()));
    }
}

Result<bool> ProfileStorage::checkSlugAvailability(const std::string& slug) {
    try {
        // Check if slug format is valid first
        if (!isValidSlug(slug)) {
            return Result<bool>::Failure("Invalid slug format");
        }

        // Query for existing profile with this slug, excluding soft-deleted profiles
        auto filter = document{}
            << "slug" << slug
            << "deletedAt" << open_document << "$exists" << false << close_document
            << finalize;
        auto result = profileCollection_.find_one(filter.view());

        // If no document found, slug is available
        if (!result) {
            return Result<bool>::Success(true, "Slug is available");
        }

        // Slug is taken
        return Result<bool>::Success(false, "Slug is already taken");

    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error checking slug availability: " + std::string(e.what()));
        return Result<bool>::Failure("Database error: " + std::string(e.what()));
    }
}

Result<std::optional<Profile>> ProfileStorage::findByHandle(const std::string& handle) {
    try {
        // Handle should start with @ and be followed by a valid slug
        if (handle.empty() || handle[0] != '@') {
            return Result<std::optional<Profile>>::Failure("Invalid handle format");
        }

        // Extract slug from handle (@username -> username)
        std::string slug = handle.substr(1);

        // Validate slug format
        if (!isValidSlug(slug)) {
            return Result<std::optional<Profile>>::Failure("Invalid handle slug format");
        }

        // Find by slug (handle maps directly to slug)
        return findBySlug(slug);

    } catch (const std::exception& e) {
        LOG_ERROR("Error in findByHandle: " + std::string(e.what()));
        return Result<std::optional<Profile>>::Failure("Error finding profile by handle: " + std::string(e.what()));
    }
}

Result<bool> ProfileStorage::updateSlug(const std::string& profileId, const std::string& newSlug) {
    try {
        // Validate new slug format
        if (!isValidSlug(newSlug)) {
            return Result<bool>::Failure("Invalid slug format. Slug must contain only Persian letters, English letters, numbers, and hyphens.");
        }

        // Check if slug is reserved
        if (search_engine::common::SlugGenerator::isReservedSlug(newSlug)) {
            return Result<bool>::Failure("Slug '" + newSlug + "' is reserved and cannot be used.");
        }

        // Check if new slug is available (excluding current profile)
        auto availabilityResult = checkSlugAvailability(newSlug);
        if (!availabilityResult.success) {
            return Result<bool>::Failure(availabilityResult.message);
        }

        if (!availabilityResult.value) {
            // Slug is taken by another profile
            return Result<bool>::Failure("Slug '" + newSlug + "' is already taken by another profile.");
        }

        // Get current profile to store old slug in history
        auto currentResult = findById(profileId);
        if (!currentResult.success) {
            return Result<bool>::Failure(currentResult.message);
        }

        auto currentProfile = currentResult.value;
        std::string oldSlug = currentProfile.slug;

        // Prepare update document
        auto filter = document{} << "_id" << bsoncxx::oid{profileId} << finalize;

        // Build update operations
        using bsoncxx::builder::basic::kvp;
        using bsoncxx::builder::basic::make_document;
        using bsoncxx::builder::basic::make_array;

        auto updateDoc = bsoncxx::builder::basic::document{};

        // Set new slug
        updateDoc.append(kvp("$set", make_document(
            kvp("slug", newSlug),
            kvp("slugChangedAt", bsoncxx::types::b_date{std::chrono::system_clock::now()})
        )));

        // Add old slug to previousSlugs array (if different)
        if (oldSlug != newSlug) {
            auto pushDoc = make_document(kvp("previousSlugs", oldSlug));
            updateDoc.append(kvp("$addToSet", pushDoc));
        }

        // Perform update
        auto result = profileCollection_.update_one(filter.view(), updateDoc.view());

        if (result && result->modified_count() > 0) {
            LOG_INFO("Updated slug for profile " + profileId + ": '" + oldSlug + "' -> '" + newSlug + "'");
            return Result<bool>::Success(true, "Slug updated successfully");
        } else {
            return Result<bool>::Failure("No profile found with given ID or no changes made");
        }

    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error updating slug: " + std::string(e.what()));
        return Result<bool>::Failure("Database error: " + std::string(e.what()));
    }
}

Result<std::optional<Profile>> ProfileStorage::findByPreviousSlug(const std::string& oldSlug) {
    try {
        // Query profiles where previousSlugs array contains the requested slug
        // Uses the "previous_slugs" index for efficient lookup
        auto filter = document{}
            << "previousSlugs" << oldSlug
            << "deletedAt" << open_document << "$exists" << false << close_document
            << finalize;
        auto result = profileCollection_.find_one(filter.view());

        if (result) {
            return Result<std::optional<Profile>>::Success(
                std::optional<Profile>(bsonToProfile(result->view())),
                "Profile found by previous slug");
        } else {
            return Result<std::optional<Profile>>::Success(
                std::nullopt, "No profile found with this previous slug");
        }
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error finding profile by previous slug: " + std::string(e.what()));
        return Result<std::optional<Profile>>::Failure("Database error: " + std::string(e.what()));
    }
}

Result<bool> ProfileStorage::testConnection() {
    try {
        // Simple ping test
        auto result = database_.run_command(document{} << "ping" << 1 << finalize);
        return Result<bool>::Success(true, "Connection test successful");
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("ProfileStorage connection test failed: " + std::string(e.what()));
        return Result<bool>::Failure("Connection test failed: " + std::string(e.what()));
    }
}

// ==================== PersonProfile BSON Serialization ====================

bsoncxx::document::value ProfileStorage::profileToBson(const PersonProfile& profile) const {
    using bsoncxx::builder::basic::kvp;
    using bsoncxx::builder::basic::make_document;
    using bsoncxx::builder::basic::make_array;
    
    auto builder = bsoncxx::builder::basic::document{};

    // Add ID if it exists
    if (profile.id) {
        builder.append(kvp("_id", bsoncxx::oid{profile.id.value()}));
    }

    // Required base fields
    builder.append(kvp("slug", profile.slug));
    builder.append(kvp("name", profile.name));
    builder.append(kvp("type", profileTypeToString(profile.type)));
    builder.append(kvp("isPublic", profile.isPublic));
    builder.append(kvp("createdAt", timePointToDate(profile.createdAt)));

    // Optional base fields
    if (profile.bio) {
        builder.append(kvp("bio", profile.bio.value()));
    }

    if (profile.previousSlugs && !profile.previousSlugs->empty()) {
        auto slugsArray = bsoncxx::builder::basic::array{};
        for (const auto& slug : profile.previousSlugs.value()) {
            slugsArray.append(slug);
        }
        builder.append(kvp("previousSlugs", slugsArray));
    }

    if (profile.slugChangedAt) {
        builder.append(kvp("slugChangedAt", timePointToDate(profile.slugChangedAt.value())));
    }

    if (profile.updatedAt) {
        builder.append(kvp("updatedAt", timePointToDate(profile.updatedAt.value())));
    }

    // Optional ownership fields
    if (profile.ownerToken) {
        builder.append(kvp("ownerToken", profile.ownerToken.value()));
    }
    if (profile.ownerId) {
        builder.append(kvp("ownerId", profile.ownerId.value()));
    }

    // Optional deletedAt timestamp (soft delete)
    if (profile.deletedAt) {
        builder.append(kvp("deletedAt", timePointToDate(profile.deletedAt.value())));
    }

    // PersonProfile-specific fields (optional)
    if (profile.title) {
        builder.append(kvp("title", profile.title.value()));
    }
    if (profile.company) {
        builder.append(kvp("company", profile.company.value()));
    }
    if (!profile.skills.empty()) {
        auto skillsArray = bsoncxx::builder::basic::array{};
        for (const auto& skill : profile.skills) {
            skillsArray.append(skill);
        }
        builder.append(kvp("skills", skillsArray));
    }
    if (profile.experienceLevel) {
        builder.append(kvp("experienceLevel", profile.experienceLevel.value()));
    }
    if (profile.education) {
        builder.append(kvp("education", profile.education.value()));
    }
    if (profile.school) {
        builder.append(kvp("school", profile.school.value()));
    }
    if (profile.linkedinUrl) {
        builder.append(kvp("linkedinUrl", profile.linkedinUrl.value()));
    }
    if (profile.githubUrl) {
        builder.append(kvp("githubUrl", profile.githubUrl.value()));
    }
    if (profile.portfolioUrl) {
        builder.append(kvp("portfolioUrl", profile.portfolioUrl.value()));
    }
    // Encrypt sensitive PII fields before storing
    if (profile.email) {
        std::string encryptedEmail = DataEncryption::encrypt(profile.email.value(), encryptionKey_);
        builder.append(kvp("email", encryptedEmail));
    }
    if (profile.phone) {
        std::string encryptedPhone = DataEncryption::encrypt(profile.phone.value(), encryptionKey_);
        builder.append(kvp("phone", encryptedPhone));
    }

    return builder.extract();
}

PersonProfile ProfileStorage::bsonToPersonProfile(const bsoncxx::document::view& doc) const {
    PersonProfile profile;

    // ID
    if (doc["_id"]) {
        profile.id = doc["_id"].get_oid().value.to_string();
    }

    // Required base fields
    profile.slug = std::string(doc["slug"].get_string().value);
    profile.name = std::string(doc["name"].get_string().value);
    profile.type = stringToProfileType(std::string(doc["type"].get_string().value));
    profile.isPublic = doc["isPublic"].get_bool().value;
    profile.createdAt = dateToTimePoint(doc["createdAt"].get_date());

    // Optional base fields
    if (doc["bio"]) {
        profile.bio = std::string(doc["bio"].get_string().value);
    }

    if (doc["previousSlugs"] && doc["previousSlugs"].type() == bsoncxx::type::k_array) {
        std::vector<std::string> slugs;
        auto slugsArray = doc["previousSlugs"].get_array();
        for (const auto& element : slugsArray.value) {
            if (element.type() == bsoncxx::type::k_string) {
                slugs.push_back(std::string(element.get_string().value));
            }
        }
        if (!slugs.empty()) {
            profile.previousSlugs = slugs;
        }
    }

    if (doc["slugChangedAt"]) {
        profile.slugChangedAt = dateToTimePoint(doc["slugChangedAt"].get_date());
    }

    if (doc["updatedAt"]) {
        profile.updatedAt = dateToTimePoint(doc["updatedAt"].get_date());
    }

    // Optional ownership fields
    if (doc["ownerToken"]) {
        profile.ownerToken = std::string(doc["ownerToken"].get_string().value);
    }
    if (doc["ownerId"]) {
        profile.ownerId = std::string(doc["ownerId"].get_string().value);
    }

    // Optional deletedAt timestamp (soft delete)
    if (doc["deletedAt"]) {
        profile.deletedAt = dateToTimePoint(doc["deletedAt"].get_date());
    }

    // PersonProfile-specific fields
    if (doc["title"]) {
        profile.title = std::string(doc["title"].get_string().value);
    }
    if (doc["company"]) {
        profile.company = std::string(doc["company"].get_string().value);
    }
    if (doc["skills"] && doc["skills"].type() == bsoncxx::type::k_array) {
        std::vector<std::string> skills;
        auto skillsArray = doc["skills"].get_array();
        for (const auto& element : skillsArray.value) {
            if (element.type() == bsoncxx::type::k_string) {
                skills.push_back(std::string(element.get_string().value));
            }
        }
        profile.skills = skills;
    }
    if (doc["experienceLevel"]) {
        profile.experienceLevel = std::string(doc["experienceLevel"].get_string().value);
    }
    if (doc["education"]) {
        profile.education = std::string(doc["education"].get_string().value);
    }
    if (doc["school"]) {
        profile.school = std::string(doc["school"].get_string().value);
    }
    if (doc["linkedinUrl"]) {
        profile.linkedinUrl = std::string(doc["linkedinUrl"].get_string().value);
    }
    if (doc["githubUrl"]) {
        profile.githubUrl = std::string(doc["githubUrl"].get_string().value);
    }
    if (doc["portfolioUrl"]) {
        profile.portfolioUrl = std::string(doc["portfolioUrl"].get_string().value);
    }
    // Decrypt sensitive PII fields when reading
    if (doc["email"]) {
        std::string encryptedEmail = std::string(doc["email"].get_string().value);
        profile.email = DataEncryption::decrypt(encryptedEmail, encryptionKey_);
    }
    if (doc["phone"]) {
        std::string encryptedPhone = std::string(doc["phone"].get_string().value);
        profile.phone = DataEncryption::decrypt(encryptedPhone, encryptionKey_);
    }

    return profile;
}

// ==================== BusinessProfile BSON Serialization ====================

bsoncxx::document::value ProfileStorage::profileToBson(const BusinessProfile& profile) const {
    using bsoncxx::builder::basic::kvp;
    using bsoncxx::builder::basic::make_document;
    using bsoncxx::builder::basic::make_array;
    
    auto builder = bsoncxx::builder::basic::document{};

    // Add ID if it exists
    if (profile.id) {
        builder.append(kvp("_id", bsoncxx::oid{profile.id.value()}));
    }

    // Required base fields
    builder.append(kvp("slug", profile.slug));
    builder.append(kvp("name", profile.name));
    builder.append(kvp("type", profileTypeToString(profile.type)));
    builder.append(kvp("isPublic", profile.isPublic));
    builder.append(kvp("createdAt", timePointToDate(profile.createdAt)));

    // Optional base fields
    if (profile.bio) {
        builder.append(kvp("bio", profile.bio.value()));
    }

    if (profile.previousSlugs && !profile.previousSlugs->empty()) {
        auto slugsArray = bsoncxx::builder::basic::array{};
        for (const auto& slug : profile.previousSlugs.value()) {
            slugsArray.append(slug);
        }
        builder.append(kvp("previousSlugs", slugsArray));
    }

    if (profile.slugChangedAt) {
        builder.append(kvp("slugChangedAt", timePointToDate(profile.slugChangedAt.value())));
    }

    if (profile.updatedAt) {
        builder.append(kvp("updatedAt", timePointToDate(profile.updatedAt.value())));
    }

    // Optional ownership fields
    if (profile.ownerToken) {
        builder.append(kvp("ownerToken", profile.ownerToken.value()));
    }
    if (profile.ownerId) {
        builder.append(kvp("ownerId", profile.ownerId.value()));
    }

    // Optional deletedAt timestamp (soft delete)
    if (profile.deletedAt) {
        builder.append(kvp("deletedAt", timePointToDate(profile.deletedAt.value())));
    }

    // BusinessProfile-specific fields (optional)
    if (profile.companyName) {
        builder.append(kvp("companyName", profile.companyName.value()));
    }
    if (profile.industry) {
        builder.append(kvp("industry", profile.industry.value()));
    }
    if (profile.companySize) {
        builder.append(kvp("companySize", profile.companySize.value()));
    }
    if (profile.foundedYear) {
        builder.append(kvp("foundedYear", profile.foundedYear.value()));
    }
    // Encrypt sensitive PII field (address)
    if (profile.address) {
        std::string encryptedAddress = DataEncryption::encrypt(profile.address.value(), encryptionKey_);
        builder.append(kvp("address", encryptedAddress));
    }
    if (profile.city) {
        builder.append(kvp("city", profile.city.value()));
    }
    if (profile.country) {
        builder.append(kvp("country", profile.country.value()));
    }
    if (profile.website) {
        builder.append(kvp("website", profile.website.value()));
    }
    if (profile.description) {
        builder.append(kvp("description", profile.description.value()));
    }
    if (!profile.services.empty()) {
        auto servicesArray = bsoncxx::builder::basic::array{};
        for (const auto& service : profile.services) {
            servicesArray.append(service);
        }
        builder.append(kvp("services", servicesArray));
    }
    // Encrypt sensitive PII fields
    if (profile.businessEmail) {
        std::string encryptedEmail = DataEncryption::encrypt(profile.businessEmail.value(), encryptionKey_);
        builder.append(kvp("businessEmail", encryptedEmail));
    }
    if (profile.businessPhone) {
        std::string encryptedPhone = DataEncryption::encrypt(profile.businessPhone.value(), encryptionKey_);
        builder.append(kvp("businessPhone", encryptedPhone));
    }

    return builder.extract();
}

BusinessProfile ProfileStorage::bsonToBusinessProfile(const bsoncxx::document::view& doc) const {
    BusinessProfile profile;

    // ID
    if (doc["_id"]) {
        profile.id = doc["_id"].get_oid().value.to_string();
    }

    // Required base fields
    profile.slug = std::string(doc["slug"].get_string().value);
    profile.name = std::string(doc["name"].get_string().value);
    profile.type = stringToProfileType(std::string(doc["type"].get_string().value));
    profile.isPublic = doc["isPublic"].get_bool().value;
    profile.createdAt = dateToTimePoint(doc["createdAt"].get_date());

    // Optional base fields
    if (doc["bio"]) {
        profile.bio = std::string(doc["bio"].get_string().value);
    }

    if (doc["previousSlugs"] && doc["previousSlugs"].type() == bsoncxx::type::k_array) {
        std::vector<std::string> slugs;
        auto slugsArray = doc["previousSlugs"].get_array();
        for (const auto& element : slugsArray.value) {
            if (element.type() == bsoncxx::type::k_string) {
                slugs.push_back(std::string(element.get_string().value));
            }
        }
        if (!slugs.empty()) {
            profile.previousSlugs = slugs;
        }
    }

    if (doc["slugChangedAt"]) {
        profile.slugChangedAt = dateToTimePoint(doc["slugChangedAt"].get_date());
    }

    if (doc["updatedAt"]) {
        profile.updatedAt = dateToTimePoint(doc["updatedAt"].get_date());
    }

    // Optional ownership fields
    if (doc["ownerToken"]) {
        profile.ownerToken = std::string(doc["ownerToken"].get_string().value);
    }
    if (doc["ownerId"]) {
        profile.ownerId = std::string(doc["ownerId"].get_string().value);
    }

    // Optional deletedAt timestamp (soft delete)
    if (doc["deletedAt"]) {
        profile.deletedAt = dateToTimePoint(doc["deletedAt"].get_date());
    }

    // BusinessProfile-specific fields
    if (doc["companyName"]) {
        profile.companyName = std::string(doc["companyName"].get_string().value);
    }
    if (doc["industry"]) {
        profile.industry = std::string(doc["industry"].get_string().value);
    }
    if (doc["companySize"]) {
        profile.companySize = std::string(doc["companySize"].get_string().value);
    }
    if (doc["foundedYear"]) {
        profile.foundedYear = doc["foundedYear"].get_int32().value;
    }
    // Decrypt sensitive PII field (address)
    if (doc["address"]) {
        std::string encryptedAddress = std::string(doc["address"].get_string().value);
        profile.address = DataEncryption::decrypt(encryptedAddress, encryptionKey_);
    }
    if (doc["city"]) {
        profile.city = std::string(doc["city"].get_string().value);
    }
    if (doc["country"]) {
        profile.country = std::string(doc["country"].get_string().value);
    }
    if (doc["website"]) {
        profile.website = std::string(doc["website"].get_string().value);
    }
    if (doc["description"]) {
        profile.description = std::string(doc["description"].get_string().value);
    }
    if (doc["services"] && doc["services"].type() == bsoncxx::type::k_array) {
        std::vector<std::string> services;
        auto servicesArray = doc["services"].get_array();
        for (const auto& element : servicesArray.value) {
            if (element.type() == bsoncxx::type::k_string) {
                services.push_back(std::string(element.get_string().value));
            }
        }
        profile.services = services;
    }
    // Decrypt sensitive PII fields
    if (doc["businessEmail"]) {
        std::string encryptedEmail = std::string(doc["businessEmail"].get_string().value);
        profile.businessEmail = DataEncryption::decrypt(encryptedEmail, encryptionKey_);
    }
    if (doc["businessPhone"]) {
        std::string encryptedPhone = std::string(doc["businessPhone"].get_string().value);
        profile.businessPhone = DataEncryption::decrypt(encryptedPhone, encryptionKey_);
    }

    return profile;
}

// ==================== PersonProfile CRUD Operations ====================

Result<std::string> ProfileStorage::store(const PersonProfile& profile) {
    try {
        // Validate slug format
        if (!isValidSlug(profile.slug)) {
            return Result<std::string>::Failure("Invalid slug format. Slug must contain only Persian letters, English letters, numbers, and hyphens.");
        }

        // Validate profile
        if (!profile.isValid()) {
            return Result<std::string>::Failure("Invalid PersonProfile: validation failed");
        }

        // Check for slug uniqueness
        auto existingResult = findBySlug(profile.slug);
        if (existingResult.success && existingResult.value.has_value()) {
            return Result<std::string>::Failure("Slug '" + profile.slug + "' is already taken.");
        }

        auto doc = profileToBson(profile);
        auto result = profileCollection_.insert_one(doc.view());

        if (result) {
            std::string id = result->inserted_id().get_oid().value.to_string();
            LOG_INFO("Stored PersonProfile with ID: " + id + ", slug: " + profile.slug);
            return Result<std::string>::Success(id, "PersonProfile stored successfully");
        } else {
            LOG_ERROR("Failed to store PersonProfile");
            return Result<std::string>::Failure("Failed to store PersonProfile");
        }
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error storing PersonProfile: " + std::string(e.what()));
        return Result<std::string>::Failure("Database error: " + std::string(e.what()));
    }
}

Result<std::optional<PersonProfile>> ProfileStorage::findPersonById(const std::string& id) {
    try {
        // Filter: match ID and exclude soft-deleted profiles
        auto filter = document{} 
            << "_id" << bsoncxx::oid{id}
            << "deletedAt" << open_document << "$exists" << false << close_document
            << finalize;
        auto result = profileCollection_.find_one(filter.view());

        if (result) {
            auto doc = result->view();
            // Check if this is a person profile
            if (doc["type"] && std::string(doc["type"].get_string().value) == "PERSON") {
                return Result<std::optional<PersonProfile>>::Success(
                    std::optional<PersonProfile>(bsonToPersonProfile(doc)), 
                    "PersonProfile found"
                );
            } else {
                return Result<std::optional<PersonProfile>>::Success(
                    std::nullopt, 
                    "Profile found but not a PersonProfile"
                );
            }
        } else {
            return Result<std::optional<PersonProfile>>::Success(
                std::nullopt, 
                "No profile found with this ID"
            );
        }
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error finding PersonProfile by ID: " + std::string(e.what()));
        return Result<std::optional<PersonProfile>>::Failure("Database error: " + std::string(e.what()));
    }
}

Result<std::optional<PersonProfile>> ProfileStorage::findPersonBySlug(const std::string& slug) {
    try {
        // Filter: match slug and exclude soft-deleted profiles
        auto filter = document{} 
            << "slug" << slug
            << "deletedAt" << open_document << "$exists" << false << close_document
            << finalize;
        auto result = profileCollection_.find_one(filter.view());

        if (result) {
            auto doc = result->view();
            // Check if this is a person profile
            if (doc["type"] && std::string(doc["type"].get_string().value) == "PERSON") {
                return Result<std::optional<PersonProfile>>::Success(
                    std::optional<PersonProfile>(bsonToPersonProfile(doc)), 
                    "PersonProfile found"
                );
            } else {
                return Result<std::optional<PersonProfile>>::Success(
                    std::nullopt, 
                    "Profile found but not a PersonProfile"
                );
            }
        } else {
            return Result<std::optional<PersonProfile>>::Success(
                std::nullopt, 
                "No profile found with this slug"
            );
        }
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error finding PersonProfile by slug: " + std::string(e.what()));
        return Result<std::optional<PersonProfile>>::Failure("Database error: " + std::string(e.what()));
    }
}

Result<bool> ProfileStorage::update(const PersonProfile& profile) {
    try {
        if (!profile.id) {
            return Result<bool>::Failure("Profile ID is required for update");
        }

        // Validate profile
        if (!profile.isValid()) {
            return Result<bool>::Failure("Invalid PersonProfile: validation failed");
        }

        // Validate slug format
        if (!isValidSlug(profile.slug)) {
            return Result<bool>::Failure("Invalid slug format. Slug must contain only Persian letters, English letters, numbers, and hyphens.");
        }

        auto filter = document{} << "_id" << bsoncxx::oid{profile.id.value()} << finalize;

        // Check slug uniqueness (excluding current profile)
        auto existingResult = findBySlug(profile.slug);
        if (existingResult.success && existingResult.value.has_value() &&
            existingResult.value.value().id != profile.id) {
            return Result<bool>::Failure("Slug '" + profile.slug + "' is already taken by another profile.");
        }

        // Create mutable copy and set updatedAt timestamp
        PersonProfile updatedProfile = profile;
        updatedProfile.updatedAt = std::chrono::system_clock::now();

        // Build full update document with all fields
        auto updateDoc = profileToBson(updatedProfile);
        
        // Use $set to update all fields
        using bsoncxx::builder::basic::kvp;
        auto setDoc = bsoncxx::builder::basic::document{};
        setDoc.append(kvp("$set", updateDoc));

        auto result = profileCollection_.update_one(filter.view(), setDoc.view());

        if (result && result->modified_count() > 0) {
            LOG_INFO("Updated PersonProfile with ID: " + profile.id.value());
            return Result<bool>::Success(true, "PersonProfile updated successfully");
        } else {
            return Result<bool>::Failure("No profile found with given ID or no changes made");
        }
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error updating PersonProfile: " + std::string(e.what()));
        return Result<bool>::Failure("Database error: " + std::string(e.what()));
    }
}

// ==================== BusinessProfile CRUD Operations ====================

Result<std::string> ProfileStorage::store(const BusinessProfile& profile) {
    try {
        // Validate slug format
        if (!isValidSlug(profile.slug)) {
            return Result<std::string>::Failure("Invalid slug format. Slug must contain only Persian letters, English letters, numbers, and hyphens.");
        }

        // Validate profile
        if (!profile.isValid()) {
            return Result<std::string>::Failure("Invalid BusinessProfile: validation failed");
        }

        // Check for slug uniqueness
        auto existingResult = findBySlug(profile.slug);
        if (existingResult.success && existingResult.value.has_value()) {
            return Result<std::string>::Failure("Slug '" + profile.slug + "' is already taken.");
        }

        auto doc = profileToBson(profile);
        auto result = profileCollection_.insert_one(doc.view());

        if (result) {
            std::string id = result->inserted_id().get_oid().value.to_string();
            LOG_INFO("Stored BusinessProfile with ID: " + id + ", slug: " + profile.slug);
            return Result<std::string>::Success(id, "BusinessProfile stored successfully");
        } else {
            LOG_ERROR("Failed to store BusinessProfile");
            return Result<std::string>::Failure("Failed to store BusinessProfile");
        }
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error storing BusinessProfile: " + std::string(e.what()));
        return Result<std::string>::Failure("Database error: " + std::string(e.what()));
    }
}

Result<std::optional<BusinessProfile>> ProfileStorage::findBusinessById(const std::string& id) {
    try {
        // Filter: match ID and exclude soft-deleted profiles
        auto filter = document{} 
            << "_id" << bsoncxx::oid{id}
            << "deletedAt" << open_document << "$exists" << false << close_document
            << finalize;
        auto result = profileCollection_.find_one(filter.view());

        if (result) {
            auto doc = result->view();
            // Check if this is a business profile
            if (doc["type"] && std::string(doc["type"].get_string().value) == "BUSINESS") {
                return Result<std::optional<BusinessProfile>>::Success(
                    std::optional<BusinessProfile>(bsonToBusinessProfile(doc)), 
                    "BusinessProfile found"
                );
            } else {
                return Result<std::optional<BusinessProfile>>::Success(
                    std::nullopt, 
                    "Profile found but not a BusinessProfile"
                );
            }
        } else {
            return Result<std::optional<BusinessProfile>>::Success(
                std::nullopt, 
                "No profile found with this ID"
            );
        }
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error finding BusinessProfile by ID: " + std::string(e.what()));
        return Result<std::optional<BusinessProfile>>::Failure("Database error: " + std::string(e.what()));
    }
}

Result<std::optional<BusinessProfile>> ProfileStorage::findBusinessBySlug(const std::string& slug) {
    try {
        // Filter: match slug and exclude soft-deleted profiles
        auto filter = document{} 
            << "slug" << slug
            << "deletedAt" << open_document << "$exists" << false << close_document
            << finalize;
        auto result = profileCollection_.find_one(filter.view());

        if (result) {
            auto doc = result->view();
            // Check if this is a business profile
            if (doc["type"] && std::string(doc["type"].get_string().value) == "BUSINESS") {
                return Result<std::optional<BusinessProfile>>::Success(
                    std::optional<BusinessProfile>(bsonToBusinessProfile(doc)), 
                    "BusinessProfile found"
                );
            } else {
                return Result<std::optional<BusinessProfile>>::Success(
                    std::nullopt, 
                    "Profile found but not a BusinessProfile"
                );
            }
        } else {
            return Result<std::optional<BusinessProfile>>::Success(
                std::nullopt, 
                "No profile found with this slug"
            );
        }
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error finding BusinessProfile by slug: " + std::string(e.what()));
        return Result<std::optional<BusinessProfile>>::Failure("Database error: " + std::string(e.what()));
    }
}

Result<bool> ProfileStorage::update(const BusinessProfile& profile) {
    try {
        if (!profile.id) {
            return Result<bool>::Failure("Profile ID is required for update");
        }

        // Validate profile
        if (!profile.isValid()) {
            return Result<bool>::Failure("Invalid BusinessProfile: validation failed");
        }

        // Validate slug format
        if (!isValidSlug(profile.slug)) {
            return Result<bool>::Failure("Invalid slug format. Slug must contain only Persian letters, English letters, numbers, and hyphens.");
        }

        auto filter = document{} << "_id" << bsoncxx::oid{profile.id.value()} << finalize;

        // Check slug uniqueness (excluding current profile)
        auto existingResult = findBySlug(profile.slug);
        if (existingResult.success && existingResult.value.has_value() &&
            existingResult.value.value().id != profile.id) {
            return Result<bool>::Failure("Slug '" + profile.slug + "' is already taken by another profile.");
        }

        // Create mutable copy and set updatedAt timestamp
        BusinessProfile updatedProfile = profile;
        updatedProfile.updatedAt = std::chrono::system_clock::now();

        // Build full update document with all fields
        auto updateDoc = profileToBson(updatedProfile);
        
        // Use $set to update all fields
        using bsoncxx::builder::basic::kvp;
        auto setDoc = bsoncxx::builder::basic::document{};
        setDoc.append(kvp("$set", updateDoc));

        auto result = profileCollection_.update_one(filter.view(), setDoc.view());

        if (result && result->modified_count() > 0) {
            LOG_INFO("Updated BusinessProfile with ID: " + profile.id.value());
            return Result<bool>::Success(true, "BusinessProfile updated successfully");
        } else {
            return Result<bool>::Failure("No profile found with given ID or no changes made");
        }
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error updating BusinessProfile: " + std::string(e.what()));
        return Result<bool>::Failure("Database error: " + std::string(e.what()));
    }
}

} // namespace storage
} // namespace search_engine
