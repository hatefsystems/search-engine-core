#pragma once

#include <string>
#include <memory>
#include <vector>
#include <queue>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <thread>
#include "models/CrawlResult.h"
#include "models/CrawlConfig.h"
#include "../../include/search_engine/storage/ContentStorage.h"

// Forward declaration for PageFetchResult
struct PageFetchResult;

class URLFrontier;
class RobotsTxtParser;
class PageFetcher;
class ContentParser;
class DomainManager;
class CrawlMetrics;

class Crawler {
public:
    Crawler(const CrawlConfig& config, std::shared_ptr<search_engine::storage::ContentStorage> storage = nullptr, const std::string& sessionId = "");
    ~Crawler();

    // Start the crawling process
    void start();
    
    // Stop the crawling process
    void stop();
    
    // Reset crawler state (clear results, visited URLs, and seed domain)
    void reset();
    
    // Add a seed URL to start crawling from
    void addSeedURL(const std::string& url, bool force = false);
    
    // Get the current crawl results
    std::vector<CrawlResult> getResults();
    const std::vector<CrawlResult>& getResults() const { return results; }
    std::vector<CrawlResult> getResultsCopy() const {
        std::lock_guard<std::mutex> lock(resultsMutex);
        return results;
    }
    
    // Get access to the PageFetcher for configuration
    PageFetcher* getPageFetcher();
    
    // Update crawler configuration
    void setMaxPages(size_t maxPages);
    void setMaxDepth(size_t maxDepth);
    void updateConfig(const CrawlConfig& newConfig);

    // Get current crawler configuration
    CrawlConfig getConfig() const;

    std::shared_ptr<search_engine::storage::ContentStorage> getStorage() const { return storage; }

    // Process a single URL
    CrawlResult processURL(const std::string& url);
    
private:
    // Main crawling loop
    void crawlLoop();
    
    // Extract and add new URLs from the page content
    void extractAndAddURLs(const std::string& content, const std::string& baseURL, int currentDepth = 0);
    
    // Check if URL belongs to the seed domain
    bool isSameDomain(const std::string& url) const;
    
    // Update PageFetcher configuration
    void updatePageFetcherConfig();
    
    // Helper method for session-aware logging
    void logToCrawlSession(const std::string& message, const std::string& level = "info") const;
    
    // Private helper methods for processURL refactoring
    bool validateUrlWithRobotsTxt(const std::string& url, CrawlResult& result);
    void applyCrawlDelay(const std::string& domain);
    bool handleSpaDetectionAndRendering(const std::string& url, PageFetchResult& fetchResult);
    void processHtmlContent(const std::string& url, const PageFetchResult& fetchResult, CrawlResult& result);
    void classifyFailureAndSetResult(const PageFetchResult& fetchResult, CURLcode curlErrorCode, CrawlResult& result);
    
    // Performance optimization helper methods
    size_t getSuccessfulDownloadCount() const;
    void updateResultWithMinimalLocking(const std::string& url, const CrawlResult& newResult);
    void incrementSuccessfulDownloads();
    void decrementSuccessfulDownloads();

    std::unique_ptr<URLFrontier> urlFrontier;
    std::unique_ptr<RobotsTxtParser> robotsParser;
    std::unique_ptr<PageFetcher> pageFetcher;
    std::unique_ptr<ContentParser> contentParser;
    std::unique_ptr<DomainManager> domainManager;
    std::unique_ptr<CrawlMetrics> metrics;
    std::shared_ptr<search_engine::storage::ContentStorage> storage;
    
    CrawlConfig config;
    std::atomic<bool> isRunning;
    std::thread workerThread;
    
    // Separate mutexes for better performance
    mutable std::mutex resultsMutex;
    mutable std::mutex configMutex;
    
    std::vector<CrawlResult> results;
    std::unordered_set<std::string> visitedURLs;
    std::string seedDomain;  // Domain of the first seed URL
    std::string sessionId;   // Session ID for logging
    
    // Performance optimization: atomic counters to reduce mutex contention
    std::atomic<size_t> successfulDownloadCount{0};
    std::atomic<size_t> totalResultCount{0};
    
    // Session-level SPA detection tracking
    std::atomic<bool> sessionSpaDetected{false};  // Track if SPA was detected for this session
    std::atomic<bool> sessionSpaChecked{false};   // Track if SPA detection has been performed for this session
}; 