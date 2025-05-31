#include "../include/content_storage.h"
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/helpers.hpp>
#include <bsoncxx/json.hpp>
#include <mongocxx/exception/exception.hpp>
#include <sw/redis++/redis++.hpp>
#include <sw/redis++/errors.hpp>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <nlohmann/json.hpp>

using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;
using json = nlohmann::json;

// Singleton class to manage MongoDB instance
class MongoDBInstance {
private:
    static std::unique_ptr<mongocxx::instance> instance;
    static std::mutex mutex;

public:
    static mongocxx::instance& getInstance() {
        std::lock_guard<std::mutex> lock(mutex);
        if (!instance) {
            instance = std::make_unique<mongocxx::instance>();
        }
        return *instance;
    }
};

// Initialize static members
std::unique_ptr<mongocxx::instance> MongoDBInstance::instance;
std::mutex MongoDBInstance::mutex;

// WebsiteProfileStorage implementation
WebsiteProfileStorage::WebsiteProfileStorage(const std::string& uri, 
                                           const std::string& dbName, 
                                           const std::string& collectionName)
    : _uri(uri), _client(_uri), _dbName(dbName), _collectionName(collectionName) {
    // Initialize MongoDB instance
    MongoDBInstance::getInstance();
}

Result<bool> WebsiteProfileStorage::storeWebsiteProfile(const CrawlResult& result) {
    try {
        auto database = _client[_dbName];
        auto collection = database[_collectionName];

        // Check if document already exists
        auto filter = document{} << "url" << result.url << finalize;
        auto existingDoc = collection.find_one(filter.view());

        // Prepare document
        auto builder = document{};
        builder << "url" << result.url
                << "statusCode" << result.statusCode
                << "contentType" << result.contentType
                << "contentSize" << static_cast<int64_t>(result.contentSize)
                << "crawlTime" << bsoncxx::types::b_date(result.crawlTime)
                << "success" << result.success;

        // Add optional fields if they exist
        if (result.title) {
            builder << "title" << *result.title;
        }
        
        if (result.metaDescription) {
            builder << "metaDescription" << *result.metaDescription;
        }
        
        if (result.errorMessage) {
            builder << "errorMessage" << *result.errorMessage;
        }

        // Add links as array
        auto linkArray = bsoncxx::builder::basic::array{};
        for (const auto& link : result.links) {
            linkArray.append(link);
        }
        builder << "links" << linkArray;

        // Extract domain from URL
        std::string domain;
        size_t start = result.url.find("://");
        if (start != std::string::npos) {
            start += 3;
            size_t end = result.url.find('/', start);
            domain = result.url.substr(start, end - start);
        } else {
            domain = result.url;
        }
        builder << "domain" << domain;

        // Add last updated timestamp
        builder << "lastUpdated" << bsoncxx::types::b_date(std::chrono::system_clock::now());

        if (existingDoc) {
            // Update existing document
            collection.replace_one(filter.view(), builder.view());
            return Result<bool>::Success(true, "Website profile updated");
        } else {
            // Insert new document
            collection.insert_one(builder.view());
            return Result<bool>::Success(true, "Website profile created");
        }
    } catch (const mongocxx::exception& e) {
        return Result<bool>::Failure(std::string("MongoDB error: ") + e.what());
    } catch (const std::exception& e) {
        return Result<bool>::Failure(std::string("Error: ") + e.what());
    }
}

Result<std::string> WebsiteProfileStorage::getWebsiteProfile(const std::string& url) {
    try {
        auto database = _client[_dbName];
        auto collection = database[_collectionName];

        auto filter = document{} << "url" << url << finalize;
        auto result = collection.find_one(filter.view());

        if (result) {
            return Result<std::string>::Success(
                bsoncxx::to_json(result.value()),
                "Website profile retrieved"
            );
        } else {
            return Result<std::string>::Failure("Website profile not found");
        }
    } catch (const mongocxx::exception& e) {
        return Result<std::string>::Failure(std::string("MongoDB error: ") + e.what());
    } catch (const std::exception& e) {
        return Result<std::string>::Failure(std::string("Error: ") + e.what());
    }
}

Result<bool> WebsiteProfileStorage::updateWebsiteProfile(const std::string& url, const std::string& updateJson) {
    try {
        auto database = _client[_dbName];
        auto collection = database[_collectionName];

        auto filter = document{} << "url" << url << finalize;
        
        // Parse the update JSON
        auto updateDoc = bsoncxx::from_json(updateJson);
        
        // Add last updated timestamp
        auto updateBuilder = document{};
        updateBuilder << "$set" << bsoncxx::from_json(updateJson);
        updateBuilder << "$set" << open_document 
                     << "lastUpdated" << bsoncxx::types::b_date(std::chrono::system_clock::now())
                     << close_document;

        auto result = collection.update_one(filter.view(), updateBuilder.view());

        if (result && result->modified_count() > 0) {
            return Result<bool>::Success(true, "Website profile updated");
        } else {
            return Result<bool>::Failure("Website profile not found or not modified");
        }
    } catch (const mongocxx::exception& e) {
        return Result<bool>::Failure(std::string("MongoDB error: ") + e.what());
    } catch (const std::exception& e) {
        return Result<bool>::Failure(std::string("Error: ") + e.what());
    }
}

Result<bool> WebsiteProfileStorage::deleteWebsiteProfile(const std::string& url) {
    try {
        auto database = _client[_dbName];
        auto collection = database[_collectionName];

        auto filter = document{} << "url" << url << finalize;
        auto result = collection.delete_one(filter.view());

        if (result && result->deleted_count() > 0) {
            return Result<bool>::Success(true, "Website profile deleted");
        } else {
            return Result<bool>::Failure("Website profile not found");
        }
    } catch (const mongocxx::exception& e) {
        return Result<bool>::Failure(std::string("MongoDB error: ") + e.what());
    } catch (const std::exception& e) {
        return Result<bool>::Failure(std::string("Error: ") + e.what());
    }
}

// ContentSearchStorage implementation
ContentSearchStorage::ContentSearchStorage(const std::string& uri, const std::string& indexName)
    : _indexName(indexName) {
    try {
        _redis = std::make_unique<sw::redis::Redis>(uri);
    } catch (const sw::redis::Error& e) {
        // Log error but don't throw - we'll handle connection errors in the methods
        std::cerr << "Redis connection error: " << e.what() << std::endl;
    }
}

Result<bool> ContentSearchStorage::initializeIndex() {
    try {
        if (!_redis) {
            return Result<bool>::Failure("Redis client not initialized");
        }

        // Check if index exists
        bool indexExists = false;
        try {
            auto indices = _redis->command<std::vector<std::string>>("FT._LIST");
            indexExists = std::find(indices.begin(), indices.end(), _indexName) != indices.end();
        } catch (const sw::redis::Error& e) {
            // FT._LIST might not be available in older versions, ignore this error
        }

        if (!indexExists) {
            // Create the search index with fields for title, content, URL, and metadata
            std::string createIndexCmd = "FT.CREATE " + _indexName + 
                " ON HASH PREFIX 1 page: " +
                " SCHEMA title TEXT WEIGHT 5.0 " +
                " content TEXT WEIGHT 1.0 " +
                " url TEXT NOSTEM " +
                " domain TEXT NOSTEM " +
                " description TEXT WEIGHT 3.0";
            
            _redis->command<long long>(createIndexCmd);
            return Result<bool>::Success(true, "Search index created");
        }
        
        return Result<bool>::Success(true, "Search index already exists");
    } catch (const sw::redis::Error& e) {
        return Result<bool>::Failure(std::string("Redis error: ") + e.what());
    } catch (const std::exception& e) {
        return Result<bool>::Failure(std::string("Error: ") + e.what());
    }
}

std::string ContentSearchStorage::createDocId(const std::string& url) {
    // Create a unique document ID from the URL
    std::string docId = "page:";
    
    // Simple URL encoding for special characters
    for (char c : url) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            docId += c;
        } else if (c == '/') {
            docId += '_';
        } else {
            std::stringstream ss;
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
            docId += ss.str();
        }
    }
    
    return docId;
}

Result<bool> ContentSearchStorage::storePageContent(const CrawlResult& result) {
    try {
        if (!_redis) {
            return Result<bool>::Failure("Redis client not initialized");
        }

        // Ensure index exists
        auto initResult = initializeIndex();
        if (!initResult.success) {
            return initResult;
        }

        // Create document ID from URL
        std::string docId = createDocId(result.url);
        
        // Extract domain from URL
        std::string domain;
        size_t start = result.url.find("://");
        if (start != std::string::npos) {
            start += 3;
            size_t end = result.url.find('/', start);
            domain = result.url.substr(start, end - start);
        } else {
            domain = result.url;
        }

        // Prepare fields for the document
        std::unordered_map<std::string, std::string> fields;
        fields["url"] = result.url;
        fields["domain"] = domain;
        
        // Add title if available
        if (result.title) {
            fields["title"] = *result.title;
        } else {
            fields["title"] = "";
        }
        
        // Add description if available
        if (result.metaDescription) {
            fields["description"] = *result.metaDescription;
        } else {
            fields["description"] = "";
        }
        
        // Add content if available
        if (result.textContent) {
            fields["content"] = *result.textContent;
        } else if (result.rawContent) {
            fields["content"] = *result.rawContent;
        } else {
            fields["content"] = "";
        }
        
        // Store the document in Redis
        _redis->hset(docId, fields.begin(), fields.end());
        
        return Result<bool>::Success(true, "Page content stored for search");
    } catch (const sw::redis::Error& e) {
        return Result<bool>::Failure(std::string("Redis error: ") + e.what());
    } catch (const std::exception& e) {
        return Result<bool>::Failure(std::string("Error: ") + e.what());
    }
}

Result<std::string> ContentSearchStorage::search(const std::string& query, int limit) {
    try {
        if (!_redis) {
            return Result<std::string>::Failure("Redis client not initialized");
        }

        // Ensure index exists
        auto initResult = initializeIndex();
        if (!initResult.success) {
            return Result<std::string>::Failure(initResult.message);
        }

        // Perform search using FT.SEARCH
        std::string searchCmd = "FT.SEARCH " + _indexName + " '" + query + "' LIMIT 0 " + std::to_string(limit);
        auto reply = _redis->command<std::vector<std::string>>(searchCmd);
        
        // Parse results into JSON
        json results = json::array();
        
        if (!reply.empty() && reply.size() > 1) {
            // First element is the count of results
            int totalResults = std::stoi(reply[0]);
            
            // Process results (each result has a key followed by field-value pairs)
            for (size_t i = 1; i < reply.size(); i += 2) {
                std::string key = reply[i];
                
                // Next element contains field-value pairs
                if (i + 1 < reply.size()) {
                    json resultObj;
                    resultObj["id"] = key;
                    
                    std::vector<std::string> fields = reply[i + 1];
                    for (size_t j = 0; j < fields.size(); j += 2) {
                        if (j + 1 < fields.size()) {
                            resultObj[fields[j]] = fields[j + 1];
                        }
                    }
                    
                    results.push_back(resultObj);
                }
            }
        }
        
        json response = {
            {"total", results.size()},
            {"results", results}
        };
        
        return Result<std::string>::Success(response.dump(), "Search completed");
    } catch (const sw::redis::Error& e) {
        return Result<std::string>::Failure(std::string("Redis error: ") + e.what());
    } catch (const std::exception& e) {
        return Result<std::string>::Failure(std::string("Error: ") + e.what());
    }
}

Result<bool> ContentSearchStorage::deletePageContent(const std::string& url) {
    try {
        if (!_redis) {
            return Result<bool>::Failure("Redis client not initialized");
        }

        // Create document ID from URL
        std::string docId = createDocId(url);
        
        // Delete the document
        long long deleted = _redis->del(docId);
        
        if (deleted > 0) {
            return Result<bool>::Success(true, "Page content deleted");
        } else {
            return Result<bool>::Failure("Page content not found");
        }
    } catch (const sw::redis::Error& e) {
        return Result<bool>::Failure(std::string("Redis error: ") + e.what());
    } catch (const std::exception& e) {
        return Result<bool>::Failure(std::string("Error: ") + e.what());
    }
}

// ContentStorage implementation
ContentStorage::ContentStorage(const std::string& mongoUri, const std::string& redisUri)
    : _profileStorage(mongoUri), _contentStorage(redisUri) {
}

Result<bool> ContentStorage::storeCrawlResult(const CrawlResult& result) {
    // Store in MongoDB
    auto mongoResult = _profileStorage.storeWebsiteProfile(result);
    if (!mongoResult.success) {
        return mongoResult;
    }
    
    // Store in RedisSearch
    auto redisResult = _contentStorage.storePageContent(result);
    if (!redisResult.success) {
        return redisResult;
    }
    
    return Result<bool>::Success(true, "Content stored successfully in both MongoDB and RedisSearch");
}

Result<std::string> ContentStorage::search(const std::string& query, int limit) {
    return _contentStorage.search(query, limit);
}

Result<std::string> ContentStorage::getWebsiteProfile(const std::string& url) {
    return _profileStorage.getWebsiteProfile(url);
}

Result<bool> ContentStorage::deleteContent(const std::string& url) {
    // Delete from MongoDB
    auto mongoResult = _profileStorage.deleteWebsiteProfile(url);
    
    // Delete from RedisSearch
    auto redisResult = _contentStorage.deletePageContent(url);
    
    // Return success only if both operations succeeded
    if (mongoResult.success && redisResult.success) {
        return Result<bool>::Success(true, "Content deleted from both MongoDB and RedisSearch");
    } else if (!mongoResult.success) {
        return mongoResult;
    } else {
        return redisResult;
    }
}