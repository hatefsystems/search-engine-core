#pragma once

#include <string>
#include <unordered_map>
#include <deque>
#include <chrono>
#include <mutex>

class ApiRateLimiter {
public:
    ApiRateLimiter(size_t maxRequests = 60, std::chrono::seconds window = std::chrono::seconds(60));
    
    // Check if request should be rate limited
    // Returns true if rate limit exceeded (should throttle)
    bool shouldThrottle(const std::string& clientKey);
    
    // Get retry-after time in seconds for a client
    int getRetryAfter(const std::string& clientKey);
    
    // Clear old entries (for cleanup)
    void cleanup();

private:
    struct ClientRecord {
        std::deque<std::chrono::steady_clock::time_point> requestTimes;
    };
    
    size_t maxRequests_;
    std::chrono::seconds window_;
    std::unordered_map<std::string, ClientRecord> clients_;
    mutable std::mutex mutex_;
    
    // Remove expired request times from client record
    void removeExpiredTimes(ClientRecord& record, std::chrono::steady_clock::time_point now);
};
