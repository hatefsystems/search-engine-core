#include "../../include/search_engine/storage/LinkBlockStorage.h"
#include "../../include/mongodb.h"
#include "../../include/Logger.h"
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/oid.hpp>
#include <mongocxx/exception/bulk_write_exception.hpp>
#include <cstdlib>

namespace search_engine {
namespace storage {

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;

// Helper to convert time_point to BSON date
bsoncxx::types::b_date LinkBlockStorage::timePointToDate(
    const std::chrono::system_clock::time_point& tp) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch());
    return bsoncxx::types::b_date{ms};
}

// Helper to convert BSON date to time_point
std::chrono::system_clock::time_point LinkBlockStorage::dateToTimePoint(
    const bsoncxx::types::b_date& date) {
    return std::chrono::system_clock::time_point{std::chrono::milliseconds{date.to_int64()}};
}

LinkBlockStorage::LinkBlockStorage(
    const std::string& connectionString,
    const std::string& databaseName)
{
    LOG_DEBUG("LinkBlockStorage constructor called");
    
    try {
        // Initialize MongoDB instance singleton
        MongoDBInstance::getInstance();
        
        // Get MongoDB connection string from environment or use default
        const char* mongo_uri = std::getenv("MONGODB_URI");
        std::string uri = mongo_uri ? mongo_uri : connectionString;
        
        LOG_DEBUG("Connecting to MongoDB: " + uri);
        mongocxx::uri mongo_uri_obj{uri};
        client_ = std::make_unique<mongocxx::client>(mongo_uri_obj);
        
        // Get database and collection
        database_ = (*client_)[databaseName];
        linkBlockCollection_ = database_["link_blocks"];
        
        LOG_INFO("LinkBlockStorage initialized successfully");
        
        // Ensure indexes exist
        ensureIndexes();
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("Failed to initialize LinkBlockStorage: " + std::string(e.what()));
        throw std::runtime_error("Failed to initialize LinkBlockStorage: " + std::string(e.what()));
    }
}

void LinkBlockStorage::ensureIndexes() {
    LOG_DEBUG("Creating indexes for link_blocks collection");
    
    try {
        // Index 1: { profileId: 1, sortOrder: 1 } - list links ordered
        auto profileSortIndex = bsoncxx::builder::basic::document{};
        profileSortIndex.append(kvp("profileId", 1));
        profileSortIndex.append(kvp("sortOrder", 1));
        
        mongocxx::options::index profileSortOptions{};
        profileSortOptions.name("profile_sort_order");
        linkBlockCollection_.create_index(profileSortIndex.view(), profileSortOptions);
        
        // Index 2: { profileId: 1, isActive: 1 } - filter active links
        auto profileActiveIndex = bsoncxx::builder::basic::document{};
        profileActiveIndex.append(kvp("profileId", 1));
        profileActiveIndex.append(kvp("isActive", 1));
        
        mongocxx::options::index profileActiveOptions{};
        profileActiveOptions.name("profile_active_links");
        linkBlockCollection_.create_index(profileActiveIndex.view(), profileActiveOptions);
        
        LOG_INFO("LinkBlockStorage indexes created successfully");
        
    } catch (const mongocxx::exception& e) {
        LOG_WARNING("Failed to create indexes (may already exist): " + std::string(e.what()));
    }
}

bsoncxx::document::value LinkBlockStorage::linkBlockToBson(const LinkBlock& link) const {
    auto builder = bsoncxx::builder::basic::document{};
    
    // Optional _id (if updating)
    if (link.id.has_value() && !link.id.value().empty()) {
        builder.append(kvp("_id", bsoncxx::oid{link.id.value()}));
    }
    
    // Required fields
    builder.append(kvp("profileId", link.profileId));
    builder.append(kvp("url", link.url));
    builder.append(kvp("title", link.title));
    
    // Optional fields
    if (link.description.has_value()) {
        builder.append(kvp("description", link.description.value()));
    }
    if (link.iconUrl.has_value()) {
        builder.append(kvp("iconUrl", link.iconUrl.value()));
    }
    
    // Behavior and privacy
    builder.append(kvp("isActive", link.isActive));
    builder.append(kvp("privacy", linkPrivacyToString(link.privacy)));
    
    // Tags array
    auto tagsArray = bsoncxx::builder::basic::array{};
    for (const auto& tag : link.tags) {
        tagsArray.append(tag);
    }
    builder.append(kvp("tags", tagsArray));
    
    // Organization
    builder.append(kvp("sortOrder", link.sortOrder));
    
    // Timestamps
    builder.append(kvp("createdAt", timePointToDate(link.createdAt)));
    if (link.updatedAt.has_value()) {
        builder.append(kvp("updatedAt", timePointToDate(link.updatedAt.value())));
    }
    
    return builder.extract();
}

LinkBlock LinkBlockStorage::bsonToLinkBlock(const bsoncxx::document::view& doc) const {
    LinkBlock link;
    
    // _id
    if (doc["_id"]) {
        link.id = doc["_id"].get_oid().value.to_string();
    }
    
    // Required fields
    link.profileId = std::string(doc["profileId"].get_string().value);
    link.url = std::string(doc["url"].get_string().value);
    link.title = std::string(doc["title"].get_string().value);
    
    // Optional fields
    if (doc["description"]) {
        link.description = std::string(doc["description"].get_string().value);
    }
    if (doc["iconUrl"]) {
        link.iconUrl = std::string(doc["iconUrl"].get_string().value);
    }
    
    // Behavior and privacy
    if (doc["isActive"]) {
        link.isActive = doc["isActive"].get_bool().value;
    }
    if (doc["privacy"]) {
        link.privacy = stringToLinkPrivacy(std::string(doc["privacy"].get_string().value));
    }
    
    // Tags array
    if (doc["tags"]) {
        auto tagsArray = doc["tags"].get_array().value;
        for (auto&& tagElem : tagsArray) {
            link.tags.push_back(std::string(tagElem.get_string().value));
        }
    }
    
    // Organization
    if (doc["sortOrder"]) {
        link.sortOrder = doc["sortOrder"].get_int32().value;
    }
    
    // Timestamps
    if (doc["createdAt"]) {
        link.createdAt = dateToTimePoint(doc["createdAt"].get_date());
    }
    if (doc["updatedAt"]) {
        link.updatedAt = dateToTimePoint(doc["updatedAt"].get_date());
    }
    
    return link;
}

Result<std::string> LinkBlockStorage::store(const LinkBlock& link) {
    try {
        // Validate link
        if (!link.isValid()) {
            return Result<std::string>::Failure("Invalid link block data");
        }
        
        // Create BSON document (without _id, MongoDB will auto-generate)
        LinkBlock linkCopy = link;
        linkCopy.id = std::nullopt; // Ensure no ID for new document
        linkCopy.createdAt = std::chrono::system_clock::now();
        
        auto doc = linkBlockToBson(linkCopy);
        
        // Insert into collection
        auto result = linkBlockCollection_.insert_one(doc.view());
        
        if (result) {
            std::string insertedId = result->inserted_id().get_oid().value.to_string();
            LOG_INFO("Link block stored successfully: " + insertedId);
            return Result<std::string>::Success(insertedId, "Link block created");
        } else {
            LOG_ERROR("Failed to store link block: insert_one returned no result");
            return Result<std::string>::Failure("Failed to store link block");
        }
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error storing link block: " + std::string(e.what()));
        return Result<std::string>::Failure("Database error: " + std::string(e.what()));
    }
}

Result<std::optional<LinkBlock>> LinkBlockStorage::findById(const std::string& id) {
    try {
        // Validate ObjectId format
        if (id.empty() || id.length() != 24) {
            return Result<std::optional<LinkBlock>>::Success(
                std::nullopt, "Invalid ID format");
        }
        
        // Build filter
        auto filter = bsoncxx::builder::basic::document{};
        filter.append(kvp("_id", bsoncxx::oid{id}));
        
        // Find document
        auto result = linkBlockCollection_.find_one(filter.view());
        
        if (result) {
            LinkBlock link = bsonToLinkBlock(result->view());
            return Result<std::optional<LinkBlock>>::Success(
                link, "Link block found");
        } else {
            return Result<std::optional<LinkBlock>>::Success(
                std::nullopt, "Link block not found");
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error finding link block: " + std::string(e.what()));
        return Result<std::optional<LinkBlock>>::Failure(
            "Database error: " + std::string(e.what()));
    }
}

Result<std::vector<LinkBlock>> LinkBlockStorage::findByProfile(
    const std::string& profileId,
    int limit,
    int skip)
{
    try {
        // Build filter
        auto filter = bsoncxx::builder::basic::document{};
        filter.append(kvp("profileId", profileId));
        
        // Build options with sort (by sortOrder ascending)
        mongocxx::options::find opts{};
        auto sortDoc = bsoncxx::builder::basic::document{};
        sortDoc.append(kvp("sortOrder", 1));
        opts.sort(sortDoc.view());
        opts.limit(limit);
        opts.skip(skip);
        
        // Find documents
        auto cursor = linkBlockCollection_.find(filter.view(), opts);
        
        std::vector<LinkBlock> links;
        for (auto&& doc : cursor) {
            links.push_back(bsonToLinkBlock(doc));
        }
        
        LOG_DEBUG("Found " + std::to_string(links.size()) + " links for profile: " + profileId);
        return Result<std::vector<LinkBlock>>::Success(
            links, "Links retrieved successfully");
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error finding links by profile: " + std::string(e.what()));
        return Result<std::vector<LinkBlock>>::Failure(
            "Database error: " + std::string(e.what()));
    }
}

Result<bool> LinkBlockStorage::update(const LinkBlock& link) {
    try {
        // Validate link
        if (!link.isValid()) {
            return Result<bool>::Failure("Invalid link block data");
        }
        
        // Must have ID to update
        if (!link.id.has_value() || link.id.value().empty()) {
            return Result<bool>::Failure("Link ID is required for update");
        }
        
        // Build filter
        auto filter = bsoncxx::builder::basic::document{};
        filter.append(kvp("_id", bsoncxx::oid{link.id.value()}));
        
        // Build update document using basic builder
        LinkBlock linkCopy = link;
        linkCopy.updatedAt = std::chrono::system_clock::now();
        
        // Create $set document
        auto setFields = bsoncxx::builder::basic::document{};
        setFields.append(kvp("profileId", linkCopy.profileId));
        setFields.append(kvp("url", linkCopy.url));
        setFields.append(kvp("title", linkCopy.title));
        
        if (linkCopy.description.has_value()) {
            setFields.append(kvp("description", linkCopy.description.value()));
        }
        if (linkCopy.iconUrl.has_value()) {
            setFields.append(kvp("iconUrl", linkCopy.iconUrl.value()));
        }
        
        setFields.append(kvp("isActive", linkCopy.isActive));
        setFields.append(kvp("privacy", linkPrivacyToString(linkCopy.privacy)));
        
        auto tagsArray = bsoncxx::builder::basic::array{};
        for (const auto& tag : linkCopy.tags) {
            tagsArray.append(tag);
        }
        setFields.append(kvp("tags", tagsArray));
        
        setFields.append(kvp("sortOrder", linkCopy.sortOrder));
        setFields.append(kvp("updatedAt", timePointToDate(linkCopy.updatedAt.value())));
        
        // Build final update document
        auto updateDoc = bsoncxx::builder::basic::document{};
        updateDoc.append(kvp("$set", setFields.extract()));
        
        // Update document
        auto result = linkBlockCollection_.update_one(
            filter.view(), updateDoc.extract());
        
        if (result && result->modified_count() > 0) {
            LOG_INFO("Link block updated successfully: " + link.id.value());
            return Result<bool>::Success(true, "Link block updated");
        } else if (result && result->matched_count() > 0) {
            LOG_DEBUG("Link block matched but not modified (no changes): " + link.id.value());
            return Result<bool>::Success(true, "Link block unchanged");
        } else {
            LOG_WARNING("Link block not found for update: " + link.id.value());
            return Result<bool>::Failure("Link block not found");
        }
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error updating link block: " + std::string(e.what()));
        return Result<bool>::Failure("Database error: " + std::string(e.what()));
    }
}

Result<bool> LinkBlockStorage::deleteLink(const std::string& id) {
    try {
        // Validate ObjectId format
        if (id.empty() || id.length() != 24) {
            return Result<bool>::Failure("Invalid ID format");
        }
        
        // Build filter
        auto filter = bsoncxx::builder::basic::document{};
        filter.append(kvp("_id", bsoncxx::oid{id}));
        
        // Delete document
        auto result = linkBlockCollection_.delete_one(filter.view());
        
        if (result && result->deleted_count() > 0) {
            LOG_INFO("Link block deleted successfully: " + id);
            return Result<bool>::Success(true, "Link block deleted");
        } else {
            LOG_WARNING("Link block not found for deletion: " + id);
            return Result<bool>::Failure("Link block not found");
        }
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error deleting link block: " + std::string(e.what()));
        return Result<bool>::Failure("Database error: " + std::string(e.what()));
    }
}

Result<int64_t> LinkBlockStorage::countByProfile(const std::string& profileId) {
    try {
        // Build filter
        auto filter = bsoncxx::builder::basic::document{};
        filter.append(kvp("profileId", profileId));
        
        // Count documents
        int64_t count = linkBlockCollection_.count_documents(filter.view());
        
        LOG_DEBUG("Count for profile " + profileId + ": " + std::to_string(count));
        return Result<int64_t>::Success(count, "Count retrieved successfully");
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error counting links: " + std::string(e.what()));
        return Result<int64_t>::Failure("Database error: " + std::string(e.what()));
    }
}

} // namespace storage
} // namespace search_engine
