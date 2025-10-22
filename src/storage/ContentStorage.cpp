#include "../../include/search_engine/storage/ContentStorage.h"
#include "../../include/search_engine/storage/ApiRequestLog.h"
#include "../../include/search_engine/common/UrlCanonicalizer.h"
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
    const std::string& mongoDatabaseName,
    const std::string& redisConnectionString,
    const std::string& redisIndexName
) {
    LOG_DEBUG("ContentStorage constructor called");
    
    // Store connection parameters for lazy initialization
    mongoConnectionString_ = mongoConnectionString;
    mongoDatabaseName_ = mongoDatabaseName;
    redisConnectionString_ = redisConnectionString;
    redisIndexName_ = redisIndexName;
    
    // Initialize connection state
    mongoConnected_ = false;
    redisConnected_ = false;
    
    LOG_INFO("ContentStorage initialized with lazy connection handling");
    LOG_INFO("MongoDB will connect at: " + mongoConnectionString);
    LOG_INFO("Redis will connect at: " + redisConnectionString);
}

// Private method to ensure MongoDB connection (without locking - caller must lock)
void ContentStorage::ensureMongoConnectionUnsafe() {
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

// Public method to ensure MongoDB connection (with locking)
void ContentStorage::ensureMongoConnection() {
    std::lock_guard<std::mutex> lock(mongoMutex_);
    ensureMongoConnectionUnsafe();
}

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

IndexedPage ContentStorage::crawlResultToSiteProfile(const CrawlResult& crawlResult) const {
    IndexedPage page;
    
    // Use final URL after redirects if available, otherwise use original URL
    std::string effectiveUrl = (!crawlResult.finalUrl.empty()) ? crawlResult.finalUrl : crawlResult.url;
    
    // Basic information
    page.url = effectiveUrl;
    page.domain = extractDomain(effectiveUrl);
    
    // Canonicalize URL for deduplication using the effective URL
    page.canonicalUrl = search_engine::common::UrlCanonicalizer::canonicalize(effectiveUrl);
    page.canonicalHost = search_engine::common::UrlCanonicalizer::extractCanonicalHost(effectiveUrl);
    page.canonicalPath = search_engine::common::UrlCanonicalizer::extractCanonicalPath(effectiveUrl);
    page.canonicalQuery = search_engine::common::UrlCanonicalizer::extractCanonicalQuery(effectiveUrl);
    
    LOG_INFO("=== CANONICALIZATION DEBUG ===");
    LOG_INFO("Original URL: " + crawlResult.url);
    LOG_INFO("Final URL: " + crawlResult.finalUrl);
    LOG_INFO("Effective URL (used for storage): " + effectiveUrl);
    LOG_INFO("Canonical URL: " + page.canonicalUrl);
    LOG_INFO("Canonical Host: " + page.canonicalHost);
    LOG_INFO("Canonical Path: " + page.canonicalPath);
    LOG_INFO("Canonical Query: " + page.canonicalQuery);
    
    page.title = crawlResult.title.value_or("");
    page.description = crawlResult.metaDescription;
    page.textContent = crawlResult.textContent;
    
    // Technical metadata
    page.crawlMetadata.lastCrawlTime = crawlResult.crawlTime;
    page.crawlMetadata.firstCrawlTime = crawlResult.crawlTime; // Will be updated if exists
    page.crawlMetadata.lastCrawlStatus = crawlResult.success ? CrawlStatus::SUCCESS : CrawlStatus::FAILED;
    page.crawlMetadata.lastErrorMessage = crawlResult.errorMessage;
    page.crawlMetadata.crawlCount = 1; // Will be updated if exists
    page.crawlMetadata.crawlIntervalHours = 24.0; // Default interval
    page.crawlMetadata.userAgent = "Hatefbot/1.0";
    page.crawlMetadata.httpStatusCode = crawlResult.statusCode;
    page.crawlMetadata.contentSize = crawlResult.contentSize;
    page.crawlMetadata.contentType = crawlResult.contentType;
    page.crawlMetadata.crawlDurationMs = 0.0; // Not available in CrawlResult
    
    // Extract keywords from content
    if (crawlResult.textContent) {
        page.keywords = extractKeywords(*crawlResult.textContent);
        page.wordCount = countWords(*crawlResult.textContent);
    }
    
    // Set technical flags
    page.hasSSL = hasSSL(crawlResult.url);
    page.isIndexed = crawlResult.success;
    page.lastModified = crawlResult.crawlTime;
    page.indexedAt = crawlResult.crawlTime;
    
    // Extract outbound links
    page.outboundLinks = crawlResult.links;
    
    // Set default quality score based on content length and status
    if (crawlResult.success && crawlResult.textContent && !crawlResult.textContent->empty()) {
        double contentLength = static_cast<double>(crawlResult.textContent->length());
        page.contentQuality = std::min(1.0, contentLength / 10000.0); // Normalize to 0-1
    } else {
        page.contentQuality = 0.0;
    }
    
    return page;
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
        // Lock mutex for entire operation
        std::lock_guard<std::mutex> lock(mongoMutex_);
        
        // Ensure MongoDB connection before proceeding
        ensureMongoConnectionUnsafe();
        
        // Check connection state
        if (!mongoConnected_ || !mongoStorage_) {
            return Result<std::string>::Failure("MongoDB not available");
        }
        
        // Convert CrawlResult to IndexedPage
        IndexedPage page = crawlResultToSiteProfile(crawlResult);
        LOG_TRACE("CrawlResult converted to IndexedPage for URL: " + crawlResult.url);
        
        // Check if indexed page already exists
        auto existingProfile = mongoStorage_->getSiteProfile(crawlResult.url);
        if (existingProfile.success) {
            LOG_INFO("Updating existing indexed page for URL: " + crawlResult.url);
            // Update existing page
            auto existing = existingProfile.value;
            
            // Update crawl metadata
            page.id = existing.id;
            page.crawlMetadata.firstCrawlTime = existing.crawlMetadata.firstCrawlTime;
            page.crawlMetadata.crawlCount = existing.crawlMetadata.crawlCount + 1;
            
            // Keep existing fields that might have been manually set
            if (!existing.category.has_value() && page.category.has_value()) {
                page.category = existing.category;
            }
            if (existing.pageRank.has_value()) {
                page.pageRank = existing.pageRank;
            }
            if (existing.inboundLinkCount.has_value()) {
                page.inboundLinkCount = existing.inboundLinkCount;
            }
            
            // Update the page in MongoDB
            auto mongoResult = mongoStorage_->storeIndexedPage(page);
            if (!mongoResult.success) {
                LOG_ERROR("Failed to update indexed page in MongoDB for URL: " + crawlResult.url + " - " + mongoResult.message);
                return Result<std::string>::Failure("Failed to update in MongoDB: " + mongoResult.message);
            }
        } else {
            LOG_INFO("Storing new indexed page for URL: " + crawlResult.url);
            // Store new page in MongoDB
            auto mongoResult = mongoStorage_->storeIndexedPage(page);
            if (!mongoResult.success) {
                LOG_ERROR("Failed to store indexed page in MongoDB for URL: " + crawlResult.url + " - " + mongoResult.message);
                return Result<std::string>::Failure("Failed to store in MongoDB: " + mongoResult.message);
            }
            page.id = mongoResult.value;
        }
        
        // Index in Redis if successful and has content
        if (crawlResult.success && crawlResult.textContent) {
            LOG_DEBUG("Indexing content in Redis for URL: " + crawlResult.url);
            
            // Ensure Redis connection before indexing
            ensureRedisConnection();
            if (redisConnected_ && redisStorage_) {
                std::string searchableContent = extractSearchableContent(crawlResult);
                auto redisResult = redisStorage_->indexSiteProfile(page, searchableContent);
                if (!redisResult.success) {
                    LOG_WARNING("Failed to index in Redis for URL: " + crawlResult.url + " - " + redisResult.message);
                    // Log warning but don't fail the operation
                    // In a real system, you might want to queue this for retry
                }
            } else {
                LOG_WARNING("Redis not available for indexing URL: " + crawlResult.url);
            }
        }
        
        LOG_INFO("Crawl result stored successfully for URL: " + crawlResult.url + " (ID: " + page.id.value_or("") + ")");
        return Result<std::string>::Success(
            page.id.value_or(""),
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

Result<IndexedPage> ContentStorage::getSiteProfile(const std::string& url) {
    ensureMongoConnection();
    if (!mongoConnected_ || !mongoStorage_) {
        return Result<IndexedPage>::Failure("MongoDB not available");
    }
    return mongoStorage_->getSiteProfile(url);
}

Result<std::vector<IndexedPage>> ContentStorage::getSiteProfilesByDomain(const std::string& domain) {
    ensureMongoConnection();
    if (!mongoConnected_ || !mongoStorage_) {
        return Result<std::vector<IndexedPage>>::Failure("MongoDB not available");
    }
    return mongoStorage_->getSiteProfilesByDomain(domain);
}

Result<std::vector<IndexedPage>> ContentStorage::getSiteProfilesByCrawlStatus(CrawlStatus status) {
    ensureMongoConnection();
    if (!mongoConnected_ || !mongoStorage_) {
        return Result<std::vector<IndexedPage>>::Failure("MongoDB not available");
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
        
        // Initialize Redis search index
        auto redisResult = redisStorage_->initializeIndex();
        if (!redisResult.success) {
            return Result<bool>::Failure("Failed to initialize Redis index: " + redisResult.message);
        }
        
        return Result<bool>::Success(true, "All indexes initialized successfully");
        
    } catch (const std::exception& e) {
        return Result<bool>::Failure("Exception in initializeIndexes: " + std::string(e.what()));
    }
}

Result<bool> ContentStorage::reindexAll() {
    return redisStorage_->reindexAll();
}

Result<bool> ContentStorage::dropIndexes() {
    return redisStorage_->dropIndex();
}

Result<bool> ContentStorage::testConnections() {
    try {
        // Test MongoDB connection
        auto mongoResult = mongoStorage_->testConnection();
        if (!mongoResult.success) {
            return Result<bool>::Failure("MongoDB connection failed: " + mongoResult.message);
        }
        
        // Test Redis connection
        auto redisResult = redisStorage_->testConnection();
        if (!redisResult.success) {
            return Result<bool>::Failure("Redis connection failed: " + redisResult.message);
        }
        
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
        
        // Delete from Redis (ignore if not found)
        auto redisResult = redisStorage_->deleteDocument(url);
        
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
        
        // Delete each page
        for (const auto& page : profiles.value) {
            auto deleteResult = deleteSiteData(page.url);
            if (!deleteResult.success) {
                return Result<bool>::Failure("Failed to delete site data for " + page.url);
            }
        }
        
        // Delete from Redis by domain
        auto redisResult = redisStorage_->deleteDocumentsByDomain(domain);
        
        return Result<bool>::Success(true, "Domain data deleted successfully");
        
    } catch (const std::exception& e) {
        return Result<bool>::Failure("Exception in deleteDomainData: " + std::string(e.what()));
    }
} 