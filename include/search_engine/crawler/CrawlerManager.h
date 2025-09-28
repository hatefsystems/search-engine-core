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
    
    CrawlSession(const std::string& sessionId, std::unique_ptr<Crawler> crawlerInstance, 
                 CrawlCompletionCallback callback = nullptr)
        : id(sessionId), crawler(std::move(crawlerInstance)), createdAt(std::chrono::system_clock::now()),
          completionCallback(std::move(callback)) {}
    
    CrawlSession(CrawlSession&& other) noexcept
        : id(std::move(other.id))
        , crawler(std::move(other.crawler))
        , createdAt(other.createdAt)
        , isCompleted(other.isCompleted.load())
        , crawlThread(std::move(other.crawlThread))
        , completionCallback(std::move(other.completionCallback)) {}
    
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
                          CrawlCompletionCallback completionCallback = nullptr);
    
    std::vector<CrawlResult> getCrawlResults(const std::string& sessionId);
    std::string getCrawlStatus(const std::string& sessionId);
    bool stopCrawl(const std::string& sessionId);
    std::vector<std::string> getActiveSessions();
    void cleanupCompletedSessions();
    size_t getActiveSessionCount();
    
    // Get access to storage for logging
    std::shared_ptr<search_engine::storage::ContentStorage> getStorage() const { return storage_; }

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


