#include "CrawlerManager.h"
#include "../../include/Logger.h"
#include "../../include/crawler/CrawlLogger.h"
#include "PageFetcher.h"
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>

CrawlerManager::CrawlerManager(std::shared_ptr<search_engine::storage::ContentStorage> storage)
    : storage_(storage) {
    LOG_INFO("CrawlerManager initialized");
    
    // Start background cleanup thread
    cleanupThread_ = std::thread(&CrawlerManager::cleanupWorker, this);
}

CrawlerManager::~CrawlerManager() {
    LOG_INFO("CrawlerManager shutting down");
    
    // Signal cleanup thread to stop
    shouldStop_ = true;
    
    // Wait for cleanup thread to finish
    if (cleanupThread_.joinable()) {
        cleanupThread_.join();
    }
    
    // Stop all active crawls
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    for (auto& [sessionId, session] : sessions_) {
        if (session->crawler) {
            session->crawler->stop();
        }
        if (session->crawlThread.joinable()) {
            session->crawlThread.join();
        }
    }
    sessions_.clear();
    
    LOG_INFO("CrawlerManager shutdown complete");
}

std::string CrawlerManager::startCrawl(const std::string& url, const CrawlConfig& config, bool force, CrawlCompletionCallback completionCallback) {
    std::string sessionId = generateSessionId();
    
    LOG_INFO("Starting new crawl session: " + sessionId + " for URL: " + url);
    CrawlLogger::broadcastSessionLog(sessionId, "Starting new crawl session for URL: " + url, "info");
    
    try {
        // Create new crawler instance with the provided configuration
        auto crawler = createCrawler(config, sessionId);
        
        // Create crawl session with completion callback
        auto session = std::make_unique<CrawlSession>(sessionId, std::move(crawler), std::move(completionCallback));
        
        // Add seed URL to the crawler
        session->crawler->addSeedURL(url, force);
        
        // Start crawling in a separate thread
        session->crawlThread = std::thread([sessionId, this]() {
            std::unique_lock<std::mutex> lock(sessionsMutex_);
            auto it = sessions_.find(sessionId);
            if (it == sessions_.end()) {
                LOG_ERROR("Session not found during crawl start: " + sessionId);
                return;
            }
            
            auto& session = it->second;
            auto crawler = session->crawler.get();
            lock.unlock();
            
            try {
                LOG_INFO("Starting crawler for session: " + sessionId);
                CrawlLogger::broadcastLog("Starting crawler for session: " + sessionId, "info");
                crawler->start();
                
                // Wait for crawling to complete
                // The crawler will set isRunning to false when done
                while (crawler->getConfig().maxPages > 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                    
                    // Check if we should stop
                    lock.lock();
                    if (sessions_.find(sessionId) == sessions_.end()) {
                        lock.unlock();
                        break;
                    }
                    lock.unlock();
                    
                    // Check if crawler is still running by looking at results
                    auto results = crawler->getResults();
                    bool hasActiveDownloads = false;
                    size_t successfulDownloads = 0;
                    for (const auto& result : results) {
                        if (result.crawlStatus == "queued" || result.crawlStatus == "downloading") {
                            hasActiveDownloads = true;
                        }
                        if (result.success && result.crawlStatus == "downloaded") {
                            successfulDownloads++;
                        }
                    }
                    
                    if (!hasActiveDownloads && !results.empty()) {
                        LOG_INFO("No more active downloads for session: " + sessionId);
                        CrawlLogger::broadcastLog("No more active downloads for session: " + sessionId, "info");
                        break;
                    }
                    
                    if (successfulDownloads >= crawler->getConfig().maxPages) {
                        LOG_INFO("Reached max pages for session: " + sessionId + " (successful downloads: " + std::to_string(successfulDownloads) + ")");
                        break;
                    }
                }
                
                LOG_INFO("Crawl completed for session: " + sessionId);
                CrawlLogger::broadcastLog("Crawl completed for session: " + sessionId, "info");
                
            } catch (const std::exception& e) {
                LOG_ERROR("Error in crawl thread for session " + sessionId + ": " + e.what());
                CrawlLogger::broadcastLog("Error in crawl thread for session " + sessionId + ": " + e.what(), "error");
            }
            
            // Mark session as completed and execute completion callback
            lock.lock();
            auto sessionIt = sessions_.find(sessionId);
            if (sessionIt != sessions_.end()) {
                auto& completedSession = sessionIt->second;
                completedSession->isCompleted = true;
                
                // Execute completion callback if provided
                if (completedSession->completionCallback) {
                    LOG_INFO("Executing completion callback for session: " + sessionId);
                    try {
                        auto results = completedSession->crawler->getResults();
                        completedSession->completionCallback(sessionId, results, this);
                        LOG_INFO("Completion callback executed successfully for session: " + sessionId);
                    } catch (const std::exception& e) {
                        LOG_ERROR("Error executing completion callback for session " + sessionId + ": " + e.what());
                    }
                }
            }
            lock.unlock();
        });
        
        // Store session
        {
            std::lock_guard<std::mutex> lock(sessionsMutex_);
            sessions_[sessionId] = std::move(session);
        }
        
        LOG_INFO("Crawl session started successfully: " + sessionId);
        return sessionId;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to start crawl session: " + std::string(e.what()));
        throw;
    }
}

std::vector<CrawlResult> CrawlerManager::getCrawlResults(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    
    auto it = sessions_.find(sessionId);
    if (it == sessions_.end()) {
        LOG_WARNING("Session not found: " + sessionId);
        return {};
    }
    
    return it->second->crawler->getResults();
}

std::string CrawlerManager::getCrawlStatus(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    
    auto it = sessions_.find(sessionId);
    if (it == sessions_.end()) {
        return "not_found";
    }
    
    if (it->second->isCompleted) {
        return "completed";
    }
    
    auto results = it->second->crawler->getResults();
    if (results.empty()) {
        return "starting";
    }
    
    bool hasActive = false;
    for (const auto& result : results) {
        if (result.crawlStatus == "queued" || result.crawlStatus == "downloading") {
            hasActive = true;
            break;
        }
    }
    
    return hasActive ? "crawling" : "completed";
}

bool CrawlerManager::stopCrawl(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    
    auto it = sessions_.find(sessionId);
    if (it == sessions_.end()) {
        return false;
    }
    
    LOG_INFO("Stopping crawl session: " + sessionId);
    
    if (it->second->crawler) {
        it->second->crawler->stop();
    }
    
    it->second->isCompleted = true;
    return true;
}

std::vector<std::string> CrawlerManager::getActiveSessions() {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    
    std::vector<std::string> activeSessionIds;
    for (const auto& [sessionId, session] : sessions_) {
        if (!session->isCompleted) {
            activeSessionIds.push_back(sessionId);
        }
    }
    
    return activeSessionIds;
}

void CrawlerManager::cleanupCompletedSessions() {
    // First, collect sessions to clean up without holding the lock during join
    std::vector<std::string> toCleanupIds;
    std::vector<std::string> timedOutIds;
    auto now = std::chrono::system_clock::now();
    {
        std::lock_guard<std::mutex> lock(sessionsMutex_);
        for (const auto& [id, session] : sessions_) {
            // Cleanup completed sessions after 5 minutes
            bool shouldCleanup = session->isCompleted &&
                (now - session->createdAt) > std::chrono::minutes(5);
            
            // Timeout long-running sessions based on config (default: 10 minutes)
            auto sessionDuration = now - session->createdAt;
            bool isTimedOut = !session->isCompleted && 
                session->crawler &&
                sessionDuration > session->crawler->getConfig().maxSessionDuration;
            
            if (shouldCleanup) {
                toCleanupIds.push_back(id);
            } else if (isTimedOut) {
                timedOutIds.push_back(id);
                LOG_WARNING("Session timeout detected for session: " + id + 
                          " (running for " + std::to_string(
                              std::chrono::duration_cast<std::chrono::minutes>(sessionDuration).count()) + " minutes)");
            }
        }
    }

    for (const auto& id : toCleanupIds) {
        std::unique_ptr<CrawlSession> sessionCopy;
        {
            std::lock_guard<std::mutex> lock(sessionsMutex_);
            auto it = sessions_.find(id);
            if (it == sessions_.end()) continue;
            LOG_INFO("Cleaning up completed session: " + it->second->id);
            // Move session out so we can operate without holding the map lock
            sessionCopy = std::move(it->second);
            sessions_.erase(it);
        }

        // Stop crawler if still running
        if (sessionCopy && sessionCopy->crawler) {
            sessionCopy->crawler->stop();
        }

        // Join thread outside of the sessions mutex to avoid deadlocks and UB
        if (sessionCopy && sessionCopy->crawlThread.joinable()) {
            sessionCopy->crawlThread.join();
        }
        // sessionCopy goes out of scope and is destroyed cleanly here
    }
    
    // Handle timed-out sessions
    for (const auto& id : timedOutIds) {
        std::unique_ptr<CrawlSession> sessionCopy;
        {
            std::lock_guard<std::mutex> lock(sessionsMutex_);
            auto it = sessions_.find(id);
            if (it == sessions_.end()) continue;
            
            LOG_WARNING("Forcibly stopping timed-out session: " + it->second->id);
            CrawlLogger::broadcastLog("⏰ Session timeout: Stopping session " + it->second->id + " (exceeded maximum duration)", "warning");
            
            // Move session out so we can operate without holding the map lock
            sessionCopy = std::move(it->second);
            sessions_.erase(it);
        }

        // Force stop crawler
        if (sessionCopy && sessionCopy->crawler) {
            sessionCopy->crawler->stop();
        }

        // Join thread outside of the sessions mutex
        if (sessionCopy && sessionCopy->crawlThread.joinable()) {
            sessionCopy->crawlThread.join();
        }
        // sessionCopy goes out of scope and is destroyed cleanly here
    }
}

size_t CrawlerManager::getActiveSessionCount() {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    return sessions_.size();
}

std::string CrawlerManager::generateSessionId() {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    auto counter = sessionCounter_.fetch_add(1);
    
    std::stringstream ss;
    ss << "crawl_" << timestamp << "_" << counter;
    return ss.str();
}

void CrawlerManager::cleanupWorker() {
    LOG_INFO("CrawlerManager cleanup worker started");
    
    while (!shouldStop_) {
        try {
            cleanupCompletedSessions();
            
            // Sleep for 30 seconds
            for (int i = 0; i < 300 && !shouldStop_; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
        } catch (const std::exception& e) {
            LOG_ERROR("Error in cleanup worker: " + std::string(e.what()));
        }
    }
    
    LOG_INFO("CrawlerManager cleanup worker stopped");
}

std::unique_ptr<Crawler> CrawlerManager::createCrawler(const CrawlConfig& config, const std::string& sessionId) {
    auto crawler = std::make_unique<Crawler>(config, storage_, sessionId);
    
    // Configure PageFetcher settings
    if (crawler->getPageFetcher()) {
        // Disable SSL verification for problematic sites
        crawler->getPageFetcher()->setVerifySSL(false);
        
        // Enable SPA rendering if configured
        if (config.spaRenderingEnabled) {
            crawler->getPageFetcher()->setSpaRendering(true, config.browserlessUrl);
            CrawlLogger::broadcastLog("🤖 SPA rendering enabled for session with browserless URL: " + config.browserlessUrl, "info");
        }
    }
    
    return crawler;
} 