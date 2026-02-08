#include "../../include/search_engine/storage/IndexMonitor.h"
#include "../../include/Logger.h"
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>
#include <mongocxx/exception/exception.hpp>

namespace search_engine {
namespace storage {

Result<std::vector<IndexPerformance>> IndexMonitor::getIndexStats(
    mongocxx::database& database,
    const std::string& collectionName) {
    
    try {
        using bsoncxx::builder::basic::kvp;
        using bsoncxx::builder::basic::make_document;
        
        auto collection = database[collectionName];
        std::vector<IndexPerformance> stats;
        
        // Get index information using listIndexes
        try {
            auto indexesCursor = collection.list_indexes();
            
            for (const auto& indexDoc : indexesCursor) {
                IndexPerformance perf;
                perf.collection = collectionName;
                
                // Get index name
                if (indexDoc["name"]) {
                    perf.indexName = std::string(indexDoc["name"].get_string().value);
                } else {
                    perf.indexName = "unnamed";
                }
                
                // Get index specification (keys)
                if (indexDoc["key"]) {
                    perf.indexSpec = bsoncxx::to_json(indexDoc["key"].get_document().view());
                } else {
                    perf.indexSpec = "{}";
                }
                
                // Get index size using $collStats
                // Note: Individual index size requires aggregation with $indexStats
                perf.sizeBytes = 0;  // Would need $indexStats for accurate size
                perf.usageCount = 0;  // Would need $indexStats for usage count
                
                stats.push_back(perf);
            }
        } catch (const mongocxx::exception& e) {
            LOG_WARNING("Failed to list indexes for collection " + collectionName + ": " + std::string(e.what()));
        }
        
        return Result<std::vector<IndexPerformance>>::Success(
            stats,
            "Retrieved " + std::to_string(stats.size()) + " indexes for " + collectionName
        );
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error getting index stats: " + std::string(e.what()));
        return Result<std::vector<IndexPerformance>>::Failure(
            "Database error: " + std::string(e.what())
        );
    }
}

Result<std::vector<IndexPerformance>> IndexMonitor::getAllProfileIndexStats(
    mongocxx::database& database) {
    
    try {
        std::vector<IndexPerformance> allStats;
        
        // Profile-related collections
        std::vector<std::string> collections = {
            "profiles",
            "profile_view_analytics",
            "legal_compliance_logs",
            "profile_audit_logs"
        };
        
        for (const auto& collectionName : collections) {
            auto result = getIndexStats(database, collectionName);
            if (result.success) {
                allStats.insert(allStats.end(), result.value.begin(), result.value.end());
            }
        }
        
        return Result<std::vector<IndexPerformance>>::Success(
            allStats,
            "Retrieved indexes from " + std::to_string(collections.size()) + " collections"
        );
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error getting all profile index stats: " + std::string(e.what()));
        return Result<std::vector<IndexPerformance>>::Failure(
            "Error: " + std::string(e.what())
        );
    }
}

Result<std::vector<std::string>> IndexMonitor::detectUnusedIndexes(
    mongocxx::database& database,
    const std::string& collectionName) {
    
    try {
        std::vector<std::string> unusedIndexes;
        
        // This is a simplified implementation
        // In production, you'd track index usage over time using $indexStats
        // and flag indexes with very low usage counts
        
        LOG_INFO("Unused index detection for " + collectionName + " (simplified implementation)");
        
        // Note: Real implementation would use:
        // db.collection.aggregate([{$indexStats: {}}])
        // and check "accesses.ops" field to find unused indexes
        
        return Result<std::vector<std::string>>::Success(
            unusedIndexes,
            "Unused index detection requires production usage data"
        );
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error detecting unused indexes: " + std::string(e.what()));
        return Result<std::vector<std::string>>::Failure(
            "Database error: " + std::string(e.what())
        );
    }
}

} // namespace storage
} // namespace search_engine
