#pragma once

#include <string>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <chrono>
#include "FailureClassifier.h"
namespace search_engine { namespace crawler { class FrontierPersistence; } }

enum class CrawlPriority {
    LOW = 0,
    NORMAL = 1,
    HIGH = 2,
    RETRY = 3  // Higher priority for retries
};

struct QueuedURL {
    std::string url;
    int retryCount = 0;
    std::chrono::system_clock::time_point nextRetryTime = std::chrono::system_clock::now();
    std::string lastError;
    CrawlPriority priority = CrawlPriority::NORMAL;
    FailureType lastFailureType = FailureType::UNKNOWN;
    int depth = 0;  // Crawl depth (0 = seed URL, 1 = first level, etc.)
    
    // Comparison operator for priority queue (higher priority value = higher priority)
    bool operator<(const QueuedURL& other) const {
        // First compare by ready time
        if (nextRetryTime != other.nextRetryTime) {
            return nextRetryTime > other.nextRetryTime; // Earlier times have higher priority
        }
        // Then by priority level
        return static_cast<int>(priority) < static_cast<int>(other.priority);
    }
};

class URLFrontier {
public:
    URLFrontier();
    ~URLFrontier();

    // Optional: persist queue state to MongoDB for restart/resume visibility
    void setPersistentStorage(search_engine::crawler::FrontierPersistence* persistence, const std::string& sessionId);

    // Add a URL to the frontier
    void addURL(const std::string& url, bool force = false, CrawlPriority priority = CrawlPriority::NORMAL, int depth = 0);
    
    // Schedule a URL for retry with exponential backoff
    void scheduleRetry(const std::string& url, 
                      int retryCount, 
                      const std::string& error, 
                      FailureType failureType,
                      std::chrono::milliseconds delay);
    
    // Get the next URL to crawl (checks retry queue first)
    std::string getNextURL();
    
    // Get the queued URL information for a specific URL
    QueuedURL getQueuedURLInfo(const std::string& url) const;
    
    // Check if the frontier is empty (both main and retry queues)
    bool isEmpty() const;
    
    // Check if there are any URLs ready to be processed now
    bool hasReadyURLs() const;
    
    // Get the total number of URLs in both queues
    size_t size() const;
    
    // Get the number of URLs in the retry queue
    size_t retryQueueSize() const;
    
    // Get the number of pending retries (not yet ready)
    size_t pendingRetryCount() const;
    
    // Mark a URL as visited
    void markVisited(const std::string& url);
    
    // Check if a URL has been visited
    bool isVisited(const std::string& url) const;
    
    // Get the last visit time for a domain
    std::chrono::system_clock::time_point getLastVisitTime(const std::string& domain) const;

    // Extract domain from URL
    std::string extractDomain(const std::string& url) const;
    
    // Mark completion in persistence (if configured)
    void markCompleted(const std::string& url);
    
    // Get retry statistics
    struct RetryStats {
        size_t totalRetries = 0;
        size_t pendingRetries = 0;
        size_t readyRetries = 0;
        std::chrono::system_clock::time_point nextRetryTime;
    };
    RetryStats getRetryStats() const;
    
private:
    // Note: normalizeURL method removed - now using UrlCanonicalizer::canonicalize() for consistent URL normalization
    
    // Remove a URL from the main queue (used during retry scheduling)
    void removeFromMainQueue(const std::string& url);
    
    // Convert string URL to QueuedURL for main queue
    QueuedURL createQueuedURL(const std::string& url, CrawlPriority priority = CrawlPriority::NORMAL, int depth = 0) const;

    // Main queue for new URLs
    std::priority_queue<QueuedURL> mainQueue;
    
    // Retry queue for failed URLs (separate to handle timing)
    std::priority_queue<QueuedURL> retryQueue;
    
    // Track visited URLs
    std::unordered_set<std::string> visitedURLs;
    
    // Track domain last visit times
    std::unordered_map<std::string, std::chrono::system_clock::time_point> domainLastVisit;
    
    // Track URLs currently in queues to prevent duplicates
    std::unordered_set<std::string> queuedURLs;
    
    // Track retry counts separately since priority_queue doesn't allow iteration
    std::unordered_map<std::string, int> retryCountMap;
    
    // Mutexes for thread safety
    mutable std::mutex mainQueueMutex;
    mutable std::mutex retryQueueMutex;
    mutable std::mutex visitedMutex;
    mutable std::mutex domainMutex;
    mutable std::mutex queuedMutex;
    mutable std::mutex retryCountMutex;

    // Persistence hooks
    search_engine::crawler::FrontierPersistence* persistence_ = nullptr;
    std::string sessionId_;
}; 