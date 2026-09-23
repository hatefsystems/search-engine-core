#pragma once

// Crawl priority levels used by both URL-level scheduling (URLFrontier)
// and session-level scheduling (CrawlerManager).
enum class CrawlPriority {
    LOW = 0,
    NORMAL = 1,
    HIGH = 2,
    RETRY = 3  // Higher priority for retries
};
