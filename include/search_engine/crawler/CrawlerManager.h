#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>
#include "Crawler.h"
#include "CrawlPriority.h"
#include "SessionPriorityQueue.h"
#include "models/CrawlConfig.h"
#include "models/CrawlResult.h"
#include "../storage/ContentStorage.h"

// Forward declaration for completion callback
class CrawlerManager;

/**
 * @brief Completion callback function type for crawl sessions
 * @param sessionId The session ID that completed
 * @param results The crawl results
 * @param manager Pointer to the CrawlerManager for additional operations
 */
using CrawlCompletionCallback = std::function<void(const std::string& sessionId, 
                                                   const std::vector<CrawlResult>& results, 
                                                   CrawlerManager* manager)>;

struct CrawlSession {
    std::string id;
    std::unique_ptr<Crawler> crawler;
    std::chrono::system_clock::time_point createdAt;
    std::atomic<bool> isCompleted{false};
    std::thread crawlThread;
    CrawlCompletionCallback completionCallback;
    CrawlPriority priority{CrawlPriority::NORMAL};

    CrawlSession(const std::string& sessionId, std::unique_ptr<Crawler> crawlerInstance,
                 CrawlCompletionCallback callback = nullptr,
                 CrawlPriority sessionPriority = CrawlPriority::NORMAL)
        : id(sessionId), crawler(std::move(crawlerInstance)), createdAt(std::chrono::system_clock::now()),
          completionCallback(std::move(callback)), priority(sessionPriority) {}

    CrawlSession(CrawlSession&& other) noexcept
        : id(std::move(other.id))
        , crawler(std::move(other.crawler))
        , createdAt(other.createdAt)
        , isCompleted(other.isCompleted.load())
        , crawlThread(std::move(other.crawlThread))
        , completionCallback(std::move(other.completionCallback))
        , priority(other.priority) {}
    
    CrawlSession(const CrawlSession&) = delete;
    CrawlSession& operator=(const CrawlSession&) = delete;
    CrawlSession& operator=(CrawlSession&&) = delete;
};

class CrawlerManager {
public:
    CrawlerManager(std::shared_ptr<search_engine::storage::ContentStorage> storage);
    ~CrawlerManager();
    
    /**
     * @brief Start a new crawl session
     * @param url The URL to crawl
     * @param config Crawl configuration
     * @param force Whether to force crawling (ignore robots.txt)
     * @param completionCallback Optional callback to execute when crawl completes
     * @return Session ID of the started crawl
     */
    std::string startCrawl(const std::string& url, const CrawlConfig& config, bool force = false,
                          CrawlCompletionCallback completionCallback = nullptr,
                          CrawlPriority priority = CrawlPriority::NORMAL);
    
    std::vector<CrawlResult> getCrawlResults(const std::string& sessionId);
    std::string getCrawlStatus(const std::string& sessionId);
    bool stopCrawl(const std::string& sessionId);
    std::vector<std::string> getActiveSessions();
    void cleanupCompletedSessions();
    size_t getActiveSessionCount();

    // ----- Session queuing & prioritization (issue #14) -----

    // Total number of pending (queued, not yet running) sessions.
    size_t getPendingSessionCount() const;

    // Snapshot of pending sessions in priority order, for inspection / API.
    std::vector<PendingSessionEntry> getPendingSessions() const;

    // Effective concurrency limit (env MAX_CONCURRENT_SESSIONS or default).
    size_t getMaxConcurrentSessions() const;

    // Get access to storage for logging
    std::shared_ptr<search_engine::storage::ContentStorage> getStorage() const { return storage_; }


    // Default concurrent session limit (overridable via MAX_CONCURRENT_SESSIONS env).
    static constexpr size_t MAX_CONCURRENT_SESSIONS = 5;

private:
    std::shared_ptr<search_engine::storage::ContentStorage> storage_;
    std::unordered_map<std::string, std::unique_ptr<CrawlSession>> sessions_;
    std::mutex sessionsMutex_;
    std::atomic<uint64_t> sessionCounter_{0};
    std::thread cleanupThread_;
    std::atomic<bool> shouldStop_{false};
    std::string generateSessionId();
    void cleanupWorker();
    std::unique_ptr<Crawler> createCrawler(const CrawlConfig& config, const std::string& sessionId = "");

    // ----- Session queuing & prioritization internals -----
    SessionPriorityQueue pendingQueue_;

    // Per-pending-session config (cannot live in SessionPriorityQueue because
    // CrawlConfig is heavy and we want the queue testable without it).
    std::unordered_map<std::string, CrawlConfig> pendingConfigs_;
    std::unordered_map<std::string, CrawlCompletionCallback> pendingCallbacks_;
    mutable std::mutex pendingExtrasMutex_;

    // Actually start a crawl now (no concurrency check). Used both for
    // immediate starts and for dispatching pending sessions when a slot opens.
    void startCrawlInternal(const std::string& sessionId,
                            const std::string& url,
                            const CrawlConfig& config,
                            bool force,
                            CrawlPriority priority,
                            CrawlCompletionCallback callback,
                            int retryCount = 0);

    // Try to dequeue and dispatch the next ready pending session. Called when
    // an active session completes/fails and a slot opens up.
    void tryDispatchPending();

    // Read MAX_CONCURRENT_SESSIONS env (with fallback) and return effective limit.
    size_t resolveMaxConcurrentSessions() const;
};


