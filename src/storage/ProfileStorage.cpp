#include "../../include/search_engine/storage/ProfileStorage.h"
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

    // Regex pattern for Persian letters (U+0600-U+06FF) + English letters + numbers + hyphens
    // Pattern: ^[\u0600-\u06FFa-zA-Z0-9-]+$
    static const std::regex slugRegex("^[\u0600-\u06FFa-zA-Z0-9-]+$", std::regex_constants::extended);

    return std::regex_match(slug, slugRegex);
}

bsoncxx::document::value ProfileStorage::profileToBson(const Profile& profile) const {
    auto builder = document{};

    // Add ID if it exists
    if (profile.id) {
        builder << "_id" << bsoncxx::oid{profile.id.value()};
    }

    // Required fields (support UTF-8)
    builder << "slug" << profile.slug
            << "name" << profile.name
            << "type" << profileTypeToString(profile.type)
            << "isPublic" << profile.isPublic
            << "createdAt" << timePointToDate(profile.createdAt);

    // Optional bio field
    if (profile.bio) {
        builder << "bio" << profile.bio.value();
    }

    // Optional previousSlugs array for SEO redirects
    if (profile.previousSlugs && !profile.previousSlugs->empty()) {
        using bsoncxx::builder::stream::array;
        auto slugsArray = array{};
        for (const auto& slug : profile.previousSlugs.value()) {
            slugsArray << slug;
        }
        builder << "previousSlugs" << slugsArray;
    }

    // Optional slugChangedAt timestamp
    if (profile.slugChangedAt) {
        builder << "slugChangedAt" << timePointToDate(profile.slugChangedAt.value());
    }

    return builder << finalize;
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

    return profile;
}

void ProfileStorage::ensureIndexes() {
    try {
        // Create unique index on slug (MongoDB handles UTF-8 natively)
        mongocxx::options::index uniqueIndexOptions{};
        uniqueIndexOptions.unique(true);
        profileCollection_.create_index(document{} << "slug" << 1 << finalize, uniqueIndexOptions);

        // Create index on type
        profileCollection_.create_index(document{} << "type" << 1 << finalize);

        // Create index on createdAt
        profileCollection_.create_index(document{} << "createdAt" << -1 << finalize);

        // Create compound index on type and createdAt
        profileCollection_.create_index(document{} << "type" << 1 << "createdAt" << -1 << finalize);

        LOG_INFO("ProfileStorage indexes created successfully");
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

        // Check for slug uniqueness
        auto existingResult = findBySlug(profile.slug);
        if (existingResult.success && existingResult.value.has_value()) {
            return Result<std::string>::Failure("Slug '" + profile.slug + "' is already taken.");
        }

        auto doc = profileToBson(profile);
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
        LOG_ERROR("MongoDB error storing profile: " + std::string(e.what()));
        return Result<std::string>::Failure("Database error: " + std::string(e.what()));
    }
}

Result<Profile> ProfileStorage::findById(const std::string& id) {
    try {
        auto filter = document{} << "_id" << bsoncxx::oid{id} << finalize;
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
        auto filter = document{} << "slug" << slug << finalize;
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

        auto filter = document{} << "_id" << bsoncxx::oid{profile.id.value()} << finalize;

        // For update, we need to check slug uniqueness (excluding current profile)
        auto existingResult = findBySlug(profile.slug);
        if (existingResult.success && existingResult.value.has_value() &&
            existingResult.value.value().id != profile.id) {
            return Result<bool>::Failure("Slug '" + profile.slug + "' is already taken by another profile.");
        }

        auto updateDoc = profileToBson(profile);
        auto update = document{} << "$set" << updateDoc << finalize;

        auto result = profileCollection_.update_one(filter.view(), update.view());

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
        auto result = profileCollection_.delete_one(filter.view());

        if (result && result->deleted_count() > 0) {
            LOG_INFO("Deleted profile with ID: " + id);
            return Result<bool>::Success(true, "Profile deleted successfully");
        } else {
            return Result<bool>::Failure("No profile found with given ID");
        }
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error deleting profile: " + std::string(e.what()));
        return Result<bool>::Failure("Database error: " + std::string(e.what()));
    }
}

Result<std::vector<Profile>> ProfileStorage::findAll(int limit, int skip) {
    try {
        mongocxx::options::find findOptions{};
        findOptions.limit(limit).skip(skip);

        auto cursor = profileCollection_.find({}, findOptions);

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
        auto filter = document{} << "type" << profileTypeToString(type) << finalize;

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

        // Query for existing profile with this slug
        auto filter = document{} << "slug" << slug << finalize;
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

} // namespace storage
} // namespace search_engine
