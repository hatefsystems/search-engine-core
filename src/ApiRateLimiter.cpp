#include "../include/ApiRateLimiter.h"
#include "../include/Logger.h"

ApiRateLimiter::ApiRateLimiter(size_t maxRequests, std::chrono::seconds window)
    : maxRequests_(maxRequests), window_(window) {
    LOG_DEBUG("ApiRateLimiter initialized: " + std::to_string(maxRequests) + 
              " requests per " + std::to_string(window.count()) + " seconds");
}

bool ApiRateLimiter::shouldThrottle(const std::string& clientKey) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto& client = clients_[clientKey];
    
    // Remove expired request times
    removeExpiredTimes(client, now);
    
    // Check if we've exceeded the rate limit
    if (client.requestTimes.size() >= maxRequests_) {
        LOG_DEBUG("Rate limit exceeded for client: " + clientKey + 
                 " (" + std::to_string(client.requestTimes.size()) + "/" + 
                 std::to_string(maxRequests_) + ")");
        return true;
    }
    
    // Add current time to the queue
    client.requestTimes.push_back(now);
    LOG_DEBUG("Rate limit check passed for client: " + clientKey + 
             " (" + std::to_string(client.requestTimes.size()) + "/" + 
             std::to_string(maxRequests_) + ")");
    return false;
}

int ApiRateLimiter::getRetryAfter(const std::string& clientKey) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = clients_.find(clientKey);
    if (it == clients_.end() || it->second.requestTimes.empty()) {
        return 0;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto& record = it->second;
    
    // Remove expired times
    removeExpiredTimes(record, now);
    
    if (record.requestTimes.empty()) {
        return 0;
    }
    
    // Calculate when the oldest request will expire
    auto oldestTime = record.requestTimes.front();
    auto resetTime = oldestTime + window_;
    auto remainingTime = std::chrono::duration_cast<std::chrono::seconds>(resetTime - now);
    
    return std::max(1, static_cast<int>(remainingTime.count()));
}

void ApiRateLimiter::cleanup() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::steady_clock::now();
    size_t removedClients = 0;
    
    // Remove expired entries from all clients
    for (auto it = clients_.begin(); it != clients_.end(); ) {
        removeExpiredTimes(it->second, now);
        
        // Remove client record if no recent requests
        if (it->second.requestTimes.empty()) {
            it = clients_.erase(it);
            removedClients++;
        } else {
            ++it;
        }
    }
    
    if (removedClients > 0) {
        LOG_DEBUG("ApiRateLimiter cleanup: removed " + std::to_string(removedClients) + " inactive clients");
    }
}

void ApiRateLimiter::removeExpiredTimes(ClientRecord& record, std::chrono::steady_clock::time_point now) {
    auto cutoffTime = now - window_;
    
    while (!record.requestTimes.empty() && record.requestTimes.front() < cutoffTime) {
        record.requestTimes.pop_front();
    }
}
