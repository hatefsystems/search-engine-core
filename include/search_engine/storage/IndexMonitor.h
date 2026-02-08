#pragma once

#include "../../infrastructure.h"
#include <string>
#include <vector>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>

namespace search_engine {
namespace storage {

/**
 * @brief Index performance metrics
 */
struct IndexPerformance {
    std::string indexName;
    std::string collection;
    long long sizeBytes;
    long long usageCount;
    std::string indexSpec;  // JSON string of index keys
};

/**
 * @brief MongoDB index monitoring utility
 * 
 * Provides insights into index usage and performance for maintenance.
 */
class IndexMonitor {
public:
    /**
     * @brief Get index statistics for a collection
     * @param database MongoDB database
     * @param collectionName Collection to analyze
     * @return Result with vector of index performance metrics
     */
    static Result<std::vector<IndexPerformance>> getIndexStats(
        mongocxx::database& database,
        const std::string& collectionName
    );
    
    /**
     * @brief Get index statistics for all profile-related collections
     * @param database MongoDB database
     * @return Result with vector of index performance metrics across collections
     */
    static Result<std::vector<IndexPerformance>> getAllProfileIndexStats(
        mongocxx::database& database
    );
    
    /**
     * @brief Detect potentially unused indexes
     * @param database MongoDB database
     * @param collectionName Collection to analyze
     * @return Result with vector of index names that may be unused
     */
    static Result<std::vector<std::string>> detectUnusedIndexes(
        mongocxx::database& database,
        const std::string& collectionName
    );
};

} // namespace storage
} // namespace search_engine
