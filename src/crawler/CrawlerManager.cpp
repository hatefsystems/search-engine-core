#include "search_engine/crawler/CrawlerManager.h"
#include "../../include/Logger.h"
#include "../../include/crawler/CrawlLogger.h"
#include "PageFetcher.h"
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <cstdlib>

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

std::string CrawlerManager::startCrawl(const std::string& url, const CrawlConfig& config, bool force, CrawlCompletionCallback completionCallback, CrawlPriority priority) {
    // Check current concurrency vs effective limit.
    size_t currentSessions = getActiveSessionCount();
    size_t maxConcurrent = resolveMaxConcurrentSessions();

    std::string sessionId = generateSessionId();

    // If we're at the limit, queue this session instead of rejecting it.
    if (currentSessions >= maxConcurrent) {
        LOG_INFO("Concurrency limit reached (" + std::to_string(currentSessions) + "/" + std::to_string(maxConcurrent) +
                 "), queuing session: " + sessionId + " for URL: " + url +
                 " (priority=" + std::to_string(static_cast<int>(priority)) + ")");
        CrawlLogger::broadcastSessionLog(sessionId, "Queued crawl session for URL: " + url, "info");

        PendingSessionEntry entry;
        entry.sessionId = sessionId;
        entry.url = url;
        entry.force = force;
        entry.priority = priority;
        entry.queuedAt = std::chrono::system_clock::now();
        entry.readyAt = entry.queuedAt;
        entry.retryCount = 0;
        {
            std::lock_guard<std::mutex> lock(pendingExtrasMutex_);
            pendingConfigs_[sessionId] = config;
            if (completionCallback) {
                pendingCallbacks_[sessionId] = std::move(completionCallback);
            }
        }
        pendingQueue_.push(entry);
        return sessionId;
    }

    LOG_INFO("Starting new crawl session: " + sessionId + " for URL: " + url + " (priority=" + std::to_string(static_cast<int>(priority)) + ")");
    LOG_DEBUG("CrawlerManager::startCrawl - Current active sessions: " + std::to_string(currentSessions) + "/" + std::to_string(maxConcurrent));
    CrawlLogger::broadcastSessionLog(sessionId, "Starting new crawl session for URL: " + url, "info");

    try {
        startCrawlInternal(sessionId, url, config, force, priority, std::move(completionCallback), 0);
        return sessionId;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to start crawl session: " + std::string(e.what()));
        throw;
    }
}

void CrawlerManager::startCrawlInternal(const std::string& sessionId,
                                        const std::string& url,
                                        const CrawlConfig& config,
                                        bool force,
                                        CrawlPriority priority,
                                        CrawlCompletionCallback completionCallback,
                                        int retryCount) {
    try {
        // Create new crawler instance with the provided configuration
        LOG_DEBUG("CrawlerManager::startCrawlInternal - Creating crawler for session: " + sessionId);
        auto crawler = createCrawler(config, sessionId);

        // Create crawl session with completion callback
        LOG_DEBUG("CrawlerManager::startCrawlInternal - Creating crawl session for session: " + sessionId);
        auto session = std::make_unique<CrawlSession>(sessionId, std::move(crawler), std::move(completionCallback), priority);
        
        // Add seed URL to the crawler
        LOG_DEBUG("CrawlerManager::startCrawlInternal - Adding seed URL for session: " + sessionId);
        session->crawler->addSeedURL(url, force);

        // Start crawling in a separate thread.
        // Capture retry context for session-level retry on failure.
        session->crawlThread = std::thread([sessionId, this, url, config, force, priority, retryCount]() {
            bool sessionFailed = false;
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
                sessionFailed = true;
            }

            // Mark session as completed and execute completion callback
            CrawlCompletionCallback callbackCopy;
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
                    // Stash callback for potential re-use on retry.
                    callbackCopy = completedSession->completionCallback;
                }
            }
            lock.unlock();

            // Step 4: session-level retry on failure with exponential backoff.
            if (sessionFailed && config.maxSessionRetries > 0 && retryCount < config.maxSessionRetries) {
                // Exponential backoff: base * 2^retryCount, capped at max delay.
                auto delayMs = config.sessionRetryBaseDelay.count();
                for (int i = 0; i < retryCount; ++i) {
                    delayMs *= 2;
                    if (delayMs > config.sessionRetryMaxDelay.count()) {
                        delayMs = config.sessionRetryMaxDelay.count();
                        break;
                    }
                }
                auto readyAt = std::chrono::system_clock::now() + std::chrono::milliseconds(delayMs);
                LOG_WARNING("Session " + sessionId + " failed; scheduling retry " +
                            std::to_string(retryCount + 1) + "/" +
                            std::to_string(config.maxSessionRetries) +
                            " after " + std::to_string(delayMs) + "ms");

                PendingSessionEntry retryEntry;
                retryEntry.sessionId = sessionId;  // keep same session id across retries
                retryEntry.url = url;
                retryEntry.force = force;
                retryEntry.priority = CrawlPriority::RETRY;  // retries jump the queue
                retryEntry.queuedAt = std::chrono::system_clock::now();
                retryEntry.readyAt = readyAt;
                retryEntry.retryCount = retryCount + 1;
                {
                    std::lock_guard<std::mutex> elock(pendingExtrasMutex_);
                    pendingConfigs_[sessionId] = config;
                    if (callbackCopy) pendingCallbacks_[sessionId] = std::move(callbackCopy);
                }
                pendingQueue_.push(retryEntry);
            }

            // A slot just opened — try to start the next pending session.
            tryDispatchPending();
        });

        // Store session
        {
            std::lock_guard<std::mutex> lock(sessionsMutex_);
            LOG_DEBUG("CrawlerManager::startCrawlInternal - Storing session in map: " + sessionId);
            sessions_[sessionId] = std::move(session);
            LOG_DEBUG("CrawlerManager::startCrawlInternal - Session stored, total active sessions: " + std::to_string(sessions_.size()));
        }

        LOG_INFO("Crawl session started successfully: " + sessionId);
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to start crawl session " + sessionId + ": " + std::string(e.what()));
        throw;
    }
}

void CrawlerManager::tryDispatchPending() {
    // Pop a ready (priority-respecting, time-eligible) pending session and
    // start it if a slot is free. Loops in case there are multiple ready
    // sessions and multiple free slots (e.g. on init/cleanup).
    while (true) {
        size_t active = getActiveSessionCount();
        size_t maxConcurrent = resolveMaxConcurrentSessions();
        if (active >= maxConcurrent) return;

        auto popped = pendingQueue_.tryPopReady();
        if (!popped) return;  // nothing ready

        CrawlConfig cfg;
        CrawlCompletionCallback cb;
        {
            std::lock_guard<std::mutex> lock(pendingExtrasMutex_);
            auto cit = pendingConfigs_.find(popped->sessionId);
            if (cit != pendingConfigs_.end()) {
                cfg = cit->second;
                pendingConfigs_.erase(cit);
            }
            auto kit = pendingCallbacks_.find(popped->sessionId);
            if (kit != pendingCallbacks_.end()) {
                cb = std::move(kit->second);
                pendingCallbacks_.erase(kit);
            }
        }

        LOG_INFO("Dispatching pending session: " + popped->sessionId +
                 " (priority=" + std::to_string(static_cast<int>(popped->priority)) +
                 ", retry=" + std::to_string(popped->retryCount) + ")");
        CrawlLogger::broadcastSessionLog(popped->sessionId, "Dispatching queued crawl session", "info");

        try {
            startCrawlInternal(popped->sessionId, popped->url, cfg, popped->force,
                               popped->priority, std::move(cb), popped->retryCount);
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to dispatch pending session " + popped->sessionId + ": " + e.what());
            // Drop on the floor; do not loop forever on a broken entry.
        }
    }
}

size_t CrawlerManager::resolveMaxConcurrentSessions() const {
    const char* maxSessionsEnv = std::getenv("MAX_CONCURRENT_SESSIONS");
    size_t maxConcurrent = MAX_CONCURRENT_SESSIONS;
    if (maxSessionsEnv) {
        try {
            maxConcurrent = std::stoull(maxSessionsEnv);
        } catch (...) {
            LOG_WARNING("Invalid MAX_CONCURRENT_SESSIONS value, using default: " +
                        std::to_string(MAX_CONCURRENT_SESSIONS));
        }
    }
    return maxConcurrent;
}

size_t CrawlerManager::getPendingSessionCount() const {
    return pendingQueue_.size();
}

std::vector<PendingSessionEntry> CrawlerManager::getPendingSessions() const {
    return pendingQueue_.snapshot();
}

size_t CrawlerManager::getMaxConcurrentSessions() const {
    return resolveMaxConcurrentSessions();
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
    // If session is queued (not yet running), report "queued".
    if (pendingQueue_.contains(sessionId)) {
        return "queued";
    }

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
    // If session is still queued (not yet running), just drop it from the queue.
    if (pendingQueue_.remove(sessionId)) {
        std::lock_guard<std::mutex> elock(pendingExtrasMutex_);
        pendingConfigs_.erase(sessionId);
        pendingCallbacks_.erase(sessionId);
        LOG_INFO("Cancelled pending crawl session: " + sessionId);
        return true;
    }

    std::unique_ptr<CrawlSession> sessionCopy;
    {
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
        
        // Move session out so we can clean up without holding the lock
        sessionCopy = std::move(it->second);
        sessions_.erase(it);
    }
    
    // Clean up the stopped session (outside the lock)
    if (sessionCopy && sessionCopy->crawlThread.joinable()) {
        sessionCopy->crawlThread.join();
    }
    
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
            // Cleanup completed sessions after 5 seconds
            bool shouldCleanup = session->isCompleted &&
                (now - session->createdAt) > std::chrono::seconds(5);
            
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
    
    // Count only truly active sessions (not completed ones)
    size_t activeCount = 0;
    for (const auto& [id, session] : sessions_) {
        if (!session->isCompleted) {
            activeCount++;
        }
    }
    
    return activeCount;
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
            // Also drive the pending queue — picks up retry entries whose
            // backoff has just elapsed.
            tryDispatchPending();

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