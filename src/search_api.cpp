#include "../include/search_api.h"
#include "../include/api.h"
#include "../include/Logger.h"
#include "../include/search_core/SearchClient.hpp"
#include "../include/search_engine/storage/RedisSearchStorage.h"
#include "../include/search_engine/common/UrlCanonicalizer.h"
#include <sstream>
#include <cstdlib>
#include <chrono>
#include <iomanip>

using json = nlohmann::json;
using namespace hatef::search;
using namespace search_engine::storage;

namespace search_api {

// URL decoding function for handling UTF-8 encoded query parameters
std::string urlDecode(const std::string& encoded) {
    std::string decoded;
    std::size_t len = encoded.length();
    
    for (std::size_t i = 0; i < len; ++i) {
        if (encoded[i] == '%' && (i + 2) < len) {
            // Convert hex to char
            std::string hex = encoded.substr(i + 1, 2);
            char ch = static_cast<char>(std::strtol(hex.c_str(), nullptr, 16));
            decoded.push_back(ch);
            i += 2;
        } else if (encoded[i] == '+') {
            decoded.push_back(' ');
        } else {
            decoded.push_back(encoded[i]);
        }
    }
    
    return decoded;
}

// Static SearchClient instance - initialized once
static std::unique_ptr<SearchClient> g_searchClient;
static std::once_flag g_initFlag;

// Helper function to get current timestamp in ISO 8601 format
std::string getCurrentISO8601Timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

// Initialize the SearchClient with configuration from environment
void initializeSearchClient() {
    std::call_once(g_initFlag, []() {
        RedisConfig config;
        
        // Get Redis URI from environment, default to localhost
        const char* redisUri = std::getenv("SEARCH_REDIS_URI");
        if (redisUri) {
            config.uri = redisUri;
            LOG_INFO("Using Redis URI from environment: " + config.uri);
        } else {
            // Default to tcp://127.0.0.1:6379
            LOG_INFO("Using default Redis URI: " + config.uri);
        }
        
        // Get pool size from environment if set
        const char* poolSize = std::getenv("SEARCH_REDIS_POOL_SIZE");
        if (poolSize) {
            try {
                config.pool_size = std::stoul(poolSize);
                LOG_INFO("Using Redis pool size from environment: " + std::to_string(config.pool_size));
            } catch (...) {
                LOG_WARNING("Invalid SEARCH_REDIS_POOL_SIZE, using default: " + std::to_string(config.pool_size));
            }
        }
        
        try {
            g_searchClient = std::make_unique<SearchClient>(config);
            LOG_INFO("SearchClient initialized successfully");
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to initialize SearchClient: " + std::string(e.what()));
            throw;
        }
    });
}

// Parse Redis FT.SEARCH response array into structured results
json parseRedisSearchResponse(const std::string& rawResponse, const PaginationParams& pagination) {
    json response;
    response["meta"] = json::object();
    response["results"] = json::array();
    
    try {
        // Parse the raw JSON string from SearchClient
        json redisResponse = json::parse(rawResponse);
        
        if (!redisResponse.is_array() || redisResponse.empty()) {
            // Empty result set
            response["meta"]["total"] = 0;
            response["meta"]["page"] = pagination.page;
            response["meta"]["pageSize"] = pagination.limit;
            return response;
        }
        
        // First element is the total count
        int totalResults = 0;
        if (redisResponse.size() > 0 && redisResponse[0].is_number()) {
            totalResults = redisResponse[0].get<int>();
        }
        
        response["meta"]["total"] = totalResults;
        response["meta"]["page"] = pagination.page;
        response["meta"]["pageSize"] = pagination.limit;
        
        // Parse each result (skip the count, then pairs of docId and fields)
        for (size_t i = 1; i < redisResponse.size(); i += 2) {
            if (i + 1 >= redisResponse.size()) break;
            
            // Document ID at position i
            std::string docId = redisResponse[i].is_string() ? redisResponse[i].get<std::string>() : "";
            
            // Fields array at position i+1
            if (redisResponse[i + 1].is_array()) {
                json fields = redisResponse[i + 1];
                
                json result;
                result["url"] = "";
                result["title"] = "";
                result["snippet"] = "";
                result["score"] = 1.0;
                result["timestamp"] = getCurrentISO8601Timestamp();
                
                // Parse field pairs
                for (size_t j = 0; j < fields.size(); j += 2) {
                    if (j + 1 >= fields.size()) break;
                    
                    std::string fieldName = fields[j].is_string() ? fields[j].get<std::string>() : "";
                    std::string fieldValue = fields[j + 1].is_string() ? fields[j + 1].get<std::string>() : "";
                    
                    if (fieldName == "url") {
                        result["url"] = fieldValue;
                    } else if (fieldName == "title") {
                        result["title"] = fieldValue;
                    } else if (fieldName == "content") {
                        // Create snippet from content (first 200 chars)
                        std::string snippet = fieldValue.substr(0, 200);
                        if (fieldValue.length() > 200) {
                            snippet += "...";
                        }
                        result["snippet"] = snippet;
                    } else if (fieldName == "score") {
                        try {
                            result["score"] = std::stod(fieldValue);
                        } catch (...) {
                            result["score"] = 1.0;
                        }
                    }
                }
                
                response["results"].push_back(result);
            }
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse Redis search response: " + std::string(e.what()));
        // Return empty results on parse error
        response["meta"]["total"] = 0;
        response["meta"]["page"] = pagination.page;
        response["meta"]["pageSize"] = pagination.limit;
    }
    
    return response;
}

PaginationParams parsePaginationParams(const std::map<std::string, std::string>& params) {
    PaginationParams result;
    
    // Parse page parameter
    auto pageIt = params.find("page");
    if (pageIt != params.end()) {
        try {
            result.page = std::stoi(pageIt->second);
            if (result.page < 1 || result.page > 1000) {
                result.valid = false;
                result.error = "Page must be between 1 and 1000";
                return result;
            }
        } catch (const std::exception& e) {
            result.valid = false;
            result.error = "Invalid page parameter: must be an integer";
            return result;
        }
    }
    
    // Parse limit parameter
    auto limitIt = params.find("limit");
    if (limitIt != params.end()) {
        try {
            result.limit = std::stoi(limitIt->second);
            if (result.limit < 1 || result.limit > 100) {
                result.valid = false;
                result.error = "Limit must be between 1 and 100";
                return result;
            }
        } catch (const std::exception& e) {
            result.valid = false;
            result.error = "Invalid limit parameter: must be an integer";
            return result;
        }
    }
    
    return result;
}

void handleSearch(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    // Start timing from the very beginning of the request
    auto requestStartTime = std::chrono::high_resolution_clock::now();
    LOG_INFO("Handling search request");
    
    // Initialize SearchClient if not already done
    try {
        initializeSearchClient();
    } catch (const std::exception& e) {
        json errorResponse = {
            {"error", {
                {"code", "INTERNAL_ERROR"},
                {"message", "Search service unavailable"},
                {"details", {
                    {"error", e.what()}
                }}
            }}
        };
        
        res->writeStatus("503 Service Unavailable")
           ->writeHeader("Content-Type", "application/json")
           ->end(errorResponse.dump());
        return;
    }
    
    // Parse query string
    std::string queryString = std::string(req->getQuery());
    auto params = api::parseQueryString(queryString);
    
    // Check for required 'q' parameter
    auto qIt = params.find("q");
    if (qIt == params.end() || qIt->second.empty()) {
        // Return 400 error for missing 'q' parameter
        json errorResponse = {
            {"error", {
                {"code", "INVALID_REQUEST"},
                {"message", "Invalid request parameters"},
                {"details", {
                    {"q", "Query parameter is required"}
                }}
            }}
        };
        
        res->writeStatus("400 Bad Request")
           ->writeHeader("Content-Type", "application/json")
           ->end(errorResponse.dump());
        
        LOG_WARNING("Search request rejected: missing 'q' parameter");
        return;
    }
    
    // Parse pagination parameters
    auto paginationParams = parsePaginationParams(params);
    if (!paginationParams.valid) {
        // Return 400 error for invalid pagination parameters
        json errorResponse = {
            {"error", {
                {"code", "INVALID_REQUEST"},
                {"message", "Invalid request parameters"},
                {"details", {
                    {"page", paginationParams.error}
                }}
            }}
        };
        
        res->writeStatus("400 Bad Request")
           ->writeHeader("Content-Type", "application/json")
           ->end(errorResponse.dump());
        
        LOG_WARNING("Search request rejected: " + paginationParams.error);
        return;
    }
    
    // Parse domain_filter if present
    std::vector<std::string> domainFilter;
    auto domainIt = params.find("domain_filter");
    if (domainIt != params.end() && !domainIt->second.empty()) {
        std::stringstream ss(domainIt->second);
        std::string domain;
        while (std::getline(ss, domain, ',')) {
            domainFilter.push_back(domain);
        }
    }
    
    try {
        // Get index name from environment or use default
        const char* indexName = std::getenv("SEARCH_INDEX_NAME");
        std::string searchIndex = indexName ? indexName : "search_index";
        
        // Build search arguments
        std::vector<std::string> searchArgs;
        
        // Add LIMIT for pagination
        int offset = (paginationParams.page - 1) * paginationParams.limit;
        searchArgs.push_back("LIMIT");
        searchArgs.push_back(std::to_string(offset));
        searchArgs.push_back(std::to_string(paginationParams.limit));
        
        // Add domain filter if specified
        // RedisSearch FILTER doesn't support OR logic directly
        // For now, we'll skip domain filtering if the index doesn't have the field
        // This prevents errors when the field doesn't exist
        if (!domainFilter.empty()) {
            // TODO: Implement proper domain filtering when index schema is known
            LOG_WARNING("Domain filtering requested but may not be supported by current index schema");
        }
        
        // Add RETURN to specify which fields to return
        searchArgs.push_back("RETURN");
        searchArgs.push_back("4");  // Number of fields
        searchArgs.push_back("url");
        searchArgs.push_back("title");
        searchArgs.push_back("content");
        searchArgs.push_back("score");
        
        // Execute search (URL decode the query first)
        std::string decodedQuery = urlDecode(qIt->second);
        std::string rawResult = g_searchClient->search(searchIndex, decodedQuery, searchArgs);
        
        // Parse and format response
        json response = parseRedisSearchResponse(rawResult, paginationParams);
        
        // Calculate total request time from start to finish
        auto requestEndTime = std::chrono::high_resolution_clock::now();
        auto totalDuration = std::chrono::duration_cast<std::chrono::microseconds>(requestEndTime - requestStartTime);
        double totalSeconds = totalDuration.count() / 1000000.0;
        
        // Add timing information to response
        response["meta"]["queryTime"] = totalSeconds;
        response["meta"]["queryTimeMs"] = totalDuration.count() / 1000.0;
        
        // Log successful request with timing
        LOG_INFO("Search request successful: q=" + qIt->second + 
                 ", page=" + std::to_string(paginationParams.page) + 
                 ", limit=" + std::to_string(paginationParams.limit) +
                 ", domains=" + std::to_string(domainFilter.size()) +
                 ", totalTime=" + std::to_string(totalSeconds) + "s");
        
        // Return 200 OK with the response
        res->writeStatus("200 OK")
           ->writeHeader("Content-Type", "application/json")
           ->writeHeader("Access-Control-Allow-Origin", "*")  // CORS support
           ->writeHeader("Access-Control-Allow-Methods", "GET, OPTIONS")
           ->writeHeader("Access-Control-Allow-Headers", "Content-Type")
           ->end(response.dump());
           
    } catch (const SearchError& e) {
        LOG_ERROR("Search error: " + std::string(e.what()));
        
        // Check if it's a non-existent index error
        std::string errorMsg = e.what();
        if (errorMsg.find("no such index") != std::string::npos || 
            errorMsg.find("Unknown Index") != std::string::npos) {
            // Return empty results for non-existent index
            json response;
            response["meta"]["total"] = 0;
            response["meta"]["page"] = paginationParams.page;
            response["meta"]["pageSize"] = paginationParams.limit;
            response["results"] = json::array();
            
            res->writeStatus("200 OK")
               ->writeHeader("Content-Type", "application/json")
               ->writeHeader("Access-Control-Allow-Origin", "*")
               ->end(response.dump());
        } else {
            // Other search errors
            json errorResponse = {
                {"error", {
                    {"code", "SEARCH_ERROR"},
                    {"message", "Search operation failed"},
                    {"details", {
                        {"error", e.what()}
                    }}
                }}
            };
            
            res->writeStatus("500 Internal Server Error")
               ->writeHeader("Content-Type", "application/json")
               ->end(errorResponse.dump());
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Unexpected error: " + std::string(e.what()));
        
        json errorResponse = {
            {"error", {
                {"code", "INTERNAL_ERROR"},
                {"message", "An unexpected error occurred"},
                {"details", {
                    {"error", e.what()}
                }}
            }}
        };
        
        res->writeStatus("500 Internal Server Error")
           ->writeHeader("Content-Type", "application/json")
           ->end(errorResponse.dump());
    }
}

} // namespace search_api 