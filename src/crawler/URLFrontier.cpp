#include "URLFrontier.h"
#include "../../include/search_engine/crawler/FrontierPersistence.h"
#include "../../include/Logger.h"
#include "../../include/crawler/CrawlLogger.h"
#include "../../include/search_engine/common/UrlSanitizer.h"
#include "../../include/search_engine/common/UrlCanonicalizer.h"
#include <algorithm>
#include <regex>
#include <sstream>
#include <iomanip>
#include <vector>

URLFrontier::URLFrontier() {
    LOG_DEBUG("URLFrontier constructor called");
}

URLFrontier::~URLFrontier() {
    LOG_DEBUG("URLFrontier destructor called");
}

void URLFrontier::setPersistentStorage(search_engine::crawler::FrontierPersistence* persistence, const std::string& sessionId) {
    persistence_ = persistence;
    sessionId_ = sessionId;
}

void URLFrontier::addURL(const std::string& url, bool force, CrawlPriority priority, int depth) {
    LOG_DEBUG("URLFrontier::addURL called with: " + url + (force ? " (force)" : "") +
              ", priority: " + std::to_string(static_cast<int>(priority)) + 
              ", depth: " + std::to_string(depth));
    
    std::string normalizedURL = search_engine::common::UrlCanonicalizer::canonicalize(search_engine::common::sanitizeUrl(url));
    
    if (force) {
        // Remove from visited set if present
        {
            std::lock_guard<std::mutex> visitedLock(visitedMutex);
            visitedURLs.erase(normalizedURL);
        }
        // Remove from queued URLs tracking
        {
            std::lock_guard<std::mutex> queuedLock(queuedMutex);
            queuedURLs.erase(normalizedURL);
        }
        // Note: We don't remove from queues directly as it's expensive with priority_queue
        // Instead, we'll check and skip during getNextURL() if needed
    }
    
    // Check if already visited (unless forced)
    if (!force) {
        std::lock_guard<std::mutex> visitedLock(visitedMutex);
        if (visitedURLs.find(normalizedURL) != visitedURLs.end()) {
            LOG_DEBUG("URL already visited, skipping: " + normalizedURL);
            return;
        }
    }
    
    // Check if already queued (unless forced)
    if (!force) {
        std::lock_guard<std::mutex> queuedLock(queuedMutex);
        if (queuedURLs.find(normalizedURL) != queuedURLs.end()) {
            LOG_DEBUG("URL already queued, skipping: " + normalizedURL);
            return;
        }
    }
    
    // Add to main queue
    {
        std::lock_guard<std::mutex> mainLock(mainQueueMutex);
        std::lock_guard<std::mutex> queuedLock(queuedMutex);
        
        QueuedURL queuedUrl = createQueuedURL(normalizedURL, priority, depth);
        mainQueue.push(queuedUrl);
        queuedURLs.insert(normalizedURL);
        if (persistence_) {
            persistence_->upsertTask(sessionId_, url, normalizedURL, extractDomain(normalizedURL), depth, static_cast<int>(priority), "queued", std::chrono::system_clock::now(), 0);
        }
        
        LOG_DEBUG("Added URL to main queue: " + normalizedURL + 
                  ", main queue size: " + std::to_string(mainQueue.size()));
        CrawlLogger::broadcastLog("Added URL to queue: " + normalizedURL, "info");
    }
}

std::string URLFrontier::getNextURL() {
    // Use steady_clock for consistent monotonic time in readiness checks
    auto now = std::chrono::system_clock::now();
    
    // First, check retry queue for ready URLs
    {
        std::lock_guard<std::mutex> retryLock(retryQueueMutex);
        std::lock_guard<std::mutex> queuedLock(queuedMutex);
        
        // Process retry queue - get URLs that are ready to retry
        std::vector<QueuedURL> notReadyYet;
        
        while (!retryQueue.empty()) {
            QueuedURL queuedUrl = retryQueue.top();
            retryQueue.pop();
            
            // Skip if URL was visited while waiting for retry
            {
                std::lock_guard<std::mutex> visitedLock(visitedMutex);
                if (visitedURLs.find(queuedUrl.url) != visitedURLs.end()) {
                    queuedURLs.erase(queuedUrl.url);
                    LOG_DEBUG("Skipping retry URL (already visited): " + queuedUrl.url);
                    continue;
                }
            }
            
            if (queuedUrl.nextRetryTime <= now) {
                // Ready to retry
                queuedURLs.erase(queuedUrl.url);  // Remove from tracking
                LOG_INFO("Retrieved retry URL: " + queuedUrl.url + 
                        " (attempt " + std::to_string(queuedUrl.retryCount + 1) + ")");
                if (persistence_) {
                    persistence_->upsertTask(sessionId_, queuedUrl.url, search_engine::common::UrlCanonicalizer::canonicalize(queuedUrl.url), extractDomain(queuedUrl.url), queuedUrl.depth, static_cast<int>(queuedUrl.priority), "claimed", std::chrono::system_clock::now(), queuedUrl.retryCount);
                }
                CrawlLogger::broadcastLog("Retrying URL: " + queuedUrl.url + 
                                        " (attempt " + std::to_string(queuedUrl.retryCount + 1) + ")", "info");
                
                // Put back not-ready URLs
                for (const auto& notReady : notReadyYet) {
                    retryQueue.push(notReady);
                }
                
                return queuedUrl.url;
            } else {
                // Not ready yet, keep it
                notReadyYet.push_back(queuedUrl);
            }
        }
        
        // Put back all not-ready URLs
        for (const auto& notReady : notReadyYet) {
            retryQueue.push(notReady);
        }
    }
    
    // Then check main queue
    {
        std::lock_guard<std::mutex> mainLock(mainQueueMutex);
        std::lock_guard<std::mutex> queuedLock(queuedMutex);
        
        while (!mainQueue.empty()) {
            QueuedURL queuedUrl = mainQueue.top();
            mainQueue.pop();
            
            // Skip if URL was visited while in queue
            {
                std::lock_guard<std::mutex> visitedLock(visitedMutex);
                if (visitedURLs.find(queuedUrl.url) != visitedURLs.end()) {
                    queuedURLs.erase(queuedUrl.url);
                    LOG_DEBUG("Skipping main queue URL (already visited): " + queuedUrl.url);
                    continue;
                }
            }
            
            queuedURLs.erase(queuedUrl.url);  // Remove from tracking
            LOG_DEBUG("Retrieved URL from main queue: " + queuedUrl.url + 
                     ", remaining main queue size: " + std::to_string(mainQueue.size()));
            if (persistence_) {
                persistence_->upsertTask(sessionId_, queuedUrl.url, search_engine::common::UrlCanonicalizer::canonicalize(queuedUrl.url), extractDomain(queuedUrl.url), queuedUrl.depth, static_cast<int>(queuedUrl.priority), "claimed", std::chrono::system_clock::now(), queuedUrl.retryCount);
            }
            return queuedUrl.url;
        }
    }
    
    LOG_DEBUG("URLFrontier::getNextURL - All queues are empty, returning empty string");
    return "";
}

void URLFrontier::scheduleRetry(const std::string& url, 
                              int retryCount, 
                              const std::string& error, 
                              FailureType failureType,
                              std::chrono::milliseconds delay) {
    LOG_INFO("Scheduling retry for URL: " + url + 
             ", attempt: " + std::to_string(retryCount) + 
             ", delay: " + std::to_string(delay.count()) + "ms" + 
             ", error: " + error);
    
    std::string normalizedURL = search_engine::common::UrlCanonicalizer::canonicalize(search_engine::common::sanitizeUrl(url));
    auto nextRetryTime = std::chrono::system_clock::now() + delay;
    
    QueuedURL retryUrl;
    retryUrl.url = normalizedURL;
    retryUrl.retryCount = retryCount;
    retryUrl.nextRetryTime = nextRetryTime;
    retryUrl.lastError = error;
    retryUrl.priority = CrawlPriority::RETRY;
    retryUrl.lastFailureType = failureType;
    
    // Preserve the original depth of the URL being retried
    // Try to find the URL in visited or current queues to get its depth
    retryUrl.depth = 0; // Default depth if not found
    
    // Check if we can find the URL's original depth from getQueuedURLInfo
    try {
        QueuedURL originalInfo = getQueuedURLInfo(normalizedURL);
        if (!originalInfo.url.empty()) {
            retryUrl.depth = originalInfo.depth;
        }
    } catch (...) {
        // If we can't find the original depth, default to 0
        LOG_DEBUG("Could not find original depth for retry URL: " + normalizedURL + ", using depth 0");
    }
    
    {
        std::lock_guard<std::mutex> retryLock(retryQueueMutex);
        std::lock_guard<std::mutex> queuedLock(queuedMutex);
        std::lock_guard<std::mutex> retryCountLock(retryCountMutex);
        
        retryQueue.push(retryUrl);
        queuedURLs.insert(normalizedURL);
        
        // Update retry count in tracking map
        retryCountMap[normalizedURL] = retryCount;
        
        if (persistence_) {
            persistence_->updateRetry(sessionId_, normalizedURL, retryCount, nextRetryTime);
        }
        
        LOG_DEBUG("Added URL to retry queue: " + normalizedURL + 
                  ", retry queue size: " + std::to_string(retryQueue.size()));
        
        CrawlLogger::broadcastLog("Scheduled retry for: " + url + 
                                " (attempt " + std::to_string(retryCount) + 
                                " in " + std::to_string(delay.count()) + "ms)", "warning");
    }
}

bool URLFrontier::isEmpty() const {
    std::lock_guard<std::mutex> mainLock(mainQueueMutex);
    bool mainEmpty = mainQueue.empty();
    
    if (!mainEmpty) {
        return false;
    }
    
    std::lock_guard<std::mutex> retryLock(retryQueueMutex);
    bool retryEmpty = retryQueue.empty();
    
    LOG_TRACE("URLFrontier::isEmpty - Both queues are " + std::string((mainEmpty && retryEmpty) ? "empty" : "not empty"));
    return mainEmpty && retryEmpty;
}

bool URLFrontier::hasReadyURLs() const {
    
    // Check main queue
    {
        std::lock_guard<std::mutex> mainLock(mainQueueMutex);
        if (!mainQueue.empty()) {
            return true;
        }
    }
    
    // Check retry queue for ready URLs
    {
        std::lock_guard<std::mutex> retryLock(retryQueueMutex);
        if (retryQueue.empty()) {
            return false;
        }
        
        // We need to peek at the top element without modifying the queue
        // Since priority_queue doesn't have non-const top(), we can't do this easily
        // So we'll be conservative and return true if retry queue has items
        // The actual readiness check happens in getNextURL()
        return true;
    }
}

size_t URLFrontier::size() const {
    std::lock_guard<std::mutex> mainLock(mainQueueMutex);
    std::lock_guard<std::mutex> retryLock(retryQueueMutex);
    
    size_t totalSize = mainQueue.size() + retryQueue.size();
    LOG_TRACE("URLFrontier::size - Total queue size: " + std::to_string(totalSize) + 
              " (main: " + std::to_string(mainQueue.size()) + 
              ", retry: " + std::to_string(retryQueue.size()) + ")");
    return totalSize;
}

size_t URLFrontier::retryQueueSize() const {
    std::lock_guard<std::mutex> retryLock(retryQueueMutex);
    return retryQueue.size();
}

size_t URLFrontier::pendingRetryCount() const {
    std::lock_guard<std::mutex> retryLock(retryQueueMutex);
    // For now, return total retry queue size
    // Could be enhanced to count only non-ready URLs
    return retryQueue.size();
}

void URLFrontier::markVisited(const std::string& url) {
    LOG_DEBUG("URLFrontier::markVisited called with: " + url);
    std::string normalizedURL = search_engine::common::UrlCanonicalizer::canonicalize(search_engine::common::sanitizeUrl(url));
    std::string domain = extractDomain(normalizedURL);
    
    std::lock_guard<std::mutex> visitedLock(visitedMutex);
    visitedURLs.insert(normalizedURL);
    LOG_DEBUG("Marked URL as visited: " + normalizedURL + ", visited URLs count: " + std::to_string(visitedURLs.size()));
    
    std::lock_guard<std::mutex> domainLock(domainMutex);
    domainLastVisit[domain] = std::chrono::system_clock::now();
    LOG_DEBUG("Updated last visit time for domain: " + domain);
    
    // Clean up retry count for this URL since it's been processed
    std::lock_guard<std::mutex> retryCountLock(retryCountMutex);
    retryCountMap.erase(normalizedURL);
    LOG_DEBUG("Cleaned up retry count for URL: " + normalizedURL);
}

bool URLFrontier::isVisited(const std::string& url) const {
    std::string normalizedURL = search_engine::common::UrlCanonicalizer::canonicalize(search_engine::common::sanitizeUrl(url));
    std::lock_guard<std::mutex> lock(visitedMutex);
    bool visited = visitedURLs.find(normalizedURL) != visitedURLs.end();
    LOG_TRACE("URLFrontier::isVisited - URL: " + url + " is " + (visited ? "visited" : "not visited"));
    return visited;
}

std::chrono::system_clock::time_point URLFrontier::getLastVisitTime(const std::string& domain) const {
    std::lock_guard<std::mutex> lock(domainMutex);
    auto it = domainLastVisit.find(domain);
    if (it != domainLastVisit.end()) {
        LOG_TRACE("URLFrontier::getLastVisitTime - Domain: " + domain + " has a last visit time");
        return it->second;
    }
    LOG_TRACE("URLFrontier::getLastVisitTime - Domain: " + domain + " has no last visit time");
    return std::chrono::system_clock::time_point::min();
}

std::string URLFrontier::extractDomain(const std::string& url) const {
    static const std::regex domainRegex(R"(https?://([^/:]+))");
    std::smatch matches;
    if (std::regex_search(url, matches, domainRegex) && matches.size() > 1) {
        std::string domain = matches[1].str();
        LOG_TRACE("URLFrontier::extractDomain - Extracted domain: " + domain + " from URL: " + url);
        return domain;
    }
    LOG_WARNING("URLFrontier::extractDomain - Could not extract domain from URL: " + url);
    return "";
}

QueuedURL URLFrontier::getQueuedURLInfo(const std::string& url) const {
    std::string normalizedURL = search_engine::common::UrlCanonicalizer::canonicalize(search_engine::common::sanitizeUrl(url));
    
    QueuedURL result;
    result.url = normalizedURL;
    result.priority = CrawlPriority::NORMAL;
    result.lastFailureType = FailureType::UNKNOWN;
    
    // Get retry count from our tracking map
    {
        std::lock_guard<std::mutex> retryCountLock(retryCountMutex);
        auto it = retryCountMap.find(normalizedURL);
        result.retryCount = (it != retryCountMap.end()) ? it->second : 0;
    }
    
    return result;
}

URLFrontier::RetryStats URLFrontier::getRetryStats() const {
    RetryStats stats;
    auto now = std::chrono::system_clock::now();
    
    std::lock_guard<std::mutex> retryLock(retryQueueMutex);
    stats.totalRetries = retryQueue.size();
    stats.pendingRetries = retryQueue.size();
    stats.readyRetries = 0;
    
    // Set next retry time to max if no retries pending
    if (retryQueue.empty()) {
        stats.nextRetryTime = std::chrono::system_clock::time_point::max();
    } else {
        // We can't easily peek into priority_queue without modifying it
        // So we'll set a reasonable default
        stats.nextRetryTime = now + std::chrono::minutes(1);
    }
    
    return stats;
}

QueuedURL URLFrontier::createQueuedURL(const std::string& url, CrawlPriority priority, int depth) const {
    QueuedURL queuedUrl;
    queuedUrl.url = url;
    queuedUrl.retryCount = 0;
    queuedUrl.nextRetryTime = std::chrono::system_clock::now();
    queuedUrl.lastError = "";
    queuedUrl.priority = priority;
    queuedUrl.lastFailureType = FailureType::UNKNOWN;
    queuedUrl.depth = depth;
    return queuedUrl;
}

void URLFrontier::removeFromMainQueue(const std::string& url) {
    // Note: priority_queue doesn't support efficient removal
    // This is a placeholder - in practice, we handle this by checking during getNextURL()
    LOG_DEBUG("URLFrontier::removeFromMainQueue called for: " + url + " (lazy removal)");
}

void URLFrontier::markCompleted(const std::string& url) {
    if (!persistence_) return;
    std::string normalized = search_engine::common::UrlCanonicalizer::canonicalize(url);
    persistence_->markCompleted(sessionId_, normalized);
}

// Note: normalizeURL method removed - now using UrlCanonicalizer::canonicalize() for consistent URL normalization 