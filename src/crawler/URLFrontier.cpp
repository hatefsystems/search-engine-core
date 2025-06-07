#include "URLFrontier.h"
#include "../../include/Logger.h"
#include <algorithm>
#include <regex>
#include <sstream>
#include <iomanip>

URLFrontier::URLFrontier() {
    LOG_DEBUG("URLFrontier constructor called");
}

URLFrontier::~URLFrontier() {
    LOG_DEBUG("URLFrontier destructor called");
}

void URLFrontier::addURL(const std::string& url) {
    LOG_DEBUG("URLFrontier::addURL called with: " + url);
    std::string normalizedURL = normalizeURL(url);
    std::string domain = extractDomain(normalizedURL);
    
    // Check both visited URLs and queue for duplicates
    {
    std::lock_guard<std::mutex> visitedLock(visitedMutex);
    if (visitedURLs.find(normalizedURL) != visitedURLs.end()) {
            LOG_DEBUG("URL already visited, skipping: " + normalizedURL);
        return;
        }
    }
    
    {
    std::lock_guard<std::mutex> queueLock(queueMutex);
        // Check if URL is already in queue
        std::queue<std::string> tempQueue;
        bool found = false;
        while (!urlQueue.empty()) {
            std::string current = urlQueue.front();
            urlQueue.pop();
            if (current == normalizedURL) {
                found = true;
            }
            tempQueue.push(current);
        }
        // Restore queue
        while (!tempQueue.empty()) {
            urlQueue.push(tempQueue.front());
            tempQueue.pop();
        }
        
        if (found) {
            LOG_DEBUG("URL already in queue, skipping: " + normalizedURL);
            return;
        }
        
    urlQueue.push(normalizedURL);
        LOG_DEBUG("Added URL to queue: " + normalizedURL + ", queue size: " + std::to_string(urlQueue.size()));
    }
}

std::string URLFrontier::getNextURL() {
    std::lock_guard<std::mutex> lock(queueMutex);
    if (urlQueue.empty()) {
        LOG_DEBUG("URLFrontier::getNextURL - Queue is empty, returning empty string");
        return "";
    }
    
    std::string url = urlQueue.front();
    urlQueue.pop();
    LOG_DEBUG("URLFrontier::getNextURL - Retrieved URL: " + url + ", remaining queue size: " + std::to_string(urlQueue.size()));
    return url;
}

bool URLFrontier::isEmpty() const {
    std::lock_guard<std::mutex> lock(queueMutex);
    bool empty = urlQueue.empty();
    LOG_TRACE("URLFrontier::isEmpty - Queue is " + std::string(empty ? "empty" : "not empty"));
    return empty;
}

size_t URLFrontier::size() const {
    std::lock_guard<std::mutex> lock(queueMutex);
    size_t size = urlQueue.size();
    LOG_TRACE("URLFrontier::size - Queue size: " + std::to_string(size));
    return size;
}

void URLFrontier::markVisited(const std::string& url) {
    LOG_DEBUG("URLFrontier::markVisited called with: " + url);
    std::string normalizedURL = normalizeURL(url);
    std::string domain = extractDomain(normalizedURL);
    
    std::lock_guard<std::mutex> visitedLock(visitedMutex);
    visitedURLs.insert(normalizedURL);
    LOG_DEBUG("Marked URL as visited: " + normalizedURL + ", visited URLs count: " + std::to_string(visitedURLs.size()));
    
    std::lock_guard<std::mutex> domainLock(domainMutex);
    domainLastVisit[domain] = std::chrono::system_clock::now();
    LOG_DEBUG("Updated last visit time for domain: " + domain);
}

bool URLFrontier::isVisited(const std::string& url) const {
    std::string normalizedURL = normalizeURL(url);
    std::lock_guard<std::mutex> lock(visitedMutex);
    bool visited = visitedURLs.find(normalizedURL) != visitedURLs.end();
    LOG_TRACE("URLFrontier::isVisited - URL: " + url + " is " + (visited ? "visited" : "not visited"));
    return visited;
}

std::chrono::system_clock::time_point URLFrontier::getLastVisitTime(const std::string& domain) const {
    std::lock_guard<std::mutex> lock(domainMutex);
    auto it = domainLastVisit.find(domain);
    if (it != domainLastVisit.end()) {
        LOG_TRACE("URLFrontier::getLastVisitTime - Domain: " + domain + " has a last visit time");
        return it->second;
    }
    LOG_TRACE("URLFrontier::getLastVisitTime - Domain: " + domain + " has no last visit time");
    return std::chrono::system_clock::time_point::min();
}

std::string URLFrontier::extractDomain(const std::string& url) const {
    // Extract host portion without scheme and port
    static const std::regex domainRegex(R"(https?://([^/:]+))", std::regex::icase);
    std::smatch matches;
    if (std::regex_search(url, matches, domainRegex) && matches.size() > 1) {
        std::string domain = matches[1].str();
        LOG_TRACE("URLFrontier::extractDomain - Extracted domain: " + domain + " from URL: " + url);
        return domain;
    }
    LOG_WARNING("URLFrontier::extractDomain - Could not extract domain from URL: " + url);
    return "";
}

std::string URLFrontier::normalizeURL(const std::string& url) const {
    std::string normalized = url;
    
    // Convert to lowercase
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
    
    // Remove trailing slash
    if (!normalized.empty() && normalized.back() == '/') {
        normalized.pop_back();
    }
    
    // Remove fragment
    size_t hashPos = normalized.find('#');
    if (hashPos != std::string::npos) {
        normalized = normalized.substr(0, hashPos);
    }
    
    LOG_TRACE("URLFrontier::normalizeURL - Original: " + url + " Normalized: " + normalized);
    return normalized;
} 