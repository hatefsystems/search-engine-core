#include "SearchController.h"
#include "../../include/Logger.h"
#include "../../src/crawler/Crawler.h"
#include "../../src/crawler/PageFetcher.h"
#include "../../src/crawler/models/CrawlConfig.h"
#include "../../include/search_engine/storage/ContentStorage.h"
#include <cstdlib>
#include <chrono>
#include <iomanip>
#include <map>
#include <mutex>
#include <numeric>
#include <regex>

using namespace hatef::search;

// Static SearchClient instance
static std::unique_ptr<SearchClient> g_searchClient;
static std::once_flag g_initFlag;

// Static Crawler instance
static std::unique_ptr<Crawler> g_crawler;
static std::once_flag g_crawlerInitFlag;

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
    
    // Initialize Crawler once
    std::call_once(g_crawlerInitFlag, []() {
        CrawlConfig config;
        config.maxPages = 1000;  // Default max pages
        config.maxDepth = 3;     // Default max depth
        config.userAgent = "Hatefbot/1.0";
        
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
            
            // Initialize crawler with database storage
            g_crawler = std::make_unique<Crawler>(config, storage);
            
            // Disable SSL verification for problematic sites (like time.ir)
            g_crawler->getPageFetcher()->setVerifySSL(false);

            // Enable SPA rendering with browserless
            g_crawler->getPageFetcher()->setSpaRendering(true, "http://browserless:3000");

            LOG_INFO("Crawler initialized successfully with database storage, SSL verification disabled, and SPA rendering enabled");
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to initialize Crawler: " + std::string(e.what()));
            throw;
        }
    });
}

void SearchController::addSiteToCrawl(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    LOG_INFO("SearchController::addSiteToCrawl called");
    
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
                int maxPages = jsonBody.value("maxPages", 1000);
                int maxDepth = jsonBody.value("maxDepth", 3);
                bool restrictToSeedDomain = jsonBody.value("restrictToSeedDomain", true);
                bool followRedirects = jsonBody.value("followRedirects", true);  // Default to true for cookie handling
                int maxRedirects = jsonBody.value("maxRedirects", 10);  // Increase default to handle cookie redirects
                bool force = jsonBody.value("force", false);
                bool spaRenderingEnabled = jsonBody.value("spaRenderingEnabled", true);  // Default to enabled
                bool includeFullContent = jsonBody.value("includeFullContent", false);
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
                
                // Add URL to crawler
                if (g_crawler) {
                    // Update crawler configuration with the provided parameters
                    g_crawler->setMaxPages(maxPages);
                    g_crawler->setMaxDepth(maxDepth);
                    
                    // Update domain restriction setting
                    CrawlConfig currentConfig = g_crawler->getConfig();
                    currentConfig.restrictToSeedDomain = restrictToSeedDomain;
                    currentConfig.followRedirects = followRedirects;
                    currentConfig.maxRedirects = maxRedirects;
                    currentConfig.spaRenderingEnabled = spaRenderingEnabled;
                    currentConfig.includeFullContent = includeFullContent;
                    currentConfig.browserlessUrl = browserlessUrl;
                    g_crawler->updateConfig(currentConfig);
                    
                    g_crawler->addSeedURL(url, force);
                    
                    // Start crawling if not already running
                    g_crawler->start();
                    
                    LOG_INFO("Added site to crawl: " + url + " (maxPages: " + std::to_string(maxPages) + 
                             ", maxDepth: " + std::to_string(maxDepth) + 
                             ", restrictToSeedDomain: " + (restrictToSeedDomain ? "true" : "false") + 
                             ", followRedirects: " + (followRedirects ? "true" : "false") + 
                             ", maxRedirects: " + std::to_string(maxRedirects) + 
                             ", spaRenderingEnabled: " + (spaRenderingEnabled ? "true" : "false") + 
                             ", includeFullContent: " + (includeFullContent ? "true" : "false") + ")");
                    
                    // Return success response
                    nlohmann::json response = {
                        {"success", true},
                        {"message", "Site added to crawl queue successfully"},
                        {"data", {
                            {"url", url},
                            {"maxPages", maxPages},
                            {"maxDepth", maxDepth},
                            {"restrictToSeedDomain", restrictToSeedDomain},
                            {"followRedirects", followRedirects},
                            {"maxRedirects", maxRedirects},
                            {"spaRenderingEnabled", spaRenderingEnabled},
                            {"includeFullContent", includeFullContent},
                            {"browserlessUrl", browserlessUrl},
                            {"status", "queued"}
                        }}
                    };
                    
                    json(res, response);
                } else {
                    serverError(res, "Crawler not initialized");
                }
                
            } catch (const nlohmann::json::parse_error& e) {
                LOG_ERROR("Failed to parse JSON: " + std::string(e.what()));
                badRequest(res, "Invalid JSON format");
            } catch (const std::exception& e) {
                LOG_ERROR("Unexpected error in addSiteToCrawl: " + std::string(e.what()));
                serverError(res, "An unexpected error occurred");
            }
        }
    });
    
    res->onAborted([]() {
        LOG_WARNING("Add site to crawl request aborted");
    });
}

void SearchController::search(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
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
        
        // Execute search
        std::string rawResult = g_searchClient->search(searchIndex, qIt->second, searchArgs);
        
        // Parse and format response
        nlohmann::json response = parseRedisSearchResponse(rawResult, page, limit);
        
        LOG_INFO("Search request successful: q=" + qIt->second + 
                 ", page=" + std::to_string(page) + 
                 ", limit=" + std::to_string(limit));
        
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
        
        // Get optional parameters
        bool includeLogs = params.find("logs") != params.end() && params["logs"] == "true";
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
        
        if (g_crawler) {
            // Get crawl results
            auto results = g_crawler->getResults();
            
            // Basic status information
            response["status"] = {
                {"isRunning", true}, // We'll need to add a method to check this
                {"totalCrawled", static_cast<int>(results.size())},
                {"lastUpdate", std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            };
            
            // Include crawl results if requested
            if (includeResults) {
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
                    
                    // Add optional fields if they exist
                    if (result.title.has_value()) {
                        resultJson["title"] = result.title.value();
                    }
                    
                    if (result.rawContent.has_value()) {
                        resultJson["contentLength"] = static_cast<int>(result.rawContent.value().length());
                    }
                    
                    if (!result.success && result.errorMessage.has_value()) {
                        resultJson["error"] = result.errorMessage.value();
                    }
                    
                    resultsArray.push_back(resultJson);
                }
                
                response["results"] = resultsArray;
            }
            
            // Include recent logs if requested
            if (includeLogs) {
                // For now, we'll return a placeholder for logs
                // In a real implementation, you'd want to capture logs from the crawler
                response["logs"] = {
                    {"message", "Log collection not yet implemented"},
                    {"note", "Consider implementing a log collector in the Crawler class"}
                };
            }
            
            // Crawl statistics
            int successfulCrawls = 0;
            int failedCrawls = 0;
            int totalLinks = 0;
            
            for (const auto& result : results) {
                if (result.success) {
                    successfulCrawls++;
                    totalLinks += result.links.size();
                } else {
                    failedCrawls++;
                }
            }
            
            response["statistics"] = {
                {"successfulCrawls", successfulCrawls},
                {"failedCrawls", failedCrawls},
                {"totalLinksFound", totalLinks},
                {"successRate", results.empty() ? 0.0 : (double)successfulCrawls / results.size() * 100.0}
            };
            
        } else {
            response["status"] = {
                {"isRunning", false},
                {"message", "Crawler not initialized"},
                {"totalCrawled", 0}
            };
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
    // Access ContentStorage from the crawler
    auto storage = g_crawler ? g_crawler->getStorage() : nullptr;
    if (!storage) {
        serverError(res, "Database storage not available");
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
                std::string lowerHtml = result.content;
                std::transform(lowerHtml.begin(), lowerHtml.end(), lowerHtml.begin(), ::tolower);
                
                // Framework detection with better pattern matching
                std::vector<std::pair<std::string, std::string>> frameworks = {
                    {"react", "React"},
                    {"vue", "Vue.js"},
                    {"angular", "Angular"},
                    {"ember", "Ember"},
                    {"backbone", "Backbone"},
                    {"svelte", "Svelte"},
                    {"single-page", "Single Page Application"},
                    {"client-side", "Client-side Rendering"},
                    // Next.js and modern frameworks
                    {"next-head-count", "Next.js"},
                    {"data-n-g", "Next.js"},
                    {"data-n-p", "Next.js"},
                    {"_next/static", "Next.js"},
                    {"next.js", "Next.js"},
                    {"nextjs", "Next.js"},
                    {"nuxt", "Nuxt.js"},
                    {"nuxtjs", "Nuxt.js"},
                    {"_nuxt", "Nuxt.js"},
                    {"gatsby", "Gatsby"},
                    {"gatsbyjs", "Gatsby"},
                    {"___gatsby", "Gatsby"},
                    {"remix", "Remix"},
                    {"sveltekit", "SvelteKit"},
                    {"astro", "Astro"},
                    {"qwik", "Qwik"},
                    // State management
                    {"redux", "Redux"},
                    {"mobx", "MobX"},
                    {"zustand", "Zustand"},
                    {"recoil", "Recoil"},
                    {"jotai", "Jotai"},
                    // Build tools
                    {"webpack", "Webpack"},
                    {"vite", "Vite"},
                    {"parcel", "Parcel"},
                    {"rollup", "Rollup"},
                    {"esbuild", "esbuild"}
                };
                
                for (const auto& [pattern, name] : frameworks) {
                    if (lowerHtml.find(pattern) != std::string::npos) {
                        indicators.push_back(name);
                    }
                }
                
                // Special handling for problematic patterns that cause false positives
                // SPA - only if it's actually SPA framework related
                if (lowerHtml.find("single-page application") != std::string::npos ||
                    lowerHtml.find("spa framework") != std::string::npos ||
                    lowerHtml.find("spa.js") != std::string::npos ||
                    lowerHtml.find("spa/") != std::string::npos) {
                    indicators.push_back("SPA Framework");
                }
                
                // SolidJS - only if it's actually SolidJS, not just "solid"
                if (lowerHtml.find("solidjs") != std::string::npos ||
                    lowerHtml.find("solid-") != std::string::npos ||
                    lowerHtml.find("solid.js") != std::string::npos) {
                    indicators.push_back("SolidJS");
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
                
                // Special handling for hydration patterns to avoid false positives
                if (lowerHtml.find("hydration") != std::string::npos) {
                    // Only if it's actually about JavaScript hydration, not just the word
                    if (lowerHtml.find("hydrate") != std::string::npos ||
                        lowerHtml.find("client-side") != std::string::npos) {
                        indicators.push_back("Hydration Pattern");
                    }
                }
                
                // Angular directive - be more specific
                if (lowerHtml.find("ng-") != std::string::npos) {
                    // Check if it's actually an Angular directive, not just a word containing "ng-"
                    std::regex ngPattern("\\bng-[a-zA-Z-]+\\b", std::regex_constants::icase);
                    if (std::regex_search(lowerHtml, ngPattern)) {
                        indicators.push_back("Angular Directive");
                    }
                }
                
                // Content analysis
                std::regex scriptRegex(R"(<script[^>]*src[^>]*>)");
                auto scriptMatches = std::distance(std::sregex_iterator(lowerHtml.begin(), lowerHtml.end(), scriptRegex), std::sregex_iterator());
                
                if (scriptMatches > 2) {
                    indicators.push_back("Multiple Script Tags (" + std::to_string(scriptMatches) + ")");
                }
                
                // Calculate confidence based on indicators
                double confidence = 0.0;
                if (isSpa) {
                    confidence = std::min(100.0, static_cast<double>(indicators.size()) * 25.0);
                    if (scriptMatches > 5) confidence += 20.0;
                    if (result.content.size() < 10000) confidence += 15.0; // Small initial HTML
                }
                
                response["spaDetection"]["indicators"] = indicators;
                response["spaDetection"]["confidence"] = std::min(100.0, confidence);
                
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
            fetcher.setSpaRendering(true, "http://browserless:3000");
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