#include "../../include/search_engine/storage/ContentStorage.h"
#include "../../include/search_engine/storage/ApiRequestLog.h"
#include "../../include/Logger.h"
#include <mongocxx/exception/exception.hpp>
#include <regex>
#include <algorithm>
#include <sstream>

using namespace search_engine::storage;

namespace {
    // Helper function to extract domain from URL
    std::string extractDomain(const std::string& url) {
        std::regex domainRegex(R"(https?://(?:www\.)?([^/]+))");
        std::smatch match;
        if (std::regex_search(url, match, domainRegex)) {
            return match[1].str();
        }
        return "";
    }
    
    // Helper function to check if URL uses HTTPS
    bool hasSSL(const std::string& url) {
        return url.find("https://") == 0;
    }
    
    // Helper function to count words in text
    int countWords(const std::string& text) {
        std::istringstream stream(text);
        std::string word;
        int count = 0;
        while (stream >> word) {
            ++count;
        }
        return count;
    }
    
    // Helper function to extract keywords from text (simple implementation)
    std::vector<std::string> extractKeywords(const std::string& text, int maxKeywords = 10) {
        std::vector<std::string> keywords;
        std::istringstream stream(text);
        std::string word;
        std::unordered_map<std::string, int> wordCount;
        
        // Count word frequencies
        while (stream >> word) {
            // Remove punctuation and convert to lowercase
            word.erase(std::remove_if(word.begin(), word.end(), ::ispunct), word.end());
            std::transform(word.begin(), word.end(), word.begin(), ::tolower);
            
            // Skip short words and common stop words
            if (word.length() > 3 && 
                word != "the" && word != "and" && word != "for" && 
                word != "are" && word != "but" && word != "not" && 
                word != "you" && word != "all" && word != "can" &&
                word != "had" && word != "her" && word != "was" &&
                word != "one" && word != "our" && word != "out" &&
                word != "day" && word != "get" && word != "has" &&
                word != "him" && word != "his" && word != "how" &&
                word != "its" && word != "may" && word != "new" &&
                word != "now" && word != "old" && word != "see" &&
                word != "two" && word != "who" && word != "boy" &&
                word != "did" && word != "she" && word != "use" &&
                word != "her" && word != "how" && word != "man" &&
                word != "way") {
                wordCount[word]++;
            }
        }
        
        // Sort by frequency and take top keywords
        std::vector<std::pair<std::string, int>> sortedWords(wordCount.begin(), wordCount.end());
        std::sort(sortedWords.begin(), sortedWords.end(), 
                  [](const auto& a, const auto& b) { return a.second > b.second; });
        
        for (int i = 0; i < std::min(maxKeywords, static_cast<int>(sortedWords.size())); ++i) {
            keywords.push_back(sortedWords[i].first);
        }
        
        return keywords;
    }
}

ContentStorage::ContentStorage(
    const std::string& mongoConnectionString,
    const std::string& mongoDatabaseName
#ifdef REDIS_AVAILABLE
    ,const std::string& redisConnectionString,
    const std::string& redisIndexName
#endif
) {
    LOG_DEBUG("ContentStorage constructor called");
    
    // Store connection parameters for lazy initialization
    mongoConnectionString_ = mongoConnectionString;
    mongoDatabaseName_ = mongoDatabaseName;
#ifdef REDIS_AVAILABLE
    redisConnectionString_ = redisConnectionString;
    redisIndexName_ = redisIndexName;
#endif
    
    // Initialize connection state
    mongoConnected_ = false;
#ifdef REDIS_AVAILABLE
    redisConnected_ = false;
#endif
    
    LOG_INFO("ContentStorage initialized with lazy connection handling");
    LOG_INFO("MongoDB will connect at: " + mongoConnectionString);
#ifdef REDIS_AVAILABLE
    LOG_INFO("Redis will connect at: " + redisConnectionString);
#endif
}

// Private method to ensure MongoDB connection
void ContentStorage::ensureMongoConnection() {
    if (!mongoConnected_ || !mongoStorage_) {
        try {
            LOG_DEBUG("Initializing MongoDB connection...");
            
            // Create MongoDBStorage with proper error handling
            mongoStorage_ = std::make_unique<MongoDBStorage>(mongoConnectionString_, mongoDatabaseName_);
            
            // Test connection with timeout and retry logic
            auto mongoTest = mongoStorage_->testConnection();
            if (mongoTest.success) {
                mongoConnected_ = true;
                LOG_INFO("MongoDB connection established successfully");
            } else {
                LOG_WARNING("MongoDB connection test failed: " + mongoTest.message);
                mongoConnected_ = false;
                mongoStorage_.reset(); // Clean up failed connection
            }
        } catch (const mongocxx::exception& e) {
            LOG_ERROR("MongoDB connection error: " + std::string(e.what()));
            mongoConnected_ = false;
            mongoStorage_.reset(); // Clean up failed connection
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to initialize MongoDB connection: " + std::string(e.what()));
            mongoConnected_ = false;
            mongoStorage_.reset(); // Clean up failed connection
        }
    }
}

#ifdef REDIS_AVAILABLE
// Private method to ensure Redis connection
void ContentStorage::ensureRedisConnection() {
    if (!redisConnected_ || !redisStorage_) {
        try {
            LOG_DEBUG("Initializing Redis connection...");
            redisStorage_ = std::make_unique<RedisSearchStorage>(redisConnectionString_, redisIndexName_);
            
            // Test connection without blocking startup
            auto redisTest = redisStorage_->testConnection();
            if (redisTest.success) {
                redisConnected_ = true;
                LOG_INFO("Redis connection established successfully");
            } else {
                LOG_WARNING("Redis connection test failed: " + redisTest.message);
                // Don't throw - allow the service to start without Redis
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to initialize Redis connection: " + std::string(e.what()));
            // Don't throw - allow the service to start without Redis
        }
    }
}
#endif

SiteProfile ContentStorage::crawlResultToSiteProfile(const CrawlResult& crawlResult) const {
    SiteProfile profile;
    
    // Basic information
    profile.url = crawlResult.url;
    profile.domain = extractDomain(crawlResult.url);
    profile.title = crawlResult.title.value_or("");
    profile.description = crawlResult.metaDescription;
    profile.textContent = crawlResult.textContent;
    
    // Technical metadata
    profile.crawlMetadata.lastCrawlTime = crawlResult.crawlTime;
    profile.crawlMetadata.firstCrawlTime = crawlResult.crawlTime; // Will be updated if exists
    profile.crawlMetadata.lastCrawlStatus = crawlResult.success ? CrawlStatus::SUCCESS : CrawlStatus::FAILED;
    profile.crawlMetadata.lastErrorMessage = crawlResult.errorMessage;
    profile.crawlMetadata.crawlCount = 1; // Will be updated if exists
    profile.crawlMetadata.crawlIntervalHours = 24.0; // Default interval
    profile.crawlMetadata.userAgent = "Hatefbot/1.0";
    profile.crawlMetadata.httpStatusCode = crawlResult.statusCode;
    profile.crawlMetadata.contentSize = crawlResult.contentSize;
    profile.crawlMetadata.contentType = crawlResult.contentType;
    profile.crawlMetadata.crawlDurationMs = 0.0; // Not available in CrawlResult
    
    // Extract keywords from content
    if (crawlResult.textContent) {
        profile.keywords = extractKeywords(*crawlResult.textContent);
        profile.wordCount = countWords(*crawlResult.textContent);
    }
    
    // Set technical flags
    profile.hasSSL = hasSSL(crawlResult.url);
    profile.isIndexed = crawlResult.success;
    profile.lastModified = crawlResult.crawlTime;
    profile.indexedAt = crawlResult.crawlTime;
    
    // Extract outbound links
    profile.outboundLinks = crawlResult.links;
    
    // Set default quality score based on content length and status
    if (crawlResult.success && crawlResult.textContent && !crawlResult.textContent->empty()) {
        double contentLength = static_cast<double>(crawlResult.textContent->length());
        profile.contentQuality = std::min(1.0, contentLength / 10000.0); // Normalize to 0-1
    } else {
        profile.contentQuality = 0.0;
    }
    
    return profile;
}

std::string ContentStorage::extractSearchableContent(const CrawlResult& crawlResult) const {
    std::ostringstream content;
    
    // Add title with higher weight
    if (crawlResult.title) {
        content << *crawlResult.title << " ";
        content << *crawlResult.title << " "; // Add twice for weight
    }
    
    // Add meta description
    if (crawlResult.metaDescription) {
        content << *crawlResult.metaDescription << " ";
    }
    
    // Add main text content
    if (crawlResult.textContent) {
        content << *crawlResult.textContent;
    }
    
    return content.str();
}

Result<std::string> ContentStorage::storeCrawlResult(const CrawlResult& crawlResult) {
    LOG_DEBUG("ContentStorage::storeCrawlResult called for URL: " + crawlResult.url);
    try {
        // Ensure MongoDB connection before proceeding
        ensureMongoConnection();
        if (!mongoConnected_ || !mongoStorage_) {
            return Result<std::string>::Failure("MongoDB not available");
        }
        
        // Convert CrawlResult to SiteProfile
        SiteProfile profile = crawlResultToSiteProfile(crawlResult);
        LOG_TRACE("CrawlResult converted to SiteProfile for URL: " + crawlResult.url);
        
        // Check if site profile already exists
        auto existingProfile = mongoStorage_->getSiteProfile(crawlResult.url);
        if (existingProfile.success) {
            LOG_INFO("Updating existing site profile for URL: " + crawlResult.url);
            // Update existing profile
            auto existing = existingProfile.value;
            
            // Update crawl metadata
            profile.id = existing.id;
            profile.crawlMetadata.firstCrawlTime = existing.crawlMetadata.firstCrawlTime;
            profile.crawlMetadata.crawlCount = existing.crawlMetadata.crawlCount + 1;
            
            // Keep existing fields that might have been manually set
            if (!existing.category.has_value() && profile.category.has_value()) {
                profile.category = existing.category;
            }
            if (existing.pageRank.has_value()) {
                profile.pageRank = existing.pageRank;
            }
            if (existing.inboundLinkCount.has_value()) {
                profile.inboundLinkCount = existing.inboundLinkCount;
            }
            
            // Update the profile in MongoDB
            auto mongoResult = mongoStorage_->updateSiteProfile(profile);
            if (!mongoResult.success) {
                LOG_ERROR("Failed to update site profile in MongoDB for URL: " + crawlResult.url + " - " + mongoResult.message);
                return Result<std::string>::Failure("Failed to update in MongoDB: " + mongoResult.message);
            }
        } else {
            LOG_INFO("Storing new site profile for URL: " + crawlResult.url);
            // Store new profile in MongoDB
            auto mongoResult = mongoStorage_->storeSiteProfile(profile);
            if (!mongoResult.success) {
                LOG_ERROR("Failed to store site profile in MongoDB for URL: " + crawlResult.url + " - " + mongoResult.message);
                return Result<std::string>::Failure("Failed to store in MongoDB: " + mongoResult.message);
            }
            profile.id = mongoResult.value;
        }
        
        // Index in Redis if successful and has content
#ifdef REDIS_AVAILABLE
        if (crawlResult.success && crawlResult.textContent) {
            LOG_DEBUG("Indexing content in Redis for URL: " + crawlResult.url);
            
            // Ensure Redis connection before indexing
            ensureRedisConnection();
            if (redisConnected_ && redisStorage_) {
                std::string searchableContent = extractSearchableContent(crawlResult);
                auto redisResult = redisStorage_->indexSiteProfile(profile, searchableContent);
                if (!redisResult.success) {
                    LOG_WARNING("Failed to index in Redis for URL: " + crawlResult.url + " - " + redisResult.message);
                    // Log warning but don't fail the operation
                    // In a real system, you might want to queue this for retry
                }
            } else {
                LOG_WARNING("Redis not available for indexing URL: " + crawlResult.url);
            }
        }
#endif
        
        LOG_INFO("Crawl result stored successfully for URL: " + crawlResult.url + " (ID: " + profile.id.value_or("") + ")");
        return Result<std::string>::Success(
            profile.id.value_or(""),
            "Crawl result stored successfully"
        );
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in storeCrawlResult for URL: " + crawlResult.url + " - " + std::string(e.what()));
        return Result<std::string>::Failure("Exception in storeCrawlResult: " + std::string(e.what()));
    }
}

Result<bool> ContentStorage::updateCrawlResult(const CrawlResult& crawlResult) {
    // For updates, we use the same logic as store
    auto result = storeCrawlResult(crawlResult);
    return Result<bool>::Success(result.success, result.message);
}

Result<SiteProfile> ContentStorage::getSiteProfile(const std::string& url) {
    ensureMongoConnection();
    if (!mongoConnected_ || !mongoStorage_) {
        return Result<SiteProfile>::Failure("MongoDB not available");
    }
    return mongoStorage_->getSiteProfile(url);
}

Result<std::vector<SiteProfile>> ContentStorage::getSiteProfilesByDomain(const std::string& domain) {
    ensureMongoConnection();
    if (!mongoConnected_ || !mongoStorage_) {
        return Result<std::vector<SiteProfile>>::Failure("MongoDB not available");
    }
    return mongoStorage_->getSiteProfilesByDomain(domain);
}

Result<std::vector<SiteProfile>> ContentStorage::getSiteProfilesByCrawlStatus(CrawlStatus status) {
    ensureMongoConnection();
    if (!mongoConnected_ || !mongoStorage_) {
        return Result<std::vector<SiteProfile>>::Failure("MongoDB not available");
    }
    return mongoStorage_->getSiteProfilesByCrawlStatus(status);
}

Result<int64_t> ContentStorage::getTotalSiteCount() {
    ensureMongoConnection();
    if (!mongoConnected_ || !mongoStorage_) {
        return Result<int64_t>::Failure("MongoDB not available");
    }
    return mongoStorage_->getTotalSiteCount();
}

#ifdef REDIS_AVAILABLE
Result<SearchResponse> ContentStorage::search(const SearchQuery& query) {
    ensureRedisConnection();
    if (!redisConnected_ || !redisStorage_) {
        return Result<SearchResponse>::Failure("Redis not available");
    }
    return redisStorage_->search(query);
}

Result<SearchResponse> ContentStorage::searchSimple(const std::string& query, int limit) {
    ensureRedisConnection();
    if (!redisConnected_ || !redisStorage_) {
        return Result<SearchResponse>::Failure("Redis not available");
    }
    return redisStorage_->searchSimple(query, limit);
}

Result<std::vector<std::string>> ContentStorage::suggest(const std::string& prefix, int limit) {
    ensureRedisConnection();
    if (!redisConnected_ || !redisStorage_) {
        return Result<std::vector<std::string>>::Failure("Redis not available");
    }
    return redisStorage_->suggest(prefix, limit);
}
#endif

Result<std::vector<std::string>> ContentStorage::storeCrawlResults(const std::vector<CrawlResult>& crawlResults) {
    LOG_DEBUG("ContentStorage::storeCrawlResults called with " + std::to_string(crawlResults.size()) + " results");
    std::vector<std::string> ids;
    
    for (const auto& crawlResult : crawlResults) {
        auto result = storeCrawlResult(crawlResult);
        if (result.success) {
            ids.push_back(result.value);
        } else {
            LOG_ERROR("Failed to store crawl result for " + crawlResult.url + ": " + result.message);
            return Result<std::vector<std::string>>::Failure(
                "Failed to store crawl result for " + crawlResult.url + ": " + result.message
            );
        }
    }
    
    LOG_INFO("All " + std::to_string(crawlResults.size()) + " crawl results stored successfully");
    return Result<std::vector<std::string>>::Success(
        std::move(ids),
        "All crawl results stored successfully"
    );
}

Result<bool> ContentStorage::initializeIndexes() {
    try {
        // Initialize MongoDB indexes
        auto mongoResult = mongoStorage_->ensureIndexes();
        if (!mongoResult.success) {
            return Result<bool>::Failure("Failed to initialize MongoDB indexes: " + mongoResult.message);
        }
        
#ifdef REDIS_AVAILABLE
        // Initialize Redis search index
        auto redisResult = redisStorage_->initializeIndex();
        if (!redisResult.success) {
            return Result<bool>::Failure("Failed to initialize Redis index: " + redisResult.message);
        }
#endif
        
        return Result<bool>::Success(true, "All indexes initialized successfully");
        
    } catch (const std::exception& e) {
        return Result<bool>::Failure("Exception in initializeIndexes: " + std::string(e.what()));
    }
}

#ifdef REDIS_AVAILABLE
Result<bool> ContentStorage::reindexAll() {
    return redisStorage_->reindexAll();
}

Result<bool> ContentStorage::dropIndexes() {
    return redisStorage_->dropIndex();
}
#endif

Result<bool> ContentStorage::testConnections() {
    try {
        // Test MongoDB connection
        auto mongoResult = mongoStorage_->testConnection();
        if (!mongoResult.success) {
            return Result<bool>::Failure("MongoDB connection failed: " + mongoResult.message);
        }
        
#ifdef REDIS_AVAILABLE
        // Test Redis connection
        auto redisResult = redisStorage_->testConnection();
        if (!redisResult.success) {
            return Result<bool>::Failure("Redis connection failed: " + redisResult.message);
        }
#endif
        
        return Result<bool>::Success(true, "All connections are healthy");
        
    } catch (const std::exception& e) {
        return Result<bool>::Failure("Exception in testConnections: " + std::string(e.what()));
    }
}

Result<std::unordered_map<std::string, std::string>> ContentStorage::getStorageStats() {
    LOG_DEBUG("ContentStorage::getStorageStats() - Starting to retrieve storage statistics");
    try {
        LOG_DEBUG("ContentStorage::getStorageStats() - Entering try block");
        std::unordered_map<std::string, std::string> stats;
        LOG_DEBUG("ContentStorage::getStorageStats() - Created empty stats map");
        
        // Get MongoDB stats
        LOG_DEBUG("ContentStorage::getStorageStats() - Attempting to get MongoDB total site count");
        auto mongoCount = mongoStorage_->getTotalSiteCount();
        LOG_DEBUG("ContentStorage::getStorageStats() - MongoDB getTotalSiteCount() returned, success: " + std::string(mongoCount.success ? "true" : "false"));
        if (mongoCount.success) {
            LOG_DEBUG("ContentStorage::getStorageStats() - MongoDB total count: " + std::to_string(mongoCount.value));
            stats["mongodb_total_documents"] = std::to_string(mongoCount.value);
            LOG_DEBUG("ContentStorage::getStorageStats() - Added mongodb_total_documents to stats");
        } else {
            LOG_DEBUG("ContentStorage::getStorageStats() - Failed to get MongoDB total count: " + mongoCount.message);
        }
        
        LOG_DEBUG("ContentStorage::getStorageStats() - Attempting to get MongoDB successful crawls count");
        auto mongoSuccessCount = mongoStorage_->getSiteCountByStatus(CrawlStatus::SUCCESS);
        LOG_DEBUG("ContentStorage::getStorageStats() - MongoDB getSiteCountByStatus(SUCCESS) returned, success: " + std::string(mongoSuccessCount.success ? "true" : "false"));
        if (mongoSuccessCount.success) {
            LOG_DEBUG("ContentStorage::getStorageStats() - MongoDB successful crawls count: " + std::to_string(mongoSuccessCount.value));
            stats["mongodb_successful_crawls"] = std::to_string(mongoSuccessCount.value);
            LOG_DEBUG("ContentStorage::getStorageStats() - Added mongodb_successful_crawls to stats");
        } else {
            LOG_DEBUG("ContentStorage::getStorageStats() - Failed to get MongoDB successful crawls count: " + mongoSuccessCount.message);
        }
        
#ifdef REDIS_AVAILABLE
        LOG_DEBUG("ContentStorage::getStorageStats() - Redis is available, getting Redis stats");
        // Get Redis stats
        LOG_DEBUG("ContentStorage::getStorageStats() - Attempting to get Redis document count");
        auto redisCount = redisStorage_->getDocumentCount();
        LOG_DEBUG("ContentStorage::getStorageStats() - Redis getDocumentCount() returned, success: " + std::string(redisCount.success ? "true" : "false"));
        if (redisCount.success) {
            LOG_DEBUG("ContentStorage::getStorageStats() - Redis document count: " + std::to_string(redisCount.value));
            stats["redis_indexed_documents"] = std::to_string(redisCount.value);
            LOG_DEBUG("ContentStorage::getStorageStats() - Added redis_indexed_documents to stats");
        } else {
            LOG_DEBUG("ContentStorage::getStorageStats() - Redis document count failed: " + redisCount.message);
            // Add debug info about why redis count failed
            stats["redis_count_error"] = redisCount.message;
            LOG_DEBUG("ContentStorage::getStorageStats() - Added redis_count_error to stats");
        }
        
        LOG_DEBUG("ContentStorage::getStorageStats() - Attempting to get Redis index info");
        auto redisInfo = redisStorage_->getIndexInfo();
        LOG_DEBUG("ContentStorage::getStorageStats() - Redis getIndexInfo() returned, success: " + std::string(redisInfo.success ? "true" : "false"));
        if (redisInfo.success) {
            auto info = redisInfo.value;
            LOG_DEBUG("ContentStorage::getStorageStats() - Redis index info retrieved, processing " + std::to_string(info.size()) + " entries");
            for (const auto& [key, value] : info) {
                LOG_DEBUG("ContentStorage::getStorageStats() - Processing Redis info entry: " + key + " = " + value);
                stats["redis_" + key] = value;
                LOG_DEBUG("ContentStorage::getStorageStats() - Added redis_" + key + " to stats");
            }
        } else {
            LOG_DEBUG("ContentStorage::getStorageStats() - Redis index info failed: " + redisInfo.message);
            // Add debug info about why redis info failed
            stats["redis_info_error"] = redisInfo.message;
            LOG_DEBUG("ContentStorage::getStorageStats() - Added redis_info_error to stats");
        }
#else
        LOG_DEBUG("ContentStorage::getStorageStats() - Redis is not available (REDIS_AVAILABLE not defined)");
#endif
        
        LOG_DEBUG("ContentStorage::getStorageStats() - Preparing to return success result with " + std::to_string(stats.size()) + " stats entries");
        return Result<std::unordered_map<std::string, std::string>>::Success(
            std::move(stats),
            "Storage statistics retrieved successfully"
        );
        
    } catch (const std::exception& e) {
        LOG_DEBUG("ContentStorage::getStorageStats() - Exception caught: " + std::string(e.what()));
        return Result<std::unordered_map<std::string, std::string>>::Failure(
            "Exception in getStorageStats: " + std::string(e.what())
        );
    }
}

Result<bool> ContentStorage::deleteSiteData(const std::string& url) {
    try {
        // Delete from MongoDB
        auto mongoResult = mongoStorage_->deleteSiteProfile(url);
        
#ifdef REDIS_AVAILABLE
        // Delete from Redis (ignore if not found)
        auto redisResult = redisStorage_->deleteDocument(url);
#endif
        
        if (mongoResult.success) {
            return Result<bool>::Success(true, "Site data deleted successfully");
        } else {
            return Result<bool>::Failure("Failed to delete from MongoDB: " + mongoResult.message);
        }
        
    } catch (const std::exception& e) {
        return Result<bool>::Failure("Exception in deleteSiteData: " + std::string(e.what()));
    }
}

Result<bool> ContentStorage::deleteDomainData(const std::string& domain) {
    try {
        // Get all profiles for the domain first
        auto profiles = mongoStorage_->getSiteProfilesByDomain(domain);
        if (!profiles.success) {
            return Result<bool>::Failure("Failed to get profiles for domain: " + profiles.message);
        }
        
        // Delete each profile
        for (const auto& profile : profiles.value) {
            auto deleteResult = deleteSiteData(profile.url);
            if (!deleteResult.success) {
                return Result<bool>::Failure("Failed to delete site data for " + profile.url);
            }
        }
        
#ifdef REDIS_AVAILABLE
        // Delete from Redis by domain
        auto redisResult = redisStorage_->deleteDocumentsByDomain(domain);
#endif
        
        return Result<bool>::Success(true, "Domain data deleted successfully");
        
    } catch (const std::exception& e) {
        return Result<bool>::Failure("Exception in deleteDomainData: " + std::string(e.what()));
    }
} 