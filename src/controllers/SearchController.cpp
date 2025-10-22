#include "SearchController.h"
#include "../../include/Logger.h"
#include "../../include/search_engine/crawler/Crawler.h"
#include "../../include/search_engine/crawler/CrawlerManager.h"
#include "../../include/search_engine/crawler/PageFetcher.h"
#include "../../include/search_engine/crawler/models/CrawlConfig.h"
#include "../../include/search_engine/storage/ContentStorage.h"
#include "../../include/search_engine/storage/MongoDBStorage.h"
#include "../../include/search_engine/storage/ApiRequestLog.h"
#include "../../include/search_engine/storage/EmailService.h"
#include "../../include/search_engine/storage/EmailLogsStorage.h"
#include "../../include/inja/inja.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <chrono>
#include <iomanip>
#include <map>
#include <mutex>
#include <numeric>
#include <regex>
#include <algorithm>
#include <sstream>
#include <string>
#include <cctype>
#include <thread>

using namespace hatef::search;

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

// Helper function to truncate text to a maximum length
std::string truncateDescription(const std::string& text, size_t maxLength = 300) {
    if (text.length() <= maxLength) {
        return text;
    }

    // Find the last space within the limit to avoid cutting words
    size_t truncatePos = maxLength;
    while (truncatePos > maxLength * 0.8 && truncatePos > 0) {
        if (text[truncatePos] == ' ' || text[truncatePos] == '\n' || text[truncatePos] == '\t') {
            break;
        }
        truncatePos--;
    }

    // If no suitable break point found, use the max length
    if (truncatePos <= maxLength * 0.8) {
        truncatePos = maxLength;
    }

    return text.substr(0, truncatePos) + "...";
}

// Static SearchClient instance
static std::unique_ptr<SearchClient> g_searchClient;
static std::once_flag g_initFlag;

// Static CrawlerManager instance
static std::unique_ptr<CrawlerManager> g_crawlerManager;
static std::once_flag g_crawlerManagerInitFlag;

// Static MongoDBStorage instance for search operations
static std::unique_ptr<search_engine::storage::MongoDBStorage> g_mongoStorage;
static std::once_flag g_mongoStorageInitFlag;

SearchController::SearchController() {
    // Initialize SearchClient once
    std::call_once(g_initFlag, []() {
        RedisConfig config;
        
        const char* redisUri = std::getenv("SEARCH_REDIS_URI");
        if (redisUri) {
            config.uri = redisUri;
            LOG_INFO("Using Redis URI from environment: " + config.uri);
        } else {
            LOG_INFO("Using default Redis URI: " + config.uri);
        }
        
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
    
    // Initialize CrawlerManager once
    std::call_once(g_crawlerManagerInitFlag, []() {
        try {
            // Get MongoDB connection string from environment or use default
            const char* mongoUri = std::getenv("MONGODB_URI");
            std::string mongoConnectionString = mongoUri ? mongoUri : "mongodb://localhost:27017";
            
            LOG_INFO("Using MongoDB connection string: " + mongoConnectionString);
            
            // Get Redis connection string from environment or use default
            const char* redisUri = std::getenv("SEARCH_REDIS_URI");
            std::string redisConnectionString = redisUri ? redisUri : "tcp://127.0.0.1:6379";
            
            LOG_INFO("Using Redis connection string: " + redisConnectionString);
            
            // Create ContentStorage for database persistence
            auto storage = std::make_shared<search_engine::storage::ContentStorage>(
                mongoConnectionString,
                "search-engine",
                redisConnectionString,
                "search_index"
            );
            
            // Initialize crawler manager with database storage
            g_crawlerManager = std::make_unique<CrawlerManager>(storage);

            LOG_INFO("CrawlerManager initialized successfully with database storage");
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to initialize CrawlerManager: " + std::string(e.what()));
            throw;
        }
    });
    
    // Initialize MongoDBStorage once
    std::call_once(g_mongoStorageInitFlag, []() {
        try {
            // Get MongoDB connection string from environment or use default
            const char* mongoUri = std::getenv("MONGODB_URI");
            std::string mongoConnectionString = mongoUri ? mongoUri : "mongodb://admin:password123@mongodb:27017";
            
            LOG_INFO("Initializing MongoDBStorage for search with connection: " + mongoConnectionString);
            
            // Create MongoDBStorage for search operations
            g_mongoStorage = std::make_unique<search_engine::storage::MongoDBStorage>(
                mongoConnectionString,
                "search-engine"
            );
            
            LOG_INFO("MongoDBStorage for search initialized successfully");
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to initialize MongoDBStorage for search: " + std::string(e.what()));
            throw;
        }
    });
}

void SearchController::addSiteToCrawl(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    LOG_INFO("SearchController::addSiteToCrawl called");
    
    // Start timing for response time tracking
    auto requestStartTime = std::chrono::system_clock::now();
    
    // Get IP address and user agent for logging
    std::string ipAddress = std::string(req->getHeader("x-forwarded-for"));
    if (ipAddress.empty()) {
        ipAddress = std::string(req->getHeader("x-real-ip"));
    }
    if (ipAddress.empty()) {
        ipAddress = "unknown";
    }
    
    std::string userAgent = std::string(req->getHeader("user-agent"));
    if (userAgent.empty()) {
        userAgent = "unknown";
    }
    
    LOG_INFO("IP Address: " + ipAddress + ", User Agent: " + userAgent);
    
    // Read the request body
    std::string buffer;
    res->onData([this, res, req, buffer = std::move(buffer), requestStartTime, ipAddress, userAgent](std::string_view data, bool last) mutable {
        buffer.append(data.data(), data.length());
        
        LOG_INFO("addSiteToCrawl: Received data chunk, length: " + std::to_string(data.length()) + ", last: " + (last ? "true" : "false") + ", buffer size: " + std::to_string(buffer.size()));
        
        if (last) {
            try {
                // Parse JSON body
                auto jsonBody = nlohmann::json::parse(buffer);
                
                // Validate required fields
                if (!jsonBody.contains("url") || jsonBody["url"].empty()) {
                    badRequest(res, "URL is required");
                    return;
                }
                
                std::string url = jsonBody["url"];
                
                // Optional parameters
                std::string email = jsonBody.value("email", "");  // Email for completion notification
                std::string recipientName = jsonBody.value("recipientName", "");  // Recipient name for email (default: email prefix)
                std::string language = jsonBody.value("language", "en");  // Language for email notification (default: English)
                int maxPages = jsonBody.value("maxPages", 1000);
                int maxDepth = jsonBody.value("maxDepth", 3);
                bool restrictToSeedDomain = jsonBody.value("restrictToSeedDomain", true);
                bool followRedirects = jsonBody.value("followRedirects", true);  // Default to true for cookie handling
                int maxRedirects = jsonBody.value("maxRedirects", 10);  // Increase default to handle cookie redirects
                bool force = jsonBody.value("force", true);  // Default to true for re-crawling
                bool extractTextContent = jsonBody.value("extractTextContent", true);  // Default to true for text extraction
                bool spaRenderingEnabled = jsonBody.value("spaRenderingEnabled", false);  // Default to disabled
                bool includeFullContent = jsonBody.value("includeFullContent", false);
                // Get default timeout from environment variable or use 90000ms
                int defaultTimeoutMs = 90000;
                const char* envTimeout = std::getenv("DEFAULT_REQUEST_TIMEOUT");
                if (envTimeout) {
                    try {
                        defaultTimeoutMs = std::stoi(envTimeout);
                        LOG_INFO("Using DEFAULT_REQUEST_TIMEOUT from environment: " + std::to_string(defaultTimeoutMs) + "ms");
                    } catch (...) {
                        LOG_WARNING("Invalid DEFAULT_REQUEST_TIMEOUT, using default: " + std::to_string(defaultTimeoutMs) + "ms");
                    }
                }
                
                int requestTimeoutMs = jsonBody.value("requestTimeout", defaultTimeoutMs); // allow overriding request timeout
                bool stopPreviousSessions = jsonBody.value("stopPreviousSessions", false);  // Default to false for concurrent crawling
                std::string browserlessUrl = jsonBody.value("browserlessUrl", "http://browserless:3000");
                
                // Validate parameters
                if (maxPages < 1 || maxPages > 10000) {
                    badRequest(res, "maxPages must be between 1 and 10000");
                    return;
                }
                
                if (maxDepth < 1 || maxDepth > 10) {
                    badRequest(res, "maxDepth must be between 1 and 10");
                    return;
                }
                
                if (maxRedirects < 0 || maxRedirects > 20) {
                    badRequest(res, "maxRedirects must be between 0 and 20");
                    return;
                }
                
                // Validate email if provided
                if (!email.empty()) {
                    // Simple email validation
                    if (email.find('@') == std::string::npos || email.find('.') == std::string::npos) {
                        badRequest(res, "Invalid email format");
                        return;
                    }
                }
                
                // Start new crawl session
                if (g_crawlerManager) {
                    // Stop previous sessions if requested
                    if (stopPreviousSessions) {
                        auto activeSessions = g_crawlerManager->getActiveSessions();
                        LOG_INFO("Stopping " + std::to_string(activeSessions.size()) + " active sessions before starting new crawl");
                        for (const auto& activeSessionId : activeSessions) {
                            g_crawlerManager->stopCrawl(activeSessionId);
                        }
                    }
                    
                    // Create crawler configuration
                    CrawlConfig config;
                    config.maxPages = maxPages;
                    config.maxDepth = maxDepth;
                    config.userAgent = "Hatefbot/1.0";
                    config.requestTimeout = std::chrono::milliseconds(requestTimeoutMs);
                    
                    // Override with environment variables if set
                    const char* envRequestTimeout = std::getenv("DEFAULT_REQUEST_TIMEOUT");
                    if (envRequestTimeout) {
                        try {
                            int envTimeout = std::stoi(envRequestTimeout);
                            config.requestTimeout = std::chrono::milliseconds(envTimeout);
                            LOG_INFO("Overriding requestTimeout with DEFAULT_REQUEST_TIMEOUT from environment: " + std::to_string(envTimeout) + "ms");
                        } catch (...) {
                            LOG_WARNING("Invalid DEFAULT_REQUEST_TIMEOUT, keeping API timeout: " + std::to_string(requestTimeoutMs) + "ms");
                        }
                    }
                    config.extractTextContent = extractTextContent;
                    config.restrictToSeedDomain = restrictToSeedDomain;
                    config.followRedirects = followRedirects;
                    config.maxRedirects = maxRedirects;
                    config.spaRenderingEnabled = spaRenderingEnabled;
                    config.includeFullContent = includeFullContent;
                    config.browserlessUrl = browserlessUrl;
                    
                    // Create completion callback for email notification if email is provided
                    CrawlCompletionCallback emailCallback = nullptr;
                    if (!email.empty()) {
                        LOG_INFO("Setting up email notification callback for: " + email + " (language: " + language + ", recipientName: " + recipientName + ")");
                        emailCallback = [this, email, url, language, recipientName](const std::string& sessionId, 
                                                         const std::vector<CrawlResult>& results, 
                                                         CrawlerManager* manager) {
                            this->sendCrawlCompletionEmail(sessionId, email, url, results, language, recipientName);
                        };
                    }
                    
                    // Start new crawl session with completion callback
                    std::string sessionId = g_crawlerManager->startCrawl(url, config, force, emailCallback);
                    
                    LOG_INFO("Started new crawl session: " + sessionId + " for URL: " + url + 
                             " (maxPages: " + std::to_string(maxPages) + 
                             ", maxDepth: " + std::to_string(maxDepth) + 
                             ", restrictToSeedDomain: " + (restrictToSeedDomain ? "true" : "false") + 
                             ", followRedirects: " + (followRedirects ? "true" : "false") + 
                             ", maxRedirects: " + std::to_string(maxRedirects) + 
                             ", force: " + (force ? "true" : "false") + 
                             ", extractTextContent: " + (extractTextContent ? "true" : "false") + 
                             ", spaRenderingEnabled: " + (spaRenderingEnabled ? "true" : "false") + 
                             ", includeFullContent: " + (includeFullContent ? "true" : "false") + 
                             ", stopPreviousSessions: " + (stopPreviousSessions ? "true" : "false") + ")");
                    
                    // Return success response with session ID
                    nlohmann::json response = {
                        {"success", true},
                        {"message", "Crawl session started successfully"},
                        {"data", {
                            {"sessionId", sessionId},
                            {"url", url},
                            {"maxPages", maxPages},
                            {"maxDepth", maxDepth},
                            {"restrictToSeedDomain", restrictToSeedDomain},
                            {"followRedirects", followRedirects},
                            {"maxRedirects", maxRedirects},
                            {"force", force},
                            {"extractTextContent", extractTextContent},
                            {"spaRenderingEnabled", spaRenderingEnabled},
                            {"includeFullContent", includeFullContent},
                            {"stopPreviousSessions", stopPreviousSessions},
                            {"browserlessUrl", browserlessUrl},
                            {"status", "starting"}
                        }}
                    };
                    
                    json(res, response);
                    
                    // Log API request to database asynchronously to avoid blocking the response
                    std::thread([this, ipAddress, userAgent, requestStartTime, buffer, sessionId]() {
                        try {
                            LOG_INFO("Starting API request logging...");
                            
                            // Calculate response time
                            auto responseEndTime = std::chrono::system_clock::now();
                            auto responseTime = std::chrono::duration_cast<std::chrono::milliseconds>(responseEndTime - requestStartTime);
                            
                            // Create API request log
                            search_engine::storage::ApiRequestLog apiLog;
                            apiLog.endpoint = "/api/crawl/add-site";
                            apiLog.method = "POST";
                            apiLog.ipAddress = ipAddress;
                            apiLog.userAgent = userAgent;
                            apiLog.createdAt = std::chrono::system_clock::now();
                            apiLog.requestBody = buffer;
                            apiLog.sessionId = sessionId;
                            apiLog.status = "success";
                            apiLog.responseTimeMs = static_cast<int>(responseTime.count());
                            
                            LOG_INFO("API request log created - endpoint: " + apiLog.endpoint + ", IP: " + apiLog.ipAddress + ", sessionId: " + sessionId);
                            
                            // Store in database if we have access to storage
                            if (g_crawlerManager) {
                                LOG_INFO("CrawlerManager is available");
                                if (g_crawlerManager->getStorage()) {
                                    LOG_INFO("Storage is available, storing API request log...");
                                    auto result = g_crawlerManager->getStorage()->storeApiRequestLog(apiLog);
                                    if (result.success) {
                                        LOG_INFO("API request logged successfully with ID: " + result.value);
                                    } else {
                                        LOG_WARNING("Failed to log API request: " + result.message);
                                    }
                                } else {
                                    LOG_WARNING("Storage is not available from CrawlerManager");
                                }
                            } else {
                                LOG_WARNING("CrawlerManager is not available");
                            }
                        } catch (const std::exception& e) {
                            LOG_WARNING("Failed to log API request: " + std::string(e.what()));
                        }
                    }).detach(); // Detach the thread to avoid blocking
                } else {
                    serverError(res, "CrawlerManager not initialized");
                }
                
            } catch (const nlohmann::json::parse_error& e) {
                LOG_ERROR("Failed to parse JSON: " + std::string(e.what()));
                
                // Log API request error to database
                logApiRequestError("/api/crawl/add-site", "POST", ipAddress, userAgent, requestStartTime, 
                                 buffer, "Invalid JSON format", std::string(e.what()));
                
                badRequest(res, "Invalid JSON format");
            } catch (const std::runtime_error& e) {
                std::string errorMessage = std::string(e.what());
                LOG_ERROR("Runtime error in addSiteToCrawl: " + errorMessage);
                
                // Check if this is a session limit error
                if (errorMessage.find("Maximum concurrent sessions limit reached") != std::string::npos) {
                    // Return a specific error for session limit
                    nlohmann::json errorResponse = {
                        {"error", {
                            {"code", "TOO_MANY_REQUESTS"},
                            {"message", "Server is currently busy processing other crawl requests. Please try again in a few moments."},
                            {"details", "Maximum concurrent crawl sessions limit reached. Please wait for current crawls to complete."}
                        }},
                        {"success", false}
                    };
                    
                    // Log API request error to database
                    logApiRequestError("/api/crawl/add-site", "POST", ipAddress, userAgent, requestStartTime, 
                                     buffer, "TOO_MANY_REQUESTS", errorMessage);
                    
                    res->writeStatus("429 Too Many Requests");
                    res->writeHeader("Content-Type", "application/json");
                    res->writeHeader("Retry-After", "30"); // Suggest retry after 30 seconds
                    res->end(errorResponse.dump());
                } else {
                    // Other runtime errors
                    logApiRequestError("/api/crawl/add-site", "POST", ipAddress, userAgent, requestStartTime, 
                                     buffer, "Runtime error", errorMessage);
                    serverError(res, "A runtime error occurred: " + errorMessage);
                }
            } catch (const std::exception& e) {
                LOG_ERROR("Unexpected error in addSiteToCrawl: " + std::string(e.what()));
                
                // Log API request error to database
                logApiRequestError("/api/crawl/add-site", "POST", ipAddress, userAgent, requestStartTime, 
                                 buffer, "Unexpected error", std::string(e.what()));
                
                serverError(res, "An unexpected error occurred");
            }
        }
    });
    
    res->onAborted([]() {
        LOG_WARNING("Add site to crawl request aborted");
    });
}

void SearchController::search(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    // Start timing from the very beginning of the request
    auto requestStartTime = std::chrono::high_resolution_clock::now();
    LOG_INFO("SearchController::search called");
    
    // Parse query parameters
    auto params = parseQuery(req);
    
    // Check for required 'q' parameter
    auto qIt = params.find("q");
    if (qIt == params.end() || qIt->second.empty()) {
        nlohmann::json error = {
            {"error", {
                {"code", "INVALID_REQUEST"},
                {"message", "Invalid request parameters"},
                {"details", {
                    {"q", "Query parameter is required"}
                }}
            }}
        };
        
        json(res, error, "400 Bad Request");
        LOG_WARNING("Search request rejected: missing 'q' parameter");
        return;
    }
    
    // Parse pagination
    int page = 1;
    int limit = 10;
    
    auto pageIt = params.find("page");
    if (pageIt != params.end()) {
        try {
            page = std::stoi(pageIt->second);
            if (page < 1 || page > 1000) {
                badRequest(res, "Page must be between 1 and 1000");
                return;
            }
        } catch (...) {
            badRequest(res, "Invalid page parameter");
            return;
        }
    }
    
    auto limitIt = params.find("limit");
    if (limitIt != params.end()) {
        try {
            limit = std::stoi(limitIt->second);
            if (limit < 1 || limit > 100) {
                badRequest(res, "Limit must be between 1 and 100");
                return;
            }
        } catch (...) {
            badRequest(res, "Invalid limit parameter");
            return;
        }
    }
    
    try {
        // Get index name from environment or use default
        const char* indexName = std::getenv("SEARCH_INDEX_NAME");
        std::string searchIndex = indexName ? indexName : "search_index";
        
        // Build search arguments
        std::vector<std::string> searchArgs;
        
        // Add LIMIT for pagination
        int offset = (page - 1) * limit;
        searchArgs.push_back("LIMIT");
        searchArgs.push_back(std::to_string(offset));
        searchArgs.push_back(std::to_string(limit));
        
        // Add RETURN to specify which fields to return
        searchArgs.push_back("RETURN");
        searchArgs.push_back("4");
        searchArgs.push_back("url");
        searchArgs.push_back("title");
        searchArgs.push_back("content");
        searchArgs.push_back("score");
        
        // Execute search (URL decode the query first)
        std::string decodedQuery = urlDecode(qIt->second);
        std::string rawResult = g_searchClient->search(searchIndex, decodedQuery, searchArgs);
        
        // Parse and format response
        nlohmann::json response = parseRedisSearchResponse(rawResult, page, limit);
        
        // Calculate total request time from start to finish
        auto requestEndTime = std::chrono::high_resolution_clock::now();
        auto totalDuration = std::chrono::duration_cast<std::chrono::microseconds>(requestEndTime - requestStartTime);
        double totalSeconds = totalDuration.count() / 1000000.0;
        
        // Add timing information to response
        response["meta"]["queryTime"] = totalSeconds;
        response["meta"]["queryTimeMs"] = totalDuration.count() / 1000.0;
        
        LOG_INFO("Search request successful: q=" + qIt->second + 
                 ", page=" + std::to_string(page) + 
                 ", limit=" + std::to_string(limit) +
                 ", totalTime=" + std::to_string(totalSeconds) + "s");
        
        json(res, response);
        
    } catch (const SearchError& e) {
        LOG_ERROR("Search error: " + std::string(e.what()));
        
        std::string errorMsg = e.what();
        if (errorMsg.find("no such index") != std::string::npos || 
            errorMsg.find("Unknown Index") != std::string::npos) {
            // Return empty results for non-existent index
            nlohmann::json response;
            response["meta"]["total"] = 0;
            response["meta"]["page"] = page;
            response["meta"]["pageSize"] = limit;
            response["results"] = nlohmann::json::array();
            
            json(res, response);
        } else {
            serverError(res, "Search operation failed");
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Unexpected error: " + std::string(e.what()));
        serverError(res, "An unexpected error occurred");
    }
}

void SearchController::getCrawlStatus(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    LOG_INFO("SearchController::getCrawlStatus called");
    
    try {
        // Parse query parameters
        auto params = parseQuery(req);
        
        // Get sessionId parameter
        auto sessionIdIt = params.find("sessionId");
        
        // Get optional parameters
        bool includeResults = params.find("results") != params.end() && params["results"] == "true";
        int maxResults = 50; // Default limit
        
        auto maxResultsIt = params.find("maxResults");
        if (maxResultsIt != params.end()) {
            try {
                maxResults = std::stoi(maxResultsIt->second);
                if (maxResults < 1 || maxResults > 1000) {
                    maxResults = 50; // Reset to default if invalid
                }
            } catch (...) {
                maxResults = 50; // Reset to default if parsing fails
            }
        }
        
        nlohmann::json response;
        
        if (g_crawlerManager) {
            if (sessionIdIt != params.end()) {
                // Get status for specific session
                std::string sessionId = sessionIdIt->second;
                std::string status = g_crawlerManager->getCrawlStatus(sessionId);
                
                if (status == "not_found") {
                    badRequest(res, "Session not found");
                    return;
                }
                
                response["sessionId"] = sessionId;
                response["status"] = status;
                
                if (includeResults) {
                    auto results = g_crawlerManager->getCrawlResults(sessionId);
                    nlohmann::json resultsArray = nlohmann::json::array();
                    
                    // Get the most recent results (up to maxResults)
                    int startIndex = std::max(0, static_cast<int>(results.size()) - maxResults);
                    for (size_t i = startIndex; i < results.size(); ++i) {
                        const auto& result = results[i];
                        
                        nlohmann::json resultJson = {
                            {"url", result.url},
                            {"statusCode", result.statusCode},
                            {"status", result.success ? "success" : "failed"},
                            {"crawlTime", std::chrono::duration_cast<std::chrono::seconds>(
                                result.crawlTime.time_since_epoch()).count()},
                            {"contentSize", static_cast<int>(result.contentSize)},
                            {"linksFound", static_cast<int>(result.links.size())}
                        };
                        
                        if (result.title.has_value()) {
                            resultJson["title"] = result.title.value();
                        }
                        
                        if (!result.success && result.errorMessage.has_value()) {
                            resultJson["error"] = result.errorMessage.value();
                        }
                        
                        resultsArray.push_back(resultJson);
                    }
                    
                    response["results"] = resultsArray;
                    response["totalCrawled"] = static_cast<int>(results.size());
                }
            } else {
                // Get status for all active sessions
                auto activeSessions = g_crawlerManager->getActiveSessions();
                
                response["activeSessions"] = activeSessions.size();
                response["lastUpdate"] = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                
                nlohmann::json sessionsArray = nlohmann::json::array();
                for (const auto& sessionId : activeSessions) {
                    nlohmann::json sessionJson = {
                        {"sessionId", sessionId},
                        {"status", g_crawlerManager->getCrawlStatus(sessionId)}
                    };
                    
                    if (includeResults) {
                        auto results = g_crawlerManager->getCrawlResults(sessionId);
                        sessionJson["totalCrawled"] = static_cast<int>(results.size());
                    }
                    
                    sessionsArray.push_back(sessionJson);
                }
                
                response["sessions"] = sessionsArray;
            }
        } else {
            serverError(res, "CrawlerManager not initialized");
            return;
        }
        
        json(res, response);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error in getCrawlStatus: " + std::string(e.what()));
        serverError(res, "Failed to get crawl status");
    }
}

void SearchController::getCrawlDetails(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    LOG_INFO("SearchController::getCrawlDetails called");
    // Parse query parameters
    auto params = parseQuery(req);
    std::string domainFilter;
    std::string urlFilter;
    auto it = params.find("domain");
    if (it != params.end()) {
        domainFilter = it->second;
    }
    auto itUrl = params.find("url");
    if (itUrl != params.end()) {
        urlFilter = itUrl->second;
    }

    nlohmann::json response;
    // Use the singleton ContentStorage from crawler manager to prevent connection pool exhaustion
    std::shared_ptr<search_engine::storage::ContentStorage> storage;
    
    if (g_crawlerManager) {
        // Use the existing singleton storage from crawler manager
        storage = g_crawlerManager->getStorage();
        LOG_DEBUG("Using singleton ContentStorage from crawler manager");
    } else {
        LOG_ERROR("CrawlerManager not initialized - cannot access storage");
        serverError(res, "Crawler service not available");
        return;
    }

    try {
        if (!urlFilter.empty()) {
            // Fetch logs for a specific URL
            auto logsResult = storage->getCrawlLogsByUrl(urlFilter, 100, 0);
            if (!logsResult.success) {
                serverError(res, logsResult.message);
                return;
            }
            nlohmann::json logsJson = nlohmann::json::array();
            for (const auto& log : logsResult.value) {
                nlohmann::json logJson = {
                    {"id", log.id.value_or("")},
                    {"url", log.url},
                    {"domain", log.domain},
                    {"crawlTime", std::chrono::duration_cast<std::chrono::seconds>(log.crawlTime.time_since_epoch()).count()},
                    {"status", log.status},
                    {"httpStatusCode", log.httpStatusCode},
                    {"contentSize", log.contentSize},
                    {"contentType", log.contentType},
                    {"links", log.links},
                };
                if (log.errorMessage) logJson["errorMessage"] = *log.errorMessage;
                if (log.title) logJson["title"] = *log.title;
                if (log.description) logJson["description"] = *log.description;
                if (log.downloadTimeMs) logJson["downloadTimeMs"] = *log.downloadTimeMs;
                logsJson.push_back(logJson);
            }
            response["url"] = urlFilter;
            response["logs"] = logsJson;
        } else if (!domainFilter.empty()) {
            // Fetch logs for a domain
            auto logsResult = storage->getCrawlLogsByDomain(domainFilter, 100, 0);
            if (!logsResult.success) {
                serverError(res, logsResult.message);
                return;
            }
            nlohmann::json logsJson = nlohmann::json::array();
            for (const auto& log : logsResult.value) {
                nlohmann::json logJson = {
                    {"id", log.id.value_or("")},
                    {"url", log.url},
                    {"domain", log.domain},
                    {"crawlTime", std::chrono::duration_cast<std::chrono::seconds>(log.crawlTime.time_since_epoch()).count()},
                    {"status", log.status},
                    {"httpStatusCode", log.httpStatusCode},
                    {"contentSize", log.contentSize},
                    {"contentType", log.contentType},
                    {"links", log.links},
                };
                if (log.errorMessage) logJson["errorMessage"] = *log.errorMessage;
                if (log.title) logJson["title"] = *log.title;
                if (log.description) logJson["description"] = *log.description;
                if (log.downloadTimeMs) logJson["downloadTimeMs"] = *log.downloadTimeMs;
                logsJson.push_back(logJson);
            }
            response["domain"] = domainFilter;
            response["logs"] = logsJson;
        } else {
            // No filter: return a summary (list of domains with log counts)
            // For simplicity, not implemented here. You can extend this to aggregate domains from crawl_logs.
            response["message"] = "Please provide a 'domain' or 'url' query parameter to fetch crawl details.";
        }
        json(res, response);
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Error in getCrawlDetails: ") + e.what());
        serverError(res, "Failed to get crawl details");
    }
}

void SearchController::detectSpa(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    LOG_INFO("SearchController::detectSpa called");
    
    // Read the request body
    std::string buffer;
    res->onData([this, res, buffer = std::move(buffer)](std::string_view data, bool last) mutable {
        buffer.append(data.data(), data.length());
        
        if (last) {
            try {
                // Parse JSON body
                auto jsonBody = nlohmann::json::parse(buffer);
                
                // Validate required fields
                if (!jsonBody.contains("url") || jsonBody["url"].empty()) {
                    badRequest(res, "URL is required");
                    return;
                }
                
                std::string url = jsonBody["url"];
                
                // Optional parameters
                int timeout = jsonBody.value("timeout", 30000); // 30 seconds default
                std::string userAgent = jsonBody.value("userAgent", "Hatefbot/1.0");
                
                // Validate parameters
                if (timeout < 1000 || timeout > 120000) {
                    badRequest(res, "timeout must be between 1000 and 120000 milliseconds");
                    return;
                }
                
                LOG_INFO("Detecting SPA for URL: " + url);
                
                // Create a temporary PageFetcher for SPA detection
                PageFetcher fetcher(
                    userAgent,
                    std::chrono::milliseconds(timeout),
                    true,  // follow redirects
                    5      // max redirects
                );
                
                // Fetch the page
                auto startTime = std::chrono::steady_clock::now();
                auto result = fetcher.fetch(url);
                auto endTime = std::chrono::steady_clock::now();
                auto fetchDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
                
                // Prepare response
                nlohmann::json response = {
                    {"success", result.success},
                    {"url", url},
                    {"fetchDuration", fetchDuration.count()},
                    {"httpStatusCode", result.statusCode},
                    {"contentType", result.contentType},
                    {"contentSize", result.content.size()},
                    {"spaDetection", {
                        {"isSpa", false},
                        {"indicators", nlohmann::json::array()},
                        {"confidence", 0.0}
                    }}
                };
                
                if (!result.success) {
                    response["error"] = result.errorMessage;
                    json(res, response);
                    return;
                }
                
                // Detect SPA
                bool isSpa = fetcher.isSpaPage(result.content, url);
                response["spaDetection"]["isSpa"] = isSpa;
                
                // Analyze SPA indicators
                std::vector<std::string> indicators;
                
                // Only analyze indicators if the main SPA detection thinks it's a SPA
                if (!isSpa) {
                    // If PageFetcher doesn't think it's a SPA, don't show any indicators
                    response["spaDetection"]["indicators"] = nlohmann::json::array();
                    response["spaDetection"]["confidence"] = 0.0;
                } else {
                    // Framework detection with case-sensitive word boundary matching
                    std::vector<std::pair<std::string, std::string>> frameworkPatterns = {
                        {"\\bReact\\b", "React"},
                        {"\\breact\\b", "React"},
                        {"\\bVue\\b", "Vue.js"},
                        {"\\bvue\\b", "Vue.js"},
                        {"\\bAngular\\b", "Angular"},
                        {"\\bangular\\b", "Angular"},
                        {"\\bEmber\\b", "Ember"},
                        {"\\bember\\b", "Ember"},
                        {"\\bBackbone\\b", "Backbone"},
                        {"\\bbackbone\\b", "Backbone"},
                        {"\\bSvelte\\b", "Svelte"},
                        {"\\bsvelte\\b", "Svelte"}
                    };
                    
                    // Check framework patterns with regex word boundaries
                    for (const auto& [pattern, name] : frameworkPatterns) {
                        std::regex frameworkRegex(pattern);
                        if (std::regex_search(result.content, frameworkRegex)) {
                            indicators.push_back(name);
                        }
                    }
                    
                    // Convert to lowercase only for specific patterns that are typically lowercase
                    std::string lowerHtml = result.content;
                    std::transform(lowerHtml.begin(), lowerHtml.end(), lowerHtml.begin(), ::tolower);
                    
                    // Check for very specific SPA framework patterns only
                    std::vector<std::pair<std::string, std::string>> strongPatterns = {
                        // Next.js specific patterns
                        {"next-head-count", "Next.js"},
                        {"data-n-g", "Next.js"},
                        {"data-n-p", "Next.js"},
                        {"_next/static", "Next.js"},
                        {"__next__", "Next.js"},
                        {"__next_data__", "Next.js"},
                        // Nuxt.js patterns
                        {"_nuxt", "Nuxt.js"},
                        {"data-nuxt-", "Nuxt.js"},
                        {"__nuxt__", "Nuxt.js"},
                        // Gatsby patterns
                        {"___gatsby", "Gatsby"},
                        {"gatsby-", "Gatsby"},
                        {"__gatsby", "Gatsby"},
                        // React specific patterns
                        {"data-reactroot", "React"},
                        {"react-dom", "React DOM"},
                        {"reactdom", "React DOM"},
                        // Vue specific patterns
                        {"vue-router", "Vue Router"},
                        {"vuex", "Vuex"},
                        // AngularJS (1.x) patterns
                        {"angularjs", "AngularJS"},
                        // Modern Angular (2+) patterns
                        {"<app-root>", "Angular"},
                        {"<app-root ", "Angular"},
                        {"</app-root>", "Angular"},
                        {"runtime.", "Angular CLI"},
                        {"polyfills.", "Angular CLI"},
                        {"main.", "Angular CLI"},
                        {"ng-version", "Angular"},
                        {"ng-reflect-", "Angular"},
                        // State management (only if very specific)
                        {"redux-", "Redux"},
                        {"mobx-", "MobX"}
                    };

                    for (const auto& [pattern, name] : strongPatterns) {
                        if (lowerHtml.find(pattern) != std::string::npos) {
                            indicators.push_back(name);
                        }
                    }
                    
                    // Check for build tools only with specific context
                    if (lowerHtml.find("webpack") != std::string::npos && 
                        (lowerHtml.find("bundle") != std::string::npos || lowerHtml.find("chunk") != std::string::npos)) {
                        indicators.push_back("Webpack");
                    }
                    
                    if (lowerHtml.find("vite") != std::string::npos && 
                        (lowerHtml.find("hmr") != std::string::npos || lowerHtml.find("@vite") != std::string::npos)) {
                        indicators.push_back("Vite");
                    }

                    // Pattern matching with better specificity
                    std::vector<std::pair<std::string, std::string>> patterns = {
                        {"data-reactroot", "React Root Element"},
                        {"ember-", "Ember.js Directive"},
                        {"svelte-", "Svelte Directive"},
                        {"window.__initial_state__", "Initial State Object"},
                        {"window.__preloaded_state__", "Preloaded State"},
                        {"window.__data__", "Data Object"},
                        {"window.__props__", "Props Object"},
                        // Modern SPA patterns
                        {"data-n-g", "Next.js Generated"},
                        {"data-n-p", "Next.js Preloaded"},
                        {"next-head-count", "Next.js Head Count"},
                        {"id=\"app\"", "App Root Element"},
                        {"id=\"root\"", "Root Element"},
                        {"id=\"main\"", "Main Element"}
                    };

                    for (const auto& [pattern, name] : patterns) {
                        if (lowerHtml.find(pattern) != std::string::npos) {
                            indicators.push_back(name);
                        }
                    }

                    // Angular directive - be more specific with regex
                    std::regex ngPattern("\\bng-[a-zA-Z-]+\\b", std::regex_constants::icase);
                    if (std::regex_search(result.content, ngPattern)) {
                        indicators.push_back("Angular Directive");
                    }
                    
                    // Vue directives
                    std::regex vuePattern("\\bv-[a-zA-Z-]+\\b", std::regex_constants::icase);
                    if (std::regex_search(result.content, vuePattern)) {
                        indicators.push_back("Vue Directive");
                    }
                    
                    // Content analysis
                    std::regex scriptRegex(R"(<script[^>]*src[^>]*>)");
                    auto scriptMatches = std::distance(std::sregex_iterator(lowerHtml.begin(), lowerHtml.end(), scriptRegex), std::sregex_iterator());
                    
                    if (scriptMatches > 5) {
                        indicators.push_back("Multiple Script Tags (" + std::to_string(scriptMatches) + ")");
                    }
                    
                    // Calculate confidence based on indicators
                    double confidence = 0.0;
                    confidence = std::min(100.0, static_cast<double>(indicators.size()) * 20.0);
                    if (scriptMatches > 8) confidence += 15.0;
                    if (result.content.size() < 5000) confidence += 10.0; // Small initial HTML
                    
                    response["spaDetection"]["indicators"] = indicators;
                    response["spaDetection"]["confidence"] = std::min(100.0, confidence);
                }
                
                // Add content preview
                std::string preview = result.content.substr(0, 500);
                if (result.content.length() > 500) {
                    preview += "...";
                }
                response["contentPreview"] = preview;
                
                json(res, response);
                
            } catch (const nlohmann::json::parse_error& e) {
                LOG_ERROR("Failed to parse JSON: " + std::string(e.what()));
                badRequest(res, "Invalid JSON format");
            } catch (const std::exception& e) {
                LOG_ERROR("Unexpected error in detectSpa: " + std::string(e.what()));
                serverError(res, "An unexpected error occurred");
            }
        }
    });
    
    res->onAborted([]() {
        LOG_WARNING("SPA detection request aborted");
    });
}

void SearchController::renderPage(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    LOG_INFO("SearchController::renderPage called");
    std::string buffer;
    res->onData([this, res, buffer = std::move(buffer)](std::string_view data, bool last) mutable {
        buffer.append(data.data(), data.length());
        if (!last) return;
        try {
            nlohmann::json requestJson = nlohmann::json::parse(buffer);
            if (!requestJson.contains("url") || !requestJson["url"].is_string()) {
                res->writeStatus("400 Bad Request");
                res->writeHeader("Content-Type", "application/json");
                res->end(R"({"error": "URL is required and must be a string", "success": false})");
                return;
            }
            std::string url = requestJson["url"];
            int timeout = requestJson.value("timeout", 30000); // ms
            // Create PageFetcher with SPA rendering enabled
            PageFetcher fetcher("Hatefbot/1.0", std::chrono::milliseconds(timeout), true, 5);
            fetcher.setSpaRendering(true, "http://browserless:3000", /*useWebsocket=*/true, /*wsConnectionsPerCpu=*/1);
            auto startTime = std::chrono::high_resolution_clock::now();
            auto result = fetcher.fetch(url);
            auto endTime = std::chrono::high_resolution_clock::now();
            auto fetchDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
            nlohmann::json response;
            response["success"] = result.success;
            response["url"] = url;
            response["fetchDuration"] = fetchDuration.count();
            if (result.success) {
                response["httpStatusCode"] = result.statusCode;
                response["contentType"] = result.contentType;
                response["contentSize"] = result.content.size();
                std::string preview = result.content.substr(0, 500);
                if (result.content.size() > 500) preview += "...";
                response["contentPreview"] = preview;
                if (requestJson.value("includeFullContent", false)) {
                    response["content"] = result.content;
                }
                bool isSpa = fetcher.isSpaPage(result.content, url);
                response["isSpa"] = isSpa;
                response["renderingMethod"] = isSpa ? "headless_browser" : "direct_fetch";
                LOG_INFO("Successfully rendered page: " + url + ", size: " + std::to_string(result.content.size()) + " bytes");
            } else {
                response["error"] = result.errorMessage;
                response["httpStatusCode"] = result.statusCode;
                LOG_ERROR("Failed to render page: " + url + ", error: " + result.errorMessage);
            }
            res->writeStatus("200 OK");
            res->writeHeader("Content-Type", "application/json");
            res->end(response.dump());
        } catch (const nlohmann::json::exception& e) {
            res->writeStatus("400 Bad Request");
            res->writeHeader("Content-Type", "application/json");
            res->end(R"({"error": "Invalid JSON format", "success": false})");
        } catch (const std::exception& e) {
            LOG_ERROR("Exception in renderPage: " + std::string(e.what()));
            res->writeStatus("500 Internal Server Error");
            res->writeHeader("Content-Type", "application/json");
            res->end(R"({"error": "Internal server error", "success": false})");
        }
    });
    res->onAborted([]() {
        LOG_WARNING("Render page request aborted");
    });
}

nlohmann::json SearchController::parseRedisSearchResponse(const std::string& rawResponse, int page, int limit) {
    nlohmann::json response;
    response["meta"] = nlohmann::json::object();
    response["results"] = nlohmann::json::array();
    
    try {
        // Parse the raw JSON string from SearchClient
        nlohmann::json redisResponse = nlohmann::json::parse(rawResponse);
        
        if (!redisResponse.is_array() || redisResponse.empty()) {
            response["meta"]["total"] = 0;
            response["meta"]["page"] = page;
            response["meta"]["pageSize"] = limit;
            return response;
        }
        
        // First element is the total count
        int totalResults = 0;
        if (redisResponse.size() > 0 && redisResponse[0].is_number()) {
            totalResults = redisResponse[0].get<int>();
        }
        
        response["meta"]["total"] = totalResults;
        response["meta"]["page"] = page;
        response["meta"]["pageSize"] = limit;
        
        // Parse each result (skip the count, then pairs of docId and fields)
        for (size_t i = 1; i < redisResponse.size(); i += 2) {
            if (i + 1 >= redisResponse.size()) break;
            
            std::string docId = redisResponse[i].is_string() ? redisResponse[i].get<std::string>() : "";
            
            if (redisResponse[i + 1].is_array()) {
                nlohmann::json fields = redisResponse[i + 1];
                
                nlohmann::json result;
                result["url"] = "";
                result["title"] = "";
                result["snippet"] = "";
                result["score"] = 1.0;
                
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
        response["meta"]["total"] = 0;
        response["meta"]["page"] = page;
        response["meta"]["pageSize"] = limit;
    }
    
    return response;
}

void SearchController::searchSiteProfiles(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    LOG_INFO("SearchController::searchSiteProfiles called");
    
    // Start timing for response time tracking
    auto searchStartTime = std::chrono::high_resolution_clock::now();
    
    // Parse query parameters
    auto params = parseQuery(req);
    
    // Check for required 'q' parameter
    auto qIt = params.find("q");
    if (qIt == params.end() || qIt->second.empty()) {
        nlohmann::json error = {
            {"success", false},
            {"message", "Query parameter 'q' is required"},
            {"error", "INVALID_REQUEST"}
        };
        
        json(res, error, "400 Bad Request");
        LOG_WARNING("indexed pages search request rejected: missing 'q' parameter");
        return;
    }
    
    std::string query = urlDecode(qIt->second);
    LOG_DEBUG("Decoded search query: " + query);
    
    // Parse pagination parameters
    int page = 1;
    int limit = 10;
    
    auto pageIt = params.find("page");
    if (pageIt != params.end()) {
        try {
            page = std::stoi(pageIt->second);
            if (page < 1 || page > 1000) {
                badRequest(res, "Page must be between 1 and 1000");
                return;
            }
        } catch (...) {
            badRequest(res, "Invalid page parameter");
            return;
        }
    }
    
    auto limitIt = params.find("limit");
    if (limitIt != params.end()) {
        try {
            limit = std::stoi(limitIt->second);
            if (limit < 1 || limit > 100) {
                badRequest(res, "Limit must be between 1 and 100");
                return;
            }
        } catch (...) {
            badRequest(res, "Invalid limit parameter");
            return;
        }
    }
    
    try {
        // Check if MongoDBStorage is available
        if (!g_mongoStorage) {
            serverError(res, "Search service not available");
            LOG_ERROR("MongoDBStorage not initialized for indexed pages search");
            return;
        }
        
        // Calculate skip for pagination
        int skip = (page - 1) * limit;
        
        LOG_DEBUG("Searching indexed pages with query: '" + query + "', page: " + std::to_string(page) + 
                  ", limit: " + std::to_string(limit) + ", skip: " + std::to_string(skip));
        
        // Get total count first
        auto countResult = g_mongoStorage->countSearchResults(query);
        if (!countResult.success) {
            LOG_ERROR("Failed to count search results: " + countResult.message);
            serverError(res, "Search operation failed");
            return;
        }
        
        int64_t totalResults = countResult.value;
        
        // Perform the search
        auto searchResult = g_mongoStorage->searchSiteProfiles(query, limit, skip);
        if (!searchResult.success) {
            LOG_ERROR("indexed pages search failed: " + searchResult.message);
            serverError(res, "Search operation failed");
            return;
        }
        
        // Calculate search time
        auto searchEndTime = std::chrono::high_resolution_clock::now();
        auto searchDuration = std::chrono::duration_cast<std::chrono::milliseconds>(searchEndTime - searchStartTime);
        
        // Build response
        nlohmann::json response = {
            {"success", true},
            {"message", "Search completed successfully"},
            {"data", {
                {"query", query},
                {"results", nlohmann::json::array()},
                {"pagination", {
                    {"page", page},
                    {"limit", limit},
                    {"totalResults", totalResults},
                    {"totalPages", (totalResults + limit - 1) / limit}
                }},
                {"searchTime", {
                    {"milliseconds", searchDuration.count()},
                    {"seconds", static_cast<double>(searchDuration.count()) / 1000.0}
                }}
            }}
        };
        
        // Add search results
        auto& resultsArray = response["data"]["results"];
        for (const auto& page : searchResult.value) {
            nlohmann::json profileJson = {
                {"url", page.url},
                {"title", page.title},
                {"domain", page.domain}
            };
            
            // Add description if available (truncated for long descriptions)
            if (page.description) {
                std::string description = *page.description;
                // Truncate descriptions longer than 300 characters
                profileJson["description"] = truncateDescription(description, 300);
            } else {
                profileJson["description"] = "";
            }
            
            // Add optional fields if available
            if (page.pageRank) {
                profileJson["pageRank"] = *page.pageRank;
            }
            
            if (page.contentQuality) {
                profileJson["contentQuality"] = *page.contentQuality;
            }
            
            if (page.wordCount) {
                profileJson["wordCount"] = *page.wordCount;
            }
            
            resultsArray.push_back(profileJson);
        }
        
        LOG_INFO("indexed pages search completed successfully: query='" + query + 
                 "', results=" + std::to_string(searchResult.value.size()) + 
                 "/" + std::to_string(totalResults) + 
                 ", time=" + std::to_string(searchDuration.count()) + "ms");
        
        json(res, response);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Unexpected error in searchSiteProfiles: " + std::string(e.what()));
        serverError(res, "An unexpected error occurred during search");
    }
}

void SearchController::logApiRequestError(const std::string& endpoint, const std::string& method, 
                                        const std::string& ipAddress, const std::string& userAgent,
                                        const std::chrono::system_clock::time_point& requestStartTime,
                                        const std::string& requestBody, const std::string& status, 
                                        const std::string& errorMessage) {
    // Log API request error asynchronously to avoid blocking the response
    std::thread([this, endpoint, method, ipAddress, userAgent, requestStartTime, requestBody, status, errorMessage]() {
        try {
            // Calculate response time
            auto responseEndTime = std::chrono::system_clock::now();
            auto responseTime = std::chrono::duration_cast<std::chrono::milliseconds>(responseEndTime - requestStartTime);
            
            // Create API request log
            search_engine::storage::ApiRequestLog apiLog;
            apiLog.endpoint = endpoint;
            apiLog.method = method;
            apiLog.ipAddress = ipAddress;
            apiLog.userAgent = userAgent;
            apiLog.createdAt = std::chrono::system_clock::now();
            apiLog.requestBody = requestBody;
            apiLog.status = status;
            apiLog.errorMessage = errorMessage;
            apiLog.responseTimeMs = static_cast<int>(responseTime.count());
            
            // Store in database if we have access to storage
            if (g_crawlerManager && g_crawlerManager->getStorage()) {
                auto result = g_crawlerManager->getStorage()->storeApiRequestLog(apiLog);
                if (result.success) {
                    LOG_INFO("API request error logged successfully with ID: " + result.value);
                } else {
                    LOG_WARNING("Failed to log API request error: " + result.message);
                }
            }
        } catch (const std::exception& e) {
            LOG_WARNING("Failed to log API request error: " + std::string(e.what()));
        }
    }).detach(); // Detach the thread to avoid blocking
}

// Helper methods for template rendering
std::string SearchController::loadFile(const std::string& path) const {
    LOG_DEBUG("Attempting to load file: " + path);
    
    if (!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path)) {
        LOG_ERROR("Error: File does not exist or is not a regular file: " + path);
        return "";
    }
    
    std::ifstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("Error: Could not open file: " + path);
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    if (content.empty()) {
        LOG_WARNING("Warning: File is empty: " + path);
    } else {
        LOG_INFO("Successfully loaded file: " + path + " (size: " + std::to_string(content.length()) + " bytes)");
    }
    
    return content;
}

std::string SearchController::renderTemplate(const std::string& templateName, const nlohmann::json& data) const {
    try {
        // Initialize Inja environment with absolute path and check if templates directory exists
        std::string templateDir = "/app/templates/";
        if (!std::filesystem::exists(templateDir)) {
            LOG_ERROR("Template directory does not exist: " + templateDir);
            throw std::runtime_error("Template directory not found");
        }
        LOG_DEBUG("Using template directory: " + templateDir);
        inja::Environment env(templateDir);
        
        // URL encoding is now done in C++ code and passed as search_query_encoded
        
        // Render the template with data
        std::string result = env.render_file(templateName, data);
        LOG_DEBUG("Successfully rendered template: " + templateName + " (size: " + std::to_string(result.size()) + " bytes)");
        return result;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to render template " + templateName + ": " + std::string(e.what()));
        return "";
    }
}

std::string SearchController::getDefaultLocale() const {
    return "fa"; // Persian as default
}

// Deep merge helper for JSON objects
static void jsonDeepMergeMissing(nlohmann::json &dst, const nlohmann::json &src) {
    if (!dst.is_object() || !src.is_object()) return;
    for (auto it = src.begin(); it != src.end(); ++it) {
        const std::string &key = it.key();
        if (dst.contains(key)) {
            if (dst[key].is_object() && it.value().is_object()) {
                jsonDeepMergeMissing(dst[key], it.value());
            }
        } else {
            dst[key] = it.value();
        }
    }
}

void SearchController::searchResultsPage(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    LOG_INFO("SearchController::searchResultsPage - Serving search results page");

    // Start timing
    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        // Parse query parameters
        auto params = parseQuery(req);
        
        // Get search query
        auto qIt = params.find("q");
        if (qIt == params.end() || qIt->second.empty()) {
            // Redirect to home page if no query provided
            res->writeStatus("302 Found");
            res->writeHeader("Location", "/");
            res->end();
            return;
        }
        
		std::string searchQuery = urlDecode(qIt->second);
        LOG_DEBUG("Search query: " + searchQuery);
        
        // Extract language parameter (default to Persian)
        std::string langCode = getDefaultLocale();
        auto langIt = params.find("lang");
        if (langIt != params.end() && !langIt->second.empty()) {
            std::string requestedLang = langIt->second;
            std::string metaFile = "locales/" + requestedLang + "/search.json";
            if (std::filesystem::exists(metaFile)) {
                langCode = requestedLang;
                LOG_DEBUG("Using requested language: " + langCode);
            } else {
                LOG_WARNING("Requested language not found: " + requestedLang + ", using default: " + langCode);
            }
        }
        
        // Load localization files
        std::string commonPath = "locales/" + langCode + "/common.json";
        std::string searchPath = "locales/" + langCode + "/search.json";
        
        std::string commonContent = loadFile(commonPath);
        std::string searchContent = loadFile(searchPath);
        
        if (commonContent.empty() || searchContent.empty()) {
            LOG_ERROR("Failed to load localization files for language: " + langCode);
            // Fallback to default language
            if (langCode != getDefaultLocale()) {
                langCode = getDefaultLocale();
                commonPath = "locales/" + langCode + "/common.json";
                searchPath = "locales/" + langCode + "/search.json";
                commonContent = loadFile(commonPath);
                searchContent = loadFile(searchPath);
            }
            
            if (commonContent.empty() || searchContent.empty()) {
                serverError(res, "Failed to load localization files");
                return;
            }
        }
        
        // Parse JSON files
        nlohmann::json commonJson = nlohmann::json::parse(commonContent);
        nlohmann::json searchJson = nlohmann::json::parse(searchContent);
        
        // Merge search localization into common
        jsonDeepMergeMissing(commonJson, searchJson);
        
		// Perform search via MongoDB (same logic as /api/search/sites)
		std::vector<nlohmann::json> searchResults;
		int totalResults = 0;
		
		// Pagination
		int page = 1;
		int limit = 10;
		auto pageIt = params.find("page");
		if (pageIt != params.end()) {
			try {
				page = std::stoi(pageIt->second);
				if (page < 1 || page > 1000) page = 1;
			} catch (...) { page = 1; }
		}
		int skip = (page - 1) * limit;
		
		try {
			if (!g_mongoStorage) {
				LOG_ERROR("MongoDBStorage not initialized for searchResultsPage");
				serverError(res, "Search service not available");
				return;
			}
			
			auto countResult = g_mongoStorage->countSearchResults(searchQuery);
			if (!countResult.success) {
				LOG_ERROR("Failed to count search results: " + countResult.message);
				serverError(res, "Search operation failed");
				return;
			}
			totalResults = static_cast<int>(countResult.value);
			
			auto searchResult = g_mongoStorage->searchSiteProfiles(searchQuery, limit, skip);
			if (!searchResult.success) {
				LOG_ERROR("indexed pages search failed: " + searchResult.message);
				serverError(res, "Search operation failed");
				return;
			}
			
			for (const auto& page : searchResult.value) {
				std::string displayUrl = page.url;

				// Clean up display URL (remove protocol and www)
				if (displayUrl.rfind("https://", 0) == 0) {
					displayUrl = displayUrl.substr(8);
				} else if (displayUrl.rfind("http://", 0) == 0) {
					displayUrl = displayUrl.substr(7);
				}
				if (displayUrl.rfind("www.", 0) == 0) {
					displayUrl = displayUrl.substr(4);
				}

				nlohmann::json formattedResult;
				formattedResult["url"] = std::string(page.url);
				formattedResult["title"] = std::string(page.title);
				formattedResult["displayurl"] = std::string(displayUrl);

				// Handle optional description with truncation for long descriptions
				if (page.description.has_value()) {
					std::string description = std::string(*page.description);
					// Truncate descriptions longer than 300 characters
					formattedResult["desc"] = truncateDescription(description, 300);
				} else {
					formattedResult["desc"] = std::string("");
				}

				searchResults.push_back(formattedResult);
			}
		} catch (const std::exception& e) {
			LOG_ERROR("MongoDB search error in searchResultsPage: " + std::string(e.what()));
			// Continue with empty results to still render page
		}
        
        // Get the host from the request headers for base_url
        std::string host = std::string(req->getHeader("host"));
        std::string protocol = "http://";
        
        // Check if we're behind a proxy (X-Forwarded-Proto header)
        std::string forwardedProto = std::string(req->getHeader("x-forwarded-proto"));
        if (!forwardedProto.empty()) {
            protocol = forwardedProto + "://";
        }
        
        std::string baseUrl = protocol + host;
        
        // URL encode the search query for use in URLs
        std::string encodedSearchQuery = searchQuery;
        // Simple URL encoding for the search query
        std::string encoded;
        for (char c : searchQuery) {
            if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                encoded += c;
            } else {
                std::ostringstream oss;
                oss << '%' << std::hex << std::uppercase << (unsigned char)c;
                encoded += oss.str();
            }
        }
        encodedSearchQuery = encoded;

        // Calculate elapsed time
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        double elapsedSeconds = duration.count() / 1000000.0;

        // Format elapsed time with appropriate precision
        std::stringstream timeStream;
        if (elapsedSeconds < 0.01) {
            timeStream << std::fixed << std::setprecision(3) << elapsedSeconds;
        } else if (elapsedSeconds < 0.1) {
            timeStream << std::fixed << std::setprecision(2) << elapsedSeconds;
        } else if (elapsedSeconds < 1.0) {
            timeStream << std::fixed << std::setprecision(2) << elapsedSeconds;
        } else {
            timeStream << std::fixed << std::setprecision(1) << elapsedSeconds;
        }
        std::string elapsedTimeStr = timeStream.str();

        // Prepare template data
        nlohmann::json templateData = {
            {"t", commonJson},
            {"base_url", baseUrl},
            {"search_query", searchQuery},
            {"search_query_encoded", encodedSearchQuery},
            {"current_lang", langCode},
            {"total_results", std::to_string(totalResults)},
            {"elapsed_time", elapsedTimeStr},
            {"results", searchResults}
        };

        LOG_DEBUG("Rendering search results template with " + std::to_string(searchResults.size()) + " results");
        
        // Render template
        std::string renderedHtml = renderTemplate("search.inja", templateData);
        
        if (renderedHtml.empty()) {
            LOG_ERROR("Failed to render search results template");
            serverError(res, "Failed to render search results page");
            return;
        }
        
        html(res, renderedHtml);
        LOG_INFO("Successfully served search results page for query: " + searchQuery + 
                 " (results: " + std::to_string(searchResults.size()) + ", lang: " + langCode + ")");
        
    } catch (const nlohmann::json::exception& e) {
        LOG_ERROR("JSON parsing error in search results: " + std::string(e.what()));
        serverError(res, "Failed to load search results page");
    } catch (const std::exception& e) {
        LOG_ERROR("Error serving search results page: " + std::string(e.what()));
        serverError(res, "Failed to load search results page");
    }
}

// Register the renderPage endpoint
namespace {
    struct RenderPageRouteRegister {
        RenderPageRouteRegister() {
            routing::RouteRegistry::getInstance().registerRoute({
                routing::HttpMethod::POST,
                "/api/spa/render",
                [](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
                    static SearchController controller;
                    controller.renderPage(res, req);
                },
                "SearchController",
                "renderPage"
            });
        }
    };
    static RenderPageRouteRegister _renderPageRouteRegisterInstance;
}

void SearchController::sendCrawlCompletionEmail(const std::string& sessionId, const std::string& email, 
                                               const std::string& url, const std::vector<CrawlResult>& results,
                                               const std::string& language, const std::string& recipientName) {
    try {
        LOG_INFO("Sending crawl completion email for session: " + sessionId + " to: " + email + " (language: " + language + ", recipientName: " + recipientName + ")");
        
        // Get email service using lazy initialization
        auto emailService = getEmailService();
        if (!emailService) {
            LOG_ERROR("Failed to get email service for crawl completion notification");
            return;
        }
        
        // Extract domain from URL for display
        std::string domainName = url;
        try {
            auto parsedUrl = std::string(url);
            size_t protocolEnd = parsedUrl.find("://");
            if (protocolEnd != std::string::npos) {
                size_t domainStart = protocolEnd + 3;
                size_t domainEnd = parsedUrl.find('/', domainStart);
                if (domainEnd != std::string::npos) {
                    domainName = parsedUrl.substr(domainStart, domainEnd - domainStart);
                } else {
                    domainName = parsedUrl.substr(domainStart);
                }
            }
        } catch (const std::exception& e) {
            LOG_WARNING("Failed to extract domain from URL: " + url + ", using full URL");
        }
        
        // Count successful results
        int crawledPagesCount = 0;
        for (const auto& result : results) {
            if (result.success && result.crawlStatus == "downloaded") {
                crawledPagesCount++;
            }
        }
        
        // Load localized sender name and subject using the provided language
        std::string senderName = loadLocalizedSenderName(language);
        std::string localizedSubject = loadLocalizedSubject(language, crawledPagesCount);
        
        // Prepare notification data
        search_engine::storage::EmailService::NotificationData data;
        data.recipientEmail = email;
        // Use provided recipientName if available, otherwise fallback to email prefix
        data.recipientName = !recipientName.empty() ? recipientName : email.substr(0, email.find('@'));
        data.domainName = domainName;
        data.crawledPagesCount = crawledPagesCount;
        data.crawlSessionId = sessionId;
        data.crawlCompletedAt = std::chrono::system_clock::now();
        data.language = language;
        data.subject = localizedSubject; // Set localized subject
        
        // Send email asynchronously with localized sender name
        bool success = emailService->sendCrawlingNotificationAsync(data, senderName, "");
        
        if (success) {
            LOG_INFO("Crawl completion email queued successfully for session: " + sessionId + 
                     " to: " + email + " (pages: " + std::to_string(crawledPagesCount) + ")");
        } else {
            LOG_ERROR("Failed to queue crawl completion email for session: " + sessionId + 
                      " to: " + email + ", error: " + emailService->getLastError());
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in sendCrawlCompletionEmail for session " + sessionId + 
                  " to " + email + ": " + e.what());
    }
}

search_engine::storage::EmailService* SearchController::getEmailService() const {
    if (!emailService_) {
        try {
            LOG_INFO("Lazy initializing EmailService in SearchController");
            auto config = loadSMTPConfig();
            emailService_ = std::make_unique<search_engine::storage::EmailService>(config);
            LOG_INFO("EmailService initialized successfully in SearchController");
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to initialize EmailService in SearchController: " + std::string(e.what()));
            return nullptr;
        }
    }
    return emailService_.get();
}

search_engine::storage::EmailService::SMTPConfig SearchController::loadSMTPConfig() const {
    search_engine::storage::EmailService::SMTPConfig config;
    
    // Load from environment variables (works with Docker Compose and .env files)
    const char* smtpHost = std::getenv("SMTP_HOST");
    config.smtpHost = smtpHost ? smtpHost : "smtp.gmail.com";
    
    const char* smtpPort = std::getenv("SMTP_PORT");
    config.smtpPort = smtpPort ? std::stoi(smtpPort) : 587;
    
    const char* smtpUsername = std::getenv("SMTP_USERNAME");
    config.username = smtpUsername ? smtpUsername : "";
    
    const char* smtpPassword = std::getenv("SMTP_PASSWORD");
    config.password = smtpPassword ? smtpPassword : "";
    
    const char* fromEmail = std::getenv("FROM_EMAIL");
    config.fromEmail = fromEmail ? fromEmail : "noreply@hatef.ir";
    
    const char* fromName = std::getenv("FROM_NAME");
    config.fromName = fromName ? fromName : "Search Engine";
    
    const char* useTLS = std::getenv("SMTP_USE_TLS");
    if (useTLS) {
        std::string tlsStr = std::string(useTLS);
        std::transform(tlsStr.begin(), tlsStr.end(), tlsStr.begin(), ::tolower);
        config.useTLS = (tlsStr == "true" || tlsStr == "1" || tlsStr == "yes");
    } else {
        config.useTLS = true; // Default value
    }
    
    // Load timeout configuration
    const char* timeoutSeconds = std::getenv("SMTP_TIMEOUT");
    if (timeoutSeconds) {
        try {
            config.timeoutSeconds = std::stoi(timeoutSeconds);
        } catch (const std::exception& e) {
            LOG_WARNING("Invalid SMTP_TIMEOUT value, using default: 30 seconds");
            config.timeoutSeconds = 30;
        }
    } else {
        config.timeoutSeconds = 30; // Default value
    }
    
    const char* connectionTimeoutSeconds = std::getenv("SMTP_CONNECTION_TIMEOUT");
    if (connectionTimeoutSeconds) {
        try {
            config.connectionTimeoutSeconds = std::stoi(connectionTimeoutSeconds);
        } catch (const std::exception& e) {
            LOG_WARNING("Invalid SMTP_CONNECTION_TIMEOUT value, using auto-calculate");
            config.connectionTimeoutSeconds = 0; // Auto-calculate
        }
    } else {
        config.connectionTimeoutSeconds = 0; // Auto-calculate
    }
    
    LOG_DEBUG("SMTP Config loaded - Host: " + config.smtpHost + 
              ", Port: " + std::to_string(config.smtpPort) + 
              ", From: " + config.fromEmail + 
              ", TLS: " + (config.useTLS ? "true" : "false") +
              ", Timeout: " + std::to_string(config.timeoutSeconds) + "s" +
              ", Connection Timeout: " + std::to_string(config.connectionTimeoutSeconds) + "s");
    
    return config;
}

std::string SearchController::loadLocalizedSenderName(const std::string& language) const {
    try {
        // Load localization file
        std::string localesPath = "locales/" + language + "/crawling-notification.json";
        std::string localeContent = loadFile(localesPath);
        
        if (localeContent.empty() && language != "en") {
            LOG_WARNING("SearchController: Failed to load locale file: " + localesPath + ", falling back to English");
            localesPath = "locales/en/crawling-notification.json";
            localeContent = loadFile(localesPath);
        }
        
        if (localeContent.empty()) {
            LOG_WARNING("SearchController: Failed to load any localization file, using default sender name");
            return "Hatef Search Engine"; // Default fallback
        }
        
        // Parse JSON and extract sender name
        nlohmann::json localeData = nlohmann::json::parse(localeContent);
        
        if (localeData.contains("email") && localeData["email"].contains("sender_name")) {
            std::string senderName = localeData["email"]["sender_name"];
            LOG_DEBUG("SearchController: Loaded localized sender name: " + senderName + " for language: " + language);
            return senderName;
        } else {
            LOG_WARNING("SearchController: sender_name not found in locale file, using default");
            return "Hatef Search Engine"; // Default fallback
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("SearchController: Exception loading localized sender name for language " + language + ": " + e.what());
        return "Hatef Search Engine"; // Default fallback
    }
}

std::string SearchController::loadLocalizedSubject(const std::string& language, int pageCount) const {
    try {
        // Load localization file
        std::string localesPath = "locales/" + language + "/crawling-notification.json";
        std::string localeContent = loadFile(localesPath);
        
        if (localeContent.empty() && language != "en") {
            LOG_WARNING("SearchController: Failed to load locale file: " + localesPath + ", falling back to English");
            localesPath = "locales/en/crawling-notification.json";
            localeContent = loadFile(localesPath);
        }
        
        if (localeContent.empty()) {
            LOG_WARNING("SearchController: Failed to load any localization file, using default subject");
            return "Crawling Complete - " + std::to_string(pageCount) + " pages indexed"; // Default fallback
        }
        
        // Parse JSON and extract subject
        nlohmann::json localeData = nlohmann::json::parse(localeContent);
        
        if (localeData.contains("email") && localeData["email"].contains("subject")) {
            std::string subject = localeData["email"]["subject"];
            
            // Replace {pages} placeholder with actual count
            size_t pos = subject.find("{pages}");
            if (pos != std::string::npos) {
                subject.replace(pos, 7, std::to_string(pageCount));
            }
            
            LOG_DEBUG("SearchController: Loaded localized subject: " + subject + " for language: " + language);
            return subject;
        } else {
            LOG_WARNING("SearchController: subject not found in locale file, using default");
            return "Crawling Complete - " + std::to_string(pageCount) + " pages indexed"; // Default fallback
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("SearchController: Exception loading localized subject for language " + language + ": " + e.what());
        return "Crawling Complete - " + std::to_string(pageCount) + " pages indexed"; // Default fallback
    }
} 