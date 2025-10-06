#pragma once

#include <string>
#include <chrono>
#include <vector>
#include <optional>
#include <cstdint>

namespace search_engine {
namespace storage {

enum class CrawlStatus {
    SUCCESS,
    FAILED,
    PENDING,
    TIMEOUT,
    ROBOT_BLOCKED,
    REDIRECT_LOOP,
    CONTENT_TOO_LARGE,
    INVALID_CONTENT_TYPE
};

struct CrawlMetadata {
    std::chrono::system_clock::time_point lastCrawlTime;
    std::chrono::system_clock::time_point firstCrawlTime;
    CrawlStatus lastCrawlStatus;
    std::optional<std::string> lastErrorMessage;
    int crawlCount;
    double crawlIntervalHours;
    std::string userAgent;
    int httpStatusCode;
    size_t contentSize;
    std::string contentType;
    double crawlDurationMs;
};

struct SiteProfile {
    // Unique identifier (MongoDB ObjectId will be auto-generated)
    std::optional<std::string> id;
    
    // Core site information
    std::string domain;               // e.g., "example.com"
    std::string url;                  // Full URL that was crawled
    std::string canonicalUrl;         // Canonicalized URL for deduplication
    std::string canonicalHost;        // Canonicalized host (lowercase, no www)
    std::string canonicalPath;        // Canonicalized path
    std::string canonicalQuery;       // Canonicalized query string
    std::string title;                // Page title
    std::optional<std::string> description;  // Meta description or extracted summary
    std::optional<std::string> textContent;  // Full extracted body text from the page
    
    // Content categorization
    std::vector<std::string> keywords;       // Extracted keywords
    std::optional<std::string> language;     // Detected language
    std::optional<std::string> category;     // Site category (news, blog, etc.)
    
    // Technical metadata
    CrawlMetadata crawlMetadata;
    
    // SEO and content metrics
    std::optional<int> pageRank;             // PageRank score (if calculated)
    std::optional<double> contentQuality;    // Quality score (0-1)
    std::optional<int> wordCount;            // Word count of main content
    std::optional<bool> isMobile;            // Mobile-friendly indicator
    std::optional<bool> hasSSL;              // SSL certificate present
    
    // Link analysis
    std::vector<std::string> outboundLinks;  // Links found on this page
    std::optional<int> inboundLinkCount;     // Count of backlinks (if available)
    
    // Search relevance
    bool isIndexed;                          // Whether page is included in search index
    std::chrono::system_clock::time_point lastModified;  // When content was last modified
    std::chrono::system_clock::time_point indexedAt;     // When it was added to search index
    
    // Additional metadata
    std::optional<std::string> author;       // Content author if detected
    std::optional<std::string> publisher;    // Publisher name
    std::optional<std::chrono::system_clock::time_point> publishDate;  // Content publish date
};

} // namespace storage
} // namespace search_engine 