#include <algorithm>
#include "CrawlMetrics.h"
#include "../../include/Logger.h"
#include "../../include/crawler/CrawlLogger.h"
#include <sstream>
#include <iomanip>

void CrawlMetrics::logSummary() const {
    std::ostringstream oss;
    
    // Global metrics
    size_t total = getTotalRequests();
    size_t successful = getSuccessfulRequests();
    size_t failed = getFailedRequests();
    size_t retried = getRetriedRequests();
    size_t permanent = getPermanentFailures();
    size_t circuitBreakers = getCircuitBreakerTriggered();
    size_t rateLimited = getRateLimitedRequests();
    
    oss << "\n=== CRAWL METRICS SUMMARY ===\n";
    oss << "Total Requests: " << total << "\n";
    oss << "Successful: " << successful << " (" << std::fixed << std::setprecision(1) << (getSuccessRate() * 100) << "%)\n";
    oss << "Failed: " << failed << " (" << std::fixed << std::setprecision(1) << (total > 0 ? (static_cast<double>(failed) / total * 100) : 0.0) << "%)\n";
    oss << "Retried: " << retried << " (" << std::fixed << std::setprecision(1) << (getRetryRate() * 100) << "%)\n";
    oss << "Permanent Failures: " << permanent << "\n";
    oss << "Circuit Breakers Triggered: " << circuitBreakers << "\n";
    oss << "Rate Limited: " << rateLimited << "\n";
    
    // Failure type breakdown
    auto failureTypes = getFailureTypeCounts();
    if (!failureTypes.empty()) {
        oss << "\nFailure Types:\n";
        for (const auto& [type, count] : failureTypes) {
            oss << "  " << FailureClassifier::getFailureTypeDescription(type) << ": " << count << "\n";
        }
    }
    
    // Domain metrics
    auto domainMetrics = getAllDomainMetrics();
    if (!domainMetrics.empty()) {
        oss << "\nTop Domains by Request Count:\n";
        
        // Sort domains by total requests
        std::vector<std::pair<std::string, DomainMetricsSnapshot>> sortedDomains;
        for (const auto& [domain, metrics] : domainMetrics) {
            sortedDomains.emplace_back(domain, metrics);
        }
        
        std::sort(sortedDomains.begin(), sortedDomains.end(),
                 [](const auto& a, const auto& b) {
                     return a.second.totalRequests > b.second.totalRequests;
                 });
        
        // Show top 10 domains
        size_t maxDomains = std::min(sortedDomains.size(), size_t(10));
        for (size_t i = 0; i < maxDomains; ++i) {
            const auto& [domain, metrics] = sortedDomains[i];
            size_t domainTotal = metrics.totalRequests;
            size_t domainRetries = metrics.retriedRequests;
            
            oss << "  " << domain << ": " << domainTotal << " requests, "
                << std::fixed << std::setprecision(1) << (metrics.getSuccessRate() * 100) << "% success, "
                << domainRetries << " retries";
            
            if (metrics.circuitBreakerTriggered > 0) {
                oss << " [" << metrics.circuitBreakerTriggered << " circuit breakers]";
            }
            if (metrics.rateLimitedRequests > 0) {
                oss << " [" << metrics.rateLimitedRequests << " rate limited]";
            }
            oss << "\n";
        }
    }
    
    oss << "===============================\n";
    
    std::string summary = oss.str();
    LOG_INFO(summary);
    
    // Also broadcast key metrics to WebSocket
    std::ostringstream wsMessage;
    wsMessage << "📊 Crawl Metrics - Total: " << total 
              << ", Success: " << successful << " (" << std::fixed << std::setprecision(1) << (getSuccessRate() * 100) << "%)"
              << ", Retries: " << retried
              << ", Circuit Breakers: " << circuitBreakers;
    
    CrawlLogger::broadcastLog(wsMessage.str(), "info");
}
