#pragma once

#include <string>
#include <chrono>
#include <set>
#include <curl/curl.h>

struct CrawlConfig {
    size_t maxPages = 1000;
    size_t maxDepth = 5;
    std::chrono::milliseconds politenessDelay{500};
    std::string userAgent = "Hatefbot/1.0";
    size_t maxConcurrentConnections = 5;
    std::chrono::milliseconds requestTimeout{15000}; // Will be overridden by environment variable if set
    bool respectRobotsTxt = true;
    bool followRedirects = true;
    size_t maxRedirects = 5;
    bool storeRawContent = true;
    bool extractTextContent = true;
    bool restrictToSeedDomain = true;
    bool spaRenderingEnabled = false;
    std::string browserlessUrl = "http://browserless:3000";
    // When true, prefer WebSocket/CDP transport to Browserless; falls back to HTTP on failure
    bool useWebsocketForBrowserless = true;
    // Size of WebSocket connection pool per CPU (1 means one ws per CPU)
    size_t wsConnectionsPerCpu = 1;
    bool includeFullContent = false;

    int maxRetries = 3;
    std::chrono::milliseconds baseRetryDelay{1000};
    float backoffMultiplier = 2.0f;
    std::chrono::milliseconds maxRetryDelay{30000};
    
    // Maximum session duration to prevent infinite crawling (default: 10 minutes)
    std::chrono::minutes maxSessionDuration{10};

    std::set<int> retryableHttpCodes = {408,429,500,502,503,504,520,521,522,523,524};
    std::set<CURLcode> retryableCurlCodes = {
        CURLE_OPERATION_TIMEDOUT,
        CURLE_COULDNT_CONNECT,
        // CURLE_COULDNT_RESOLVE_HOST removed - DNS failures are permanent
        CURLE_RECV_ERROR,
        CURLE_SEND_ERROR,
        CURLE_GOT_NOTHING,
        CURLE_PARTIAL_FILE,
        CURLE_SSL_CONNECT_ERROR,
        CURLE_PEER_FAILED_VERIFICATION
    };

    int circuitBreakerFailureThreshold = 5;
    std::chrono::minutes circuitBreakerResetTime{5};
    std::chrono::seconds rateLimitDelay{60};

    // Kafka frontier (durable queue) configuration
    bool useKafkaFrontier = false;
    std::string kafkaBootstrapServers = "kafka:9092";
    std::string kafkaTopic = "crawl.frontier";
};


