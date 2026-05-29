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
#include "models/CrawlConfig.h"
#include "models/CrawlResult.h"
#include "../auth/User.h"
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
    // Owner of this session. Empty string means "system / anonymous"
    // (legacy behaviour); when set, only the owner or admins can access. #13
    std::string userId;

    CrawlSession(const std::string& sessionId, std::unique_ptr<Crawler> crawlerInstance,
                 CrawlCompletionCallback callback = nullptr,
                 std::string ownerUserId = "")
        : id(sessionId), crawler(std::move(crawlerInstance)), createdAt(std::chrono::system_clock::now()),
          completionCallback(std::move(callback)), userId(std::move(ownerUserId)) {}

    CrawlSession(CrawlSession&& other) noexcept
        : id(std::move(other.id))
        , crawler(std::move(other.crawler))
        , createdAt(other.createdAt)
        , isCompleted(other.isCompleted.load())
        , crawlThread(std::move(other.crawlThread))
        , completionCallback(std::move(other.completionCallback))
        , userId(std::move(other.userId)) {}
    
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
                          const std::string& ownerUserId = "");

    std::vector<CrawlResult> getCrawlResults(const std::string& sessionId);
    std::string getCrawlStatus(const std::string& sessionId);
    bool stopCrawl(const std::string& sessionId);
    std::vector<std::string> getActiveSessions();
    void cleanupCompletedSessions();
    size_t getActiveSessionCount();

    // ---- User-aware variants (issue #13) ----
    // Each returns the same data as the legacy variant if the AuthContext is
    // an admin OR the session is owned by ctx.userId. Otherwise returns the
    // "not found" / "not authorised" equivalent (empty results, "not_found"
    // status, false for stopCrawl). Empty owner on a session = anyone may
    // access (back-compat for legacy callers).
    std::vector<CrawlResult> getCrawlResults(const std::string& sessionId,
                                             const search_engine::auth::AuthContext& ctx);
    std::string getCrawlStatus(const std::string& sessionId,
                               const search_engine::auth::AuthContext& ctx);
    bool stopCrawl(const std::string& sessionId,
                   const search_engine::auth::AuthContext& ctx);
    std::vector<std::string> getActiveSessions(const search_engine::auth::AuthContext& ctx);

    // Helper exposed for controllers and tests: is `ctx` allowed to touch
    // the session owned by `sessionOwnerId`?
    static bool canAccess(const std::string& sessionOwnerId,
                          const search_engine::auth::AuthContext& ctx);
    
    // Get access to storage for logging
    std::shared_ptr<search_engine::storage::ContentStorage> getStorage() const { return storage_; }
    
    
    // Limit concurrent sessions to prevent MongoDB connection issues
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
};


