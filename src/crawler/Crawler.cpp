#include "Crawler.h"
#include "URLFrontier.h"
#include "RobotsTxtParser.h"
#include "PageFetcher.h"
#include "ContentParser.h"
#include "FailureClassifier.h"
#include "DomainManager.h"
#include "CrawlMetrics.h"
#include "../../include/Logger.h"
#include "../../include/crawler/CrawlLogger.h"
#include "../storage/MongoDBStorage.h"
#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <algorithm>

Crawler::Crawler(const CrawlConfig& config, std::shared_ptr<search_engine::storage::ContentStorage> storage, const std::string& sessionId)
    : storage(storage)
    , config(config)
    , isRunning(false)
    , sessionId(sessionId)
    , sessionSpaDetected(false)
    , sessionSpaChecked(false) {
    LOG_INFO("🏗️ Crawler::Crawler - Initializing crawler with session: " + sessionId);
    LOG_DEBUG("Crawler::Crawler - Configuration details:");
    LOG_DEBUG("  • Max Pages: " + std::to_string(config.maxPages));
    LOG_DEBUG("  • Max Depth: " + std::to_string(config.maxDepth));
    LOG_DEBUG("  • User Agent: " + config.userAgent);
    LOG_DEBUG("  • Request Timeout: " + std::to_string(config.requestTimeout.count()) + "ms");
    LOG_DEBUG("  • SPA Rendering: " + std::string(config.spaRenderingEnabled ? "enabled" : "disabled"));
    LOG_DEBUG("  • Extract Text Content: " + std::string(config.extractTextContent ? "enabled" : "disabled"));
    LOG_DEBUG("  • Follow Redirects: " + std::string(config.followRedirects ? "enabled" : "disabled"));

    // Initialize components with detailed logging
    LOG_TRACE("Crawler::Crawler - Creating URLFrontier");
    urlFrontier = std::make_unique<URLFrontier>();

    if (storage && storage->getMongoStorage()) {
        LOG_DEBUG("Crawler::Crawler - Setting up MongoDB persistent storage for session: " + sessionId);
        static search_engine::storage::MongoFrontierPersistence staticMongoPers(storage->getMongoStorage());
        urlFrontier->setPersistentStorage(&staticMongoPers, sessionId);
        LOG_INFO("✅ MongoDB persistent storage configured for frontier");
    } else {
        LOG_WARNING("⚠️ No MongoDB storage available - frontier will not be persistent");
    }

    LOG_TRACE("Crawler::Crawler - Creating RobotsTxtParser");
    robotsParser = std::make_unique<RobotsTxtParser>();

    LOG_TRACE("Crawler::Crawler - Creating PageFetcher");
    pageFetcher = std::make_unique<PageFetcher>(
        config.userAgent,
        config.requestTimeout,
        config.followRedirects,
        config.maxRedirects
    );

    LOG_TRACE("Crawler::Crawler - Creating ContentParser");
    contentParser = std::make_unique<ContentParser>();

    LOG_TRACE("Crawler::Crawler - Creating DomainManager");
    domainManager = std::make_unique<DomainManager>(config);

    LOG_TRACE("Crawler::Crawler - Creating CrawlMetrics");
    metrics = std::make_unique<CrawlMetrics>();

    LOG_INFO("✅ Crawler initialization completed successfully");
    LOG_DEBUG("Crawler::Crawler - All components initialized and ready for crawling");
}

Crawler::~Crawler() {
    LOG_INFO("🗑️ Crawler::~Crawler - Destroying crawler instance for session: " + sessionId);
    LOG_DEBUG("Crawler::~Crawler - Stopping crawler and cleaning up resources");
    stop();
    LOG_DEBUG("Crawler::~Crawler - Crawler destruction completed");
}

void Crawler::start() {
    LOG_INFO("🚀 Crawler::start - Starting crawler for session: " + sessionId);

    if (isRunning) {
        LOG_WARNING("⚠️ Crawler::start - Crawler already running, ignoring start request");
        LOG_DEBUG("Crawler::start - Current state: isRunning=true, sessionId=" + sessionId);
        return;
    }

    LOG_INFO("🏁 Starting crawler session: " + sessionId);
    logToCrawlSession("Starting crawler", "info");
    LOG_DEBUG("Crawler::start - Logged start event to crawl session");

    // Rehydrate pending tasks from persistent frontier (Mongo) if available
    LOG_DEBUG("Crawler::start - Attempting to rehydrate pending frontier tasks from MongoDB");
    try {
        if (storage && storage->getMongoStorage()) {
            LOG_TRACE("Crawler::start - MongoDB storage available, loading pending tasks");
            auto pending = storage->getMongoStorage()->frontierLoadPending(sessionId, 2000);
            if (pending.success) {
                size_t count = 0;
                LOG_DEBUG("Crawler::start - Processing " + std::to_string(pending.value.size()) + " pending tasks");

                for (const auto& item : pending.value) {
                    const auto& url = item.first;
                    int depth = item.second;
                    urlFrontier->addURL(url, false, CrawlPriority::NORMAL, depth);
                    count++;
                    LOG_TRACE("Crawler::start - Rehydrated URL: " + url + " (depth: " + std::to_string(depth) + ")");
                }

                LOG_INFO("✅ Rehydrated " + std::to_string(count) + " pending frontier tasks from MongoDB");
                LOG_DEBUG("Crawler::start - Frontier restoration completed successfully");
            } else {
                LOG_WARNING("⚠️ Failed to load pending frontier tasks: " + pending.message);
                LOG_DEBUG("Crawler::start - Frontier rehydration failed, starting with empty frontier");
            }
        } else {
            LOG_WARNING("⚠️ No MongoDB storage available for frontier persistence");
            LOG_DEBUG("Crawler::start - Proceeding without frontier persistence");
        }
    } catch (const std::exception& e) {
        LOG_ERROR("💥 Exception during frontier rehydration: " + std::string(e.what()));
        LOG_DEBUG("Crawler::start - Continuing startup despite frontier rehydration failure");
    }

    LOG_DEBUG("Crawler::start - Setting crawler state to running");
    isRunning = true;

    if (workerThread.joinable()) {
        LOG_DEBUG("Crawler::start - Joining existing worker thread before starting new one");
        workerThread.join();
    }

    LOG_DEBUG("Crawler::start - Starting crawler worker thread");
    workerThread = std::thread(&Crawler::crawlLoop, this);
    LOG_INFO("✅ Crawler started successfully - worker thread launched");
    LOG_DEBUG("Crawler::start - Crawler startup sequence completed");
}

void Crawler::stop() {
    LOG_INFO("🛑 Crawler::stop - Stopping crawler for session: " + sessionId);
    LOG_DEBUG("Crawler::stop - Setting isRunning flag to false");

    isRunning = false;

    // Give a small delay to ensure all results are collected
    LOG_DEBUG("Crawler::stop - Waiting 500ms for pending operations to complete");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Wait for worker thread to exit cleanly
    if (workerThread.joinable()) {
        LOG_DEBUG("Crawler::stop - Joining worker thread for clean shutdown");
        workerThread.join();
        LOG_DEBUG("Crawler::stop - Worker thread joined successfully");
    } else {
        LOG_DEBUG("Crawler::stop - No worker thread to join (not running)");
    }

    // Log the final results count
    {
        std::lock_guard<std::mutex> lock(resultsMutex);
        size_t finalCount = results.size();
        LOG_INFO("✅ Crawler stopped successfully. Final results count: " + std::to_string(finalCount));
        LOG_DEBUG("Crawler::stop - Session " + sessionId + " completed with " + std::to_string(finalCount) + " results");
    }

    LOG_DEBUG("Crawler::stop - Crawler shutdown completed");
}

void Crawler::reset() {
    LOG_INFO("🔄 Crawler::reset - Resetting crawler state for session: " + sessionId);

    // Stop crawling if it's running
    if (isRunning) {
        LOG_DEBUG("Crawler::reset - Stopping running crawler before reset");
        stop();
    } else {
        LOG_DEBUG("Crawler::reset - Crawler not running, proceeding with reset");
    }

    // Clear all state
    LOG_DEBUG("Crawler::reset - Clearing results collection and seed domain");
    {
        std::lock_guard<std::mutex> lock(resultsMutex);
        size_t oldCount = results.size();
        results.clear();
        seedDomain.clear();
        LOG_DEBUG("Crawler::reset - Cleared " + std::to_string(oldCount) + " results and seed domain");
    }

    // Reset URL frontier
    if (urlFrontier) {
        LOG_DEBUG("Crawler::reset - Recreating URL frontier");
        urlFrontier = std::make_unique<URLFrontier>();
        LOG_DEBUG("Crawler::reset - URL frontier recreated successfully");
    } else {
        LOG_WARNING("⚠️ Crawler::reset - No URL frontier instance to reset");
    }

    // Reset session-level SPA detection flags
    LOG_DEBUG("Crawler::reset - Resetting session SPA detection flags");
    sessionSpaDetected.store(false);
    sessionSpaChecked.store(false);
    LOG_DEBUG("Crawler::reset - SPA detection flags reset to false");

    LOG_INFO("✅ Crawler state reset completed for session: " + sessionId);
    LOG_DEBUG("Crawler::reset - All crawler state has been reset and is ready for new session");
}

void Crawler::addSeedURL(const std::string& url, bool force) {
    LOG_INFO("Adding seed URL: " + url + (force ? " (force)" : ""));
    logToCrawlSession("Adding seed URL: " + url + (force ? " (force)" : ""), "info");
    
    // Set seed domain if this is the first URL and domain restriction is enabled
    if (config.restrictToSeedDomain && seedDomain.empty()) {
        seedDomain = urlFrontier->extractDomain(url);
        LOG_INFO("Set seed domain to: " + seedDomain);
        logToCrawlSession("Set seed domain to: " + seedDomain, "info");
    }
    
    urlFrontier->addURL(url, force, CrawlPriority::NORMAL, 0); // Seed URLs start at depth 0
    // Add a CrawlResult for this URL with status 'queued'
    CrawlResult result;
    result.url = url;
    result.domain = urlFrontier->extractDomain(url);
    result.crawlStatus = "queued";
    result.queuedAt = std::chrono::system_clock::now();
    {
        std::lock_guard<std::mutex> lock(resultsMutex);
        results.push_back(result);
    }
}

std::vector<CrawlResult> Crawler::getResults() {
    std::lock_guard<std::mutex> lock(resultsMutex);
    LOG_DEBUG("Getting results, count: " + std::to_string(results.size()));
    return results;
}

void Crawler::crawlLoop() {
    LOG_DEBUG("Entering crawl loop with retry support");
    logToCrawlSession("Starting crawl with retry support (max retries: " + std::to_string(config.maxRetries) + ")", "info");
    
    while (isRunning) {
        std::string url = urlFrontier->getNextURL();
        if (url.empty()) {
            // Check if we have pending retries before giving up
            if (urlFrontier->hasReadyURLs() || urlFrontier->pendingRetryCount() > 0) {
                LOG_DEBUG("No ready URLs, but have pending retries. Waiting...");
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                continue;
            }
            
            LOG_INFO("No more URLs to crawl (including retries), exiting crawl loop");
            logToCrawlSession("No more URLs to crawl (including retries), exiting crawl loop", "info");
            isRunning = false;
            break;
        }
        
        if (urlFrontier->isVisited(url)) {
            LOG_DEBUG("URL already visited, skipping: " + url);
            continue;
        }
        
        // Check domain circuit breaker and delays
        std::string domain = urlFrontier->extractDomain(url);
        if (domainManager->isCircuitBreakerOpen(domain)) {
            metrics->recordCircuitBreakerTriggered();
            metrics->recordDomainCircuitBreaker(domain);
            LOG_WARNING("Circuit breaker OPEN for domain " + domain + ", skipping URL: " + url);
            
            // WebSocket log for circuit breaker
            logToCrawlSession("🚨 CIRCUIT BREAKER ACTIVE for " + domain + " - Blocking: " + url, "error");
            continue;
        }
        
        if (domainManager->shouldDelay(domain)) {
            auto delay = domainManager->getDelay(domain);
            LOG_DEBUG("Domain " + domain + " requires delay of " + std::to_string(delay.count()) + "ms, skipping for now");
            
            // Put URL back in queue with delay (if not already scheduled)
            urlFrontier->scheduleRetry(url, 0, "Domain delay required", FailureType::TEMPORARY, delay);
            
            // WebSocket log for domain delays
            logToCrawlSession("⏱️ DOMAIN DELAY " + domain + " - Delaying: " + url + 
                                    " for " + std::to_string(delay.count()) + "ms", "info");
            continue;
        }
        
        // Get existing result info to determine if this is a retry
        QueuedURL queuedInfo = urlFrontier->getQueuedURLInfo(url);
        bool isRetryAttempt = queuedInfo.retryCount > 0;
        
        // Set status to 'downloading' and startedAt
        {
            std::lock_guard<std::mutex> lock(resultsMutex);
            for (auto& r : results) {
                if (r.url == url && (r.crawlStatus == "queued" || r.crawlStatus == "failed")) {
                    r.crawlStatus = "downloading";
                    r.startedAt = std::chrono::system_clock::now();
                    r.retryCount = queuedInfo.retryCount;
                    r.isRetryAttempt = isRetryAttempt;
                    
                    std::string logMessage = isRetryAttempt ? 
                        "Started retry attempt " + std::to_string(queuedInfo.retryCount) + " for: " + url :
                        "Started downloading: " + url;
                    logToCrawlSession(logMessage, "info");
                }
            }
        }
        
        auto processStartTime = std::chrono::steady_clock::now();
        CrawlResult result = processURL(url);
        auto processEndTime = std::chrono::steady_clock::now();
        
        result.domain = urlFrontier->extractDomain(url);
        
        // Use the startedAt time from the processURL result
        // The startedAt is now set at the beginning of processURL
        LOG_DEBUG("Using timing from processURL result for URL: " + url + 
                  " - startedAt: " + std::to_string(result.startedAt.time_since_epoch().count()) + 
                  " - finishedAt: " + std::to_string(result.finishedAt.time_since_epoch().count()));
        
        // Ensure timing is set even if not found in results vector
        if (result.startedAt.time_since_epoch().count() == 0) {
            LOG_WARNING("startedAt not found in processURL result for URL: " + url + ", using processStartTime");
            result.startedAt = std::chrono::system_clock::now() - (processEndTime - processStartTime);
        }
        
        // Record metrics for this request (after domain is set)
        metrics->recordRequest();
        metrics->recordDomainRequest(result.domain);
        result.retryCount = queuedInfo.retryCount;
        result.isRetryAttempt = isRetryAttempt;
        
        // Calculate total retry time
        auto processingTime = std::chrono::duration_cast<std::chrono::milliseconds>(processEndTime - processStartTime);
        result.totalRetryTime = processingTime;
        
        // Handle retry logic
        if (!result.success) {
            bool shouldRetry = FailureClassifier::shouldRetry(
                result.failureType, 
                result.retryCount, 
                config.maxRetries
            );
            
            if (shouldRetry) {
                // Record retry metrics
                metrics->recordRetry();
                metrics->recordDomainRetry(result.domain);
                metrics->recordFailureType(result.failureType);
                
                // Schedule retry with exponential backoff
                auto retryDelay = FailureClassifier::calculateRetryDelay(
                    result.retryCount + 1, 
                    config, 
                    result.failureType
                );
                
                urlFrontier->scheduleRetry(
                    url, 
                    result.retryCount + 1, 
                    result.errorMessage.value_or("Unknown error"), 
                    result.failureType, 
                    retryDelay
                );
                
                result.crawlStatus = "retry_scheduled";
                
                // Create detailed retry reason message
                std::string retryReason;
                if (result.statusCode > 0) {
                    retryReason = "HTTP " + std::to_string(result.statusCode);
                    if (result.statusCode == 429) {
                        retryReason += " (Rate Limited)";
                    } else if (result.statusCode >= 500) {
                        retryReason += " (Server Error)";
                    } else if (result.statusCode == 408) {
                        retryReason += " (Request Timeout)";
                    }
                } else if (result.curlErrorCode != CURLE_OK) {
                    retryReason = "Network Error";
                    if (result.curlErrorCode == CURLE_OPERATION_TIMEDOUT) {
                        retryReason += " (Timeout)";
                    } else if (result.curlErrorCode == CURLE_COULDNT_CONNECT) {
                        retryReason += " (Connection Failed)";
                    } else if (result.curlErrorCode == CURLE_COULDNT_RESOLVE_HOST) {
                        retryReason += " (DNS Failed)";
                    }
                } else {
                    retryReason = "Unknown Error";
                }
                
                // Add failure type classification
                retryReason += " [" + FailureClassifier::getFailureTypeDescription(result.failureType) + "]";
                
                LOG_INFO("Scheduled retry " + std::to_string(result.retryCount + 1) + "/" + 
                        std::to_string(config.maxRetries) + " for: " + url + 
                        " in " + std::to_string(retryDelay.count()) + "ms - " + retryReason);
                
                // Enhanced WebSocket log with detailed retry information
                std::string wsMessage = "🔄 RETRY " + std::to_string(result.retryCount + 1) + "/" + 
                                      std::to_string(config.maxRetries) + " scheduled for " + url + 
                                      " - Reason: " + retryReason + 
                                      " | Delay: " + std::to_string(retryDelay.count()) + "ms" +
                                      " | Domain: " + result.domain;
                
                logToCrawlSession(wsMessage, "warning");
            } else {
                result.crawlStatus = "failed";
                urlFrontier->markVisited(url); // Mark as visited to prevent further processing
                
                // Record final failure metrics
                metrics->recordFailure();
                metrics->recordPermanentFailure();
                metrics->recordDomainFailure(result.domain);
                metrics->recordFailureType(result.failureType);
                
                // Record failure in domain manager (only for final failures)
                if (result.failureType == FailureType::RATE_LIMITED) {
                    // Special handling for rate limiting
                    metrics->recordRateLimit();
                    metrics->recordDomainRateLimit(result.domain);
                    domainManager->recordRateLimit(result.domain);
                } else {
                    domainManager->recordFailure(result.domain, result.failureType, 
                                               result.errorMessage.value_or("Unknown error"));
                }
                
                std::string reason = (result.retryCount >= config.maxRetries) ? 
                    "max retries exceeded" : 
                    "permanent failure (" + FailureClassifier::getFailureTypeDescription(result.failureType) + ")";
                
                // Create detailed final failure message
                std::string finalFailureReason;
                if (result.statusCode > 0) {
                    finalFailureReason = "HTTP " + std::to_string(result.statusCode);
                } else if (result.curlErrorCode != CURLE_OK) {
                    finalFailureReason = "Network Error (Code: " + std::to_string(static_cast<int>(result.curlErrorCode)) + ")";
                } else {
                    finalFailureReason = "Unknown Error";
                }
                
                LOG_WARNING("Giving up on URL: " + url + " - " + reason);
                
                // Enhanced WebSocket log for final failures
                std::string wsMessage = "❌ FAILED (Final) " + url + 
                                      " - " + finalFailureReason + 
                                      " | Reason: " + reason + 
                                      " | Attempts: " + std::to_string(result.retryCount + 1) +
                                      " | Domain: " + result.domain;
                
                logToCrawlSession(wsMessage, "error");
            }
        } else {
            result.crawlStatus = "downloaded";
            urlFrontier->markVisited(url); // Mark as visited on success
            
            // Record success metrics
            metrics->recordSuccess();
            metrics->recordDomainSuccess(result.domain);
            
            // Record success in domain manager
            domainManager->recordSuccess(result.domain);
            
            std::string successMessage = isRetryAttempt ? 
                "Successfully downloaded on retry attempt " + std::to_string(result.retryCount) + ": " + url :
                "Successfully downloaded: " + url;
            
            LOG_INFO(successMessage + " (Status: " + std::to_string(result.statusCode) + ")");
            
            // Enhanced WebSocket log for successful downloads
            std::string wsMessage;
            // Calculate download time for display
            std::string downloadTimeStr = "";
            if (result.startedAt.time_since_epoch().count() > 0 && result.finishedAt.time_since_epoch().count() > 0) {
                auto downloadDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    result.finishedAt - result.startedAt);
                downloadTimeStr = " | Time: " + std::to_string(downloadDuration.count()) + "ms";
            }
            
            if (isRetryAttempt) {
                wsMessage = "✅ SUCCESS (Retry " + std::to_string(result.retryCount) + ") " + url + 
                           " - HTTP " + std::to_string(result.statusCode) + 
                           " | Domain: " + result.domain +
                           " | Size: " + std::to_string(result.contentSize) + " bytes" +
                           downloadTimeStr;
            } else {
                wsMessage = "✅ SUCCESS " + url + 
                           " - HTTP " + std::to_string(result.statusCode) + 
                           " | Domain: " + result.domain +
                           " | Size: " + std::to_string(result.contentSize) + " bytes" +
                           downloadTimeStr;
            }
            
            logToCrawlSession(wsMessage, "info");
        }
        
        // Store the result (replace the old one for this URL)
        {
            std::lock_guard<std::mutex> lock(resultsMutex);
            auto it = std::find_if(results.begin(), results.end(), [&](const CrawlResult& r) { return r.url == url; });
            if (it != results.end()) {
                *it = result;
            } else {
                results.push_back(result);
            }
            LOG_INFO("Updated result for URL: " + url + ", total results: " + std::to_string(results.size()));
        }
        
        if (storage) {
            auto storeResult = storage->storeCrawlResult(result);
            if (storeResult.success) {
                LOG_INFO("Successfully saved crawl result to database for URL: " + url);
                logToCrawlSession("Saved to database: " + url, "info");
            } else {
                LOG_ERROR("Failed to save crawl result to database for URL: " + url + " - " + storeResult.message);
                logToCrawlSession("Database save failed for: " + url + " - " + storeResult.message, "error");
            }
            // Store crawl log for history
            search_engine::storage::CrawlLog log;
            log.url = result.url;
            log.domain = urlFrontier->extractDomain(result.url);
            log.crawlTime = result.crawlTime;
            log.status = result.success ? "SUCCESS" : "FAILED";
            log.httpStatusCode = result.statusCode;
            log.errorMessage = result.errorMessage;
            log.contentSize = result.contentSize;
            log.contentType = result.contentType;
            log.links = result.links;
            log.title = result.title;
            log.description = result.metaDescription;
            // Calculate and store download time in milliseconds
            if (result.startedAt.time_since_epoch().count() > 0 && result.finishedAt.time_since_epoch().count() > 0) {
                auto downloadDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    result.finishedAt - result.startedAt);
                log.downloadTimeMs = downloadDuration.count();
                LOG_DEBUG("Crawl log timing for URL: " + url + " - downloadTimeMs: " + std::to_string(*log.downloadTimeMs));
            } else {
                LOG_WARNING("Missing timing data for URL: " + url + 
                           " - startedAt: " + std::to_string(result.startedAt.time_since_epoch().count()) + 
                           " - finishedAt: " + std::to_string(result.finishedAt.time_since_epoch().count()));
            }
            auto logResult = storage->storeCrawlLog(log);
            if (logResult.success) {
                LOG_INFO("Crawl log entry saved for URL: " + url);
            } else {
                LOG_ERROR("Failed to save crawl log for URL: " + url + " - " + logResult.message);
            }
        } else {
            LOG_WARNING("No storage configured, crawl result not saved to database for URL: " + url);
        }
        
        // Check if we've reached the maximum pages limit (count only successful downloads)
        size_t successfulDownloads = 0;
        {
            std::lock_guard<std::mutex> lock(resultsMutex);
            for (const auto& r : results) {
                if (r.success && r.crawlStatus == "downloaded") {
                    successfulDownloads++;
                }
            }
        }
        
        if (successfulDownloads >= config.maxPages) {
            LOG_INFO("Reached maximum pages limit (" + std::to_string(config.maxPages) + " successful downloads), stopping crawler");
            logToCrawlSession("Reached maximum pages limit (" + std::to_string(config.maxPages) + " successful downloads), stopping crawler", "warning");
            isRunning = false;
            break;
        }
        
        // Brief pause to prevent CPU spinning
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    // Log comprehensive metrics before exiting
    auto retryStats = urlFrontier->getRetryStats();
    LOG_INFO("Crawl loop finished. Retry stats - Total: " + std::to_string(retryStats.totalRetries) + 
             ", Pending: " + std::to_string(retryStats.pendingRetries));
    
    // Log comprehensive metrics summary
    if (metrics) {
        metrics->logSummary();
    }
    
    logToCrawlSession("Crawl completed - check logs for detailed metrics", "info");
    
    LOG_DEBUG("Exiting crawl loop with retry support");
}

CrawlResult Crawler::processURL(const std::string& url) {
    LOG_INFO("🚀🚀🚀 BINARY UPDATE TEST - NEW VERSION LOADED 🚀🚀🚀");
    LOG_DEBUG("[processURL] Called with url: " + url);
    CrawlResult result;
    result.url = url;
    result.crawlTime = std::chrono::system_clock::now();
    
    // Set startedAt time at the beginning of processURL to ensure timing is captured
    auto processStartTime = std::chrono::system_clock::now();
    result.startedAt = processStartTime;
    
    LOG_DEBUG("[processURL] Initialized CrawlResult with startedAt: " + std::to_string(result.startedAt.time_since_epoch().count()));
    
    LOG_INFO("Processing URL: " + url);
    
    // Check if URL is allowed by robots.txt
    if (config.respectRobotsTxt) {
        std::string domain = urlFrontier->extractDomain(url);
        LOG_DEBUG("[processURL] Extracted domain: " + domain);
        if (!robotsParser->isAllowed(url, config.userAgent)) {
            result.success = false;
            result.errorMessage = "URL not allowed by robots.txt";
            LOG_WARNING("URL not allowed by robots.txt: " + url);
            return result;
        }
        
        // Respect crawl delay
        auto lastVisit = urlFrontier->getLastVisitTime(domain);
        auto crawlDelay = robotsParser->getCrawlDelay(domain, config.userAgent);
        auto timeSinceLastVisit = std::chrono::system_clock::now() - lastVisit;
        LOG_DEBUG("[processURL] lastVisit: " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(lastVisit.time_since_epoch()).count()) + ", crawlDelay: " + std::to_string(crawlDelay.count()) + ", timeSinceLastVisit: " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(timeSinceLastVisit).count()));
        // For testing purposes, completely disable crawl delay
        LOG_DEBUG("NOTE: Crawl delay disabled for testing");
        // Only sleep for a very short time for testing purposes
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Fetch the page
    LOG_INFO("Fetching page: " + url);
    auto fetchResult = pageFetcher->fetch(url);
    LOG_DEBUG("[processURL] fetchResult.statusCode: " + std::to_string(fetchResult.statusCode));
    LOG_DEBUG("[processURL] fetchResult.contentType: " + fetchResult.contentType);
    LOG_DEBUG("[processURL] fetchResult.content (first 200): " + (fetchResult.content.size() > 200 ? fetchResult.content.substr(0, 200) + "..." : fetchResult.content));
    
    // Get CURL error code directly from fetch result
    CURLcode curlErrorCode = fetchResult.curlCode;

    // Session-level SPA detection: only check once per session
    bool shouldUseSpaRendering = false;
    
    if (!sessionSpaChecked.load()) {
        // First URL in session - check if it's an SPA
        if (pageFetcher->isSpaPage(fetchResult.content, url)) {
            LOG_INFO("SPA detected for first URL in session: " + url + ". Enabling SPA rendering for entire session.");
            sessionSpaDetected.store(true);
            sessionSpaChecked.store(true);
            shouldUseSpaRendering = true;
            
            // Enable SPA rendering for the entire session
            pageFetcher->setSpaRendering(true, config.browserlessUrl, /*useWebsocket=*/config.useWebsocketForBrowserless, /*wsConnectionsPerCpu=*/config.wsConnectionsPerCpu);
            logToCrawlSession("SPA detected for session - enabling SPA rendering for all URLs", "info");
        } else {
            LOG_INFO("No SPA detected for first URL in session: " + url + ". SPA rendering disabled for session.");
            sessionSpaDetected.store(false);
            sessionSpaChecked.store(true);
        }
    } else if (sessionSpaDetected.load()) {
        // SPA was already detected for this session, use SPA rendering
        shouldUseSpaRendering = true;
    }
    
    // If SPA rendering is enabled for the session, fetch with headless browser
    if (shouldUseSpaRendering) {
        LOG_INFO("Using SPA rendering for URL: " + url + " (session-level SPA detected)");
        auto spaFetchResult = pageFetcher->fetch(url);
        LOG_DEBUG("[processURL] spaFetchResult.statusCode: " + std::to_string(spaFetchResult.statusCode));
        LOG_DEBUG("[processURL] spaFetchResult.contentType: " + spaFetchResult.contentType);
        LOG_DEBUG("[processURL] spaFetchResult.content (first 200): " + (spaFetchResult.content.size() > 200 ? spaFetchResult.content.substr(0, 200) + "..." : spaFetchResult.content));
        if (spaFetchResult.success && !spaFetchResult.content.empty()) {
            LOG_INFO("Successfully fetched SPA-rendered HTML for URL: " + url);
            fetchResult = spaFetchResult;
        } else {
            LOG_WARNING("Failed to fetch SPA-rendered HTML for URL: " + url + ". Using original content.");
        }
    }
    
    // Set finishedAt time after all fetching is complete
    result.finishedAt = std::chrono::system_clock::now();
    LOG_DEBUG("[processURL] SPA timing for URL: " + url + 
              " - startedAt: " + std::to_string(result.startedAt.time_since_epoch().count()) + 
              " - finishedAt: " + std::to_string(result.finishedAt.time_since_epoch().count()) +
              " - SPA rendering used: " + (shouldUseSpaRendering ? "true" : "false"));
    
    // Always store the result data, regardless of status code
    result.statusCode = fetchResult.statusCode;
    result.contentType = fetchResult.contentType;
    result.contentSize = fetchResult.content.size();
    result.finalUrl = fetchResult.finalUrl;  // Store the final URL after redirects
    LOG_DEBUG("[processURL] Stored result status, contentType, contentSize, finalUrl");
    
    // Store raw content based on includeFullContent setting (similar to SPA render API)
    if (config.storeRawContent) {
        if (config.includeFullContent) {
            // Store full content when includeFullContent is enabled
            result.rawContent = fetchResult.content;
            LOG_DEBUG("[processURL] Stored full rawContent (includeFullContent=true)");
        } else {
            // Store only a preview when includeFullContent is disabled (like SPA render API)
            std::string preview = fetchResult.content.substr(0, 500);
            if (fetchResult.content.size() > 500) preview += "...";
            result.rawContent = preview;
            LOG_DEBUG("[processURL] Stored rawContent preview (includeFullContent=false)");
        }
    }
    
    LOG_INFO("=== HTTP STATUS: " + std::to_string(fetchResult.statusCode) + " === for URL: " + url);
    
    // Log the final URL if it's different from the original
    if (!fetchResult.finalUrl.empty() && fetchResult.finalUrl != url) {
        LOG_INFO("Final URL after redirects: " + fetchResult.finalUrl);
    }
    
    // Check if the fetch was successful (2xx status codes)
    if (fetchResult.statusCode >= 200 && fetchResult.statusCode < 300) {
        result.success = true;
        LOG_INFO("Page fetched successfully: " + url + " Status: " + std::to_string(fetchResult.statusCode));
    } else {
        result.success = false;
        
        // Classify the failure for potential retry
        FailureType failureType = FailureClassifier::classifyFailure(
            fetchResult.statusCode, 
            curlErrorCode, 
            fetchResult.errorMessage.empty() ? "Unknown error" : fetchResult.errorMessage,
            config
        );
        
        // Store failure classification info in result for retry logic
        result.curlErrorCode = curlErrorCode;
        result.failureType = failureType;
        
        if (fetchResult.statusCode >= 300 && fetchResult.statusCode < 400) {
            result.errorMessage = "HTTP Redirect: " + std::to_string(fetchResult.statusCode);
            LOG_INFO("HTTP REDIRECT: Status " + std::to_string(fetchResult.statusCode) + " for URL: " + url + 
                    " (Failure type: " + FailureClassifier::getFailureTypeDescription(failureType) + ")");
        } else if (fetchResult.statusCode >= 400) {
            result.errorMessage = "HTTP Error: " + std::to_string(fetchResult.statusCode);
            LOG_WARNING("HTTP ERROR: Status " + std::to_string(fetchResult.statusCode) + " for URL: " + url + 
                       " (Failure type: " + FailureClassifier::getFailureTypeDescription(failureType) + ")");
        } else if (!fetchResult.errorMessage.empty()) {
            result.errorMessage = fetchResult.errorMessage;
            LOG_ERROR("Failed to fetch page: " + url + " Error: " + fetchResult.errorMessage + 
                     " (Failure type: " + FailureClassifier::getFailureTypeDescription(failureType) + ")");
        } else {
            result.errorMessage = "Unknown error (status: " + std::to_string(fetchResult.statusCode) + ")";
            LOG_ERROR("Failed to fetch page: " + url + " - Unknown error" + 
                     " (Failure type: " + FailureClassifier::getFailureTypeDescription(failureType) + ")");
        }
    }
    
    // Parse the content if it's HTML, regardless of status code
    if (fetchResult.contentType.find("text/html") != std::string::npos && !fetchResult.content.empty()) {
        LOG_INFO("🔍 TEXTCONTENT DEBUG: Content is HTML, parsing... Content-Type: " + fetchResult.contentType);
        auto parsedContent = contentParser->parse(fetchResult.content, url);
        LOG_INFO("🔍 TEXTCONTENT DEBUG: Parsed title: " + parsedContent.title);
        LOG_INFO("🔍 TEXTCONTENT DEBUG: Parsed textContent length: " + std::to_string(parsedContent.textContent.size()));
        LOG_INFO("🔍 TEXTCONTENT DEBUG: extractTextContent config: " + std::string(config.extractTextContent ? "true" : "false"));
        if (config.extractTextContent) {
            result.textContent = parsedContent.textContent;
            LOG_INFO("🔍 TEXTCONTENT DEBUG: ✅ STORED textContent with length: " + std::to_string(result.textContent ? result.textContent->size() : 0));
        } else {
            LOG_INFO("🔍 TEXTCONTENT DEBUG: ❌ NOT storing textContent - config disabled");
        }
        result.title = parsedContent.title;
        result.metaDescription = parsedContent.metaDescription;
        // Get current URL's depth for link extraction
        QueuedURL queuedInfo = urlFrontier->getQueuedURLInfo(url);
        int currentDepth = queuedInfo.depth;
        
        // Check if we should extract links based on current progress
        size_t currentSuccessfulDownloads = 0;
        {
            std::lock_guard<std::mutex> lock(resultsMutex);
            for (const auto& r : results) {
                if (r.success && r.crawlStatus == "downloaded") {
                    currentSuccessfulDownloads++;
                }
            }
        }
        
        // Only extract links if we haven't reached the limit and don't have too many queued
        size_t queueSize = urlFrontier->size() + urlFrontier->retryQueueSize();
        size_t totalQueued = currentSuccessfulDownloads + queueSize;
        
        // More permissive link extraction - only skip if we have significantly more URLs than needed
        if (currentSuccessfulDownloads < config.maxPages && totalQueued < config.maxPages * 3) {
            extractAndAddURLs(fetchResult.content, url, currentDepth);
        } else {
            LOG_INFO("Skipping link extraction - already have " + std::to_string(currentSuccessfulDownloads) + 
                    " downloads and " + std::to_string(queueSize) + " queued URLs (total: " + 
                    std::to_string(totalQueued) + ", limit: " + std::to_string(config.maxPages * 3) + ")");
        }
    } else {
        LOG_INFO("🔍 TEXTCONTENT DEBUG: ❌ Content is NOT HTML, skipping parsing. Content-Type: " + fetchResult.contentType);
    }
    
    // CRITICAL DEBUG: Log the contentType to see why HTML parsing is skipped
    LOG_INFO("🔍 CONTENTTYPE DEBUG: fetchResult.contentType = '" + fetchResult.contentType + "'");
    LOG_INFO("🔍 CONTENTTYPE DEBUG: content.empty() = " + std::string(fetchResult.content.empty() ? "true" : "false"));
    LOG_INFO("🔍 CONTENTTYPE DEBUG: content.size() = " + std::to_string(fetchResult.content.size()));
    
    LOG_INFO("URL processed successfully: " + url);
    return result;
}

void Crawler::extractAndAddURLs(const std::string& content, const std::string& baseUrl, int currentDepth) {
    // Check if we've reached the maximum depth limit
    size_t nextDepth = static_cast<size_t>(currentDepth + 1);
    if (nextDepth > config.maxDepth) {
        LOG_INFO("Reached maximum depth limit (" + std::to_string(config.maxDepth) + 
                "), skipping link extraction from: " + baseUrl);
        return;
    }
    
    // Check if we've already reached the pages limit
    size_t currentSuccessfulDownloads = 0;
    {
        std::lock_guard<std::mutex> lock(resultsMutex);
        for (const auto& r : results) {
            if (r.success && r.crawlStatus == "downloaded") {
                currentSuccessfulDownloads++;
            }
        }
    }
    
    if (currentSuccessfulDownloads >= config.maxPages) {
        LOG_INFO("Already reached maximum pages limit (" + std::to_string(config.maxPages) + 
                "), skipping link extraction from: " + baseUrl);
        return;
    }
    
    // More permissive queue size check - only stop if we have significantly more URLs than needed
    // This accounts for potential failures and ensures we have enough URLs to reach maxPages
    size_t queueSize = urlFrontier->size() + urlFrontier->retryQueueSize();
    size_t totalQueued = currentSuccessfulDownloads + queueSize;
    
    // Only stop adding if we have more than 3x maxPages in total (allowing for failures)
    if (totalQueued >= config.maxPages * 3) {
        LOG_INFO("Queue has sufficient URLs to reach maxPages with failure margin (" + 
                std::to_string(currentSuccessfulDownloads) + " downloaded + " + 
                std::to_string(queueSize) + " queued = " + std::to_string(totalQueued) + 
                " >= " + std::to_string(config.maxPages * 3) + "), skipping link extraction from: " + baseUrl);
        return;
    }
    
    auto links = contentParser->extractLinks(content, baseUrl);
    LOG_DEBUG("Extracted " + std::to_string(links.size()) + " links from " + baseUrl + 
              " at depth " + std::to_string(currentDepth) + " (next depth would be " + 
              std::to_string(nextDepth) + ")");
    
    size_t addedCount = 0;
    size_t skippedCount = 0;
    size_t depthSkippedCount = 0;
    size_t pagesLimitSkippedCount = 0;
    
    // More permissive frontier cap - allow up to 5x maxPages to ensure we have enough URLs
    const size_t frontierCap = std::max<size_t>(config.maxPages * 5, 50);
    for (const auto& link : links) {
        // Check current successful downloads and queue size
        size_t queueSize = urlFrontier->size() + urlFrontier->retryQueueSize();
        size_t totalQueued = currentSuccessfulDownloads + queueSize;
        (void)totalQueued; // Suppress unused variable warning
        
        // Only be restrictive when we're very close to maxPages in successful downloads
        if (currentSuccessfulDownloads >= config.maxPages * 0.9) {
            LOG_DEBUG("Very close to maxPages limit (" + std::to_string(currentSuccessfulDownloads) + 
                      "/" + std::to_string(config.maxPages) + ") - skipping URL: " + link);
            pagesLimitSkippedCount++;
            continue;
        }
        
        // Throttle when frontier gets very large
        if (queueSize >= frontierCap) {
            LOG_DEBUG("Frontier size (" + std::to_string(queueSize) +
                      ") reached cap (" + std::to_string(frontierCap) + ") - throttling additions, skipping: " + link);
            pagesLimitSkippedCount++;
            continue;
        }
        
        // Check if URL is allowed by robots.txt
        if (!config.respectRobotsTxt || robotsParser->isAllowed(link, config.userAgent)) {
            // Check domain restriction if enabled
            if (config.restrictToSeedDomain && !isSameDomain(link)) {
                LOG_DEBUG("Skipping URL from different domain: " + link);
                skippedCount++;
                continue;
            }
            
            // Add URL with proper depth tracking
            urlFrontier->addURL(link, false, CrawlPriority::NORMAL, nextDepth);
            
            // Also add to results vector for timing tracking
            {
                std::lock_guard<std::mutex> lock(resultsMutex);
                // Check if URL already exists in results
                auto it = std::find_if(results.begin(), results.end(), [&](const CrawlResult& r) { return r.url == link; });
                if (it == results.end()) {
                    CrawlResult result;
                    result.url = link;
                    result.domain = urlFrontier->extractDomain(link);
                    result.crawlStatus = "queued";
                    result.queuedAt = std::chrono::system_clock::now();
                    results.push_back(result);
                }
            }
            
            addedCount++;
        }
    }
    
    LOG_INFO("Added " + std::to_string(addedCount) + " URLs to frontier at depth " + 
             std::to_string(nextDepth) + ", skipped " + std::to_string(skippedCount) + 
             " URLs (domain restriction: " + (config.restrictToSeedDomain ? "enabled" : "disabled") + 
             "), " + std::to_string(depthSkippedCount) + " (depth limit), " + 
             std::to_string(pagesLimitSkippedCount) + " (pages limit)");
}

bool Crawler::isSameDomain(const std::string& url) const {
    if (seedDomain.empty()) {
        // If no seed domain is set, allow all URLs
        return true;
    }
    
    std::string urlDomain = urlFrontier->extractDomain(url);
    
    // Enhanced domain matching - handle www prefix and subdomain variations
    auto normalizeForComparison = [](const std::string& domain) -> std::string {
        std::string normalized = domain;
        // Convert to lowercase
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
        // Remove www. prefix for comparison
        if (normalized.substr(0, 4) == "www.") {
            normalized = normalized.substr(4);
        }
        return normalized;
    };
    
    std::string normalizedUrlDomain = normalizeForComparison(urlDomain);
    std::string normalizedSeedDomain = normalizeForComparison(seedDomain);
    
    bool isSame = (normalizedUrlDomain == normalizedSeedDomain);
    
    LOG_DEBUG("Domain check - URL: " + url + ", URL domain: " + urlDomain + 
              " (normalized: " + normalizedUrlDomain + "), Seed domain: " + seedDomain + 
              " (normalized: " + normalizedSeedDomain + "), Same domain: " + (isSame ? "yes" : "no"));
    
    return isSame;
}

PageFetcher* Crawler::getPageFetcher() {
    return pageFetcher.get();
}

void Crawler::setMaxPages(size_t maxPages) {
    std::lock_guard<std::mutex> lock(resultsMutex);
    config.maxPages = maxPages;
    LOG_INFO("Updated maxPages to: " + std::to_string(maxPages));
}

void Crawler::setMaxDepth(size_t maxDepth) {
    std::lock_guard<std::mutex> lock(resultsMutex);
    config.maxDepth = maxDepth;
    LOG_INFO("Updated maxDepth to: " + std::to_string(maxDepth));
}

void Crawler::updateConfig(const CrawlConfig& newConfig) {
    std::lock_guard<std::mutex> lock(resultsMutex);
    config = newConfig;
    LOG_INFO("Updated crawler configuration - maxPages: " + std::to_string(config.maxPages) + 
             ", maxDepth: " + std::to_string(config.maxDepth) + 
             ", restrictToSeedDomain: " + (config.restrictToSeedDomain ? "true" : "false") + 
             ", followRedirects: " + (config.followRedirects ? "true" : "false") + 
             ", maxRedirects: " + std::to_string(config.maxRedirects));
    
    // Update PageFetcher configuration
    updatePageFetcherConfig();
    
    // Update DomainManager configuration
    if (domainManager) {
        domainManager->updateConfig(newConfig);
    }
}

void Crawler::updatePageFetcherConfig() {
    if (pageFetcher) {
        // Recreate PageFetcher with new configuration
        pageFetcher = std::make_unique<PageFetcher>(
            config.userAgent,
            config.requestTimeout,
            config.followRedirects,
            config.maxRedirects
        );
        
        // Preserve SPA rendering settings from config
        if (config.spaRenderingEnabled) {
            pageFetcher->setSpaRendering(true, config.browserlessUrl);
            LOG_INFO("Enabled SPA rendering with browserless URL: " + config.browserlessUrl);
        }
        
        // Preserve SSL verification settings (typically disabled for problematic sites)
        pageFetcher->setVerifySSL(false);
        
        LOG_INFO("Updated PageFetcher configuration");
    }
}

CrawlConfig Crawler::getConfig() const {
    std::lock_guard<std::mutex> lock(resultsMutex);
    return config;
}

void Crawler::logToCrawlSession(const std::string& message, const std::string& level) const {
    if (!sessionId.empty()) {
        CrawlLogger::broadcastSessionLog(sessionId, message, level);
    } else {
        CrawlLogger::broadcastLog(message, level);
    }
} 