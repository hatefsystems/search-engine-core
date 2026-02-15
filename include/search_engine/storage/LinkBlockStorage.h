#pragma once

#include "LinkBlock.h"
#include "../../infrastructure.h"
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <bsoncxx/document/value.hpp>
#include <bsoncxx/document/view.hpp>
#include <memory>
#include <vector>
#include <optional>

namespace search_engine {
namespace storage {

/**
 * @brief Storage layer for LinkBlock entities
 * 
 * Manages CRUD operations for link blocks:
 * - Create/store new links
 * - Read by ID or profile
 * - Update existing links
 * - Delete links
 * 
 * Collection: link_blocks
 * Indexes: 
 *   - { profileId: 1, sortOrder: 1 } - list links for profile ordered
 *   - { _id: 1 } - find by ID (implicit)
 *   - { profileId: 1, isActive: 1 } - filter active links
 */
class LinkBlockStorage {
private:
    std::unique_ptr<mongocxx::client> client_;
    mongocxx::database database_;
    mongocxx::collection linkBlockCollection_;
    
    // Conversion methods between LinkBlock and BSON
    bsoncxx::document::value linkBlockToBson(const LinkBlock& link) const;
    LinkBlock bsonToLinkBlock(const bsoncxx::document::view& doc) const;
    
    // Ensure indexes are created
    void ensureIndexes();
    
    // Helper to convert system_clock::time_point to bsoncxx::types::b_date
    static bsoncxx::types::b_date timePointToDate(const std::chrono::system_clock::time_point& tp);
    
    // Helper to convert bsoncxx::types::b_date to system_clock::time_point
    static std::chrono::system_clock::time_point dateToTimePoint(const bsoncxx::types::b_date& date);
    
public:
    /**
     * @brief Constructor with MongoDB connection
     * @param connectionString MongoDB URI (default from MONGODB_URI env)
     * @param databaseName Database name (default: search-engine)
     */
    explicit LinkBlockStorage(
        const std::string& connectionString = "mongodb://localhost:27017",
        const std::string& databaseName = "search-engine"
    );
    
    ~LinkBlockStorage() = default;
    
    // Move constructor and assignment (delete copy operations for RAII)
    LinkBlockStorage(LinkBlockStorage&& other) noexcept = default;
    LinkBlockStorage& operator=(LinkBlockStorage&& other) noexcept = default;
    LinkBlockStorage(const LinkBlockStorage&) = delete;
    LinkBlockStorage& operator=(const LinkBlockStorage&) = delete;
    
    /**
     * @brief Store a new link block
     * @param link LinkBlock to store (id will be auto-generated)
     * @return Result with generated link ID on success
     */
    Result<std::string> store(const LinkBlock& link);
    
    /**
     * @brief Find link block by ID
     * @param id Link block ID
     * @return Result with optional LinkBlock
     */
    Result<std::optional<LinkBlock>> findById(const std::string& id);
    
    /**
     * @brief Find all link blocks for a profile
     * @param profileId Profile ID
     * @param limit Max number of links to return (default: 100)
     * @param skip Number of links to skip (default: 0)
     * @return Result with vector of LinkBlocks ordered by sortOrder
     */
    Result<std::vector<LinkBlock>> findByProfile(
        const std::string& profileId,
        int limit = 100,
        int skip = 0
    );
    
    /**
     * @brief Update an existing link block
     * @param link LinkBlock with id set and updated fields
     * @return Result with success status
     */
    Result<bool> update(const LinkBlock& link);
    
    /**
     * @brief Delete a link block
     * @param id Link block ID
     * @return Result with success status
     */
    Result<bool> deleteLink(const std::string& id);
    
    /**
     * @brief Count link blocks for a profile
     * @param profileId Profile ID
     * @return Result with count
     */
    Result<int64_t> countByProfile(const std::string& profileId);
};

} // namespace storage
} // namespace search_engine
