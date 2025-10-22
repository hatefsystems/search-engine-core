#include "FailureClassifier.h"
#include "../../include/Logger.h"
#include <algorithm>
#include <cmath>

FailureType FailureClassifier::classifyFailure(int httpCode, 
                                             CURLcode curlCode, 
                                             const std::string& errorMessage,
                                             const CrawlConfig& config) {
    
    LOG_DEBUG("Classifying failure - HTTP: " + std::to_string(httpCode) + 
              ", CURL: " + std::to_string(static_cast<int>(curlCode)) + 
              ", Error: " + errorMessage);
    
    // Check for rate limiting first (special case)
    if (httpCode == 429) {
        LOG_DEBUG("Classified as RATE_LIMITED (HTTP 429)");
        return FailureType::RATE_LIMITED;
    }
    
    // Check for permanent HTTP failures
    if (httpCode > 0) {
        if (isPermanentHttpError(httpCode)) {
            LOG_DEBUG("Classified as PERMANENT (HTTP " + std::to_string(httpCode) + ")");
            return FailureType::PERMANENT;
        }
        
        // Check if this HTTP code is in retryable list
        if (config.retryableHttpCodes.find(httpCode) != config.retryableHttpCodes.end()) {
            LOG_DEBUG("Classified as TEMPORARY (retryable HTTP " + std::to_string(httpCode) + ")");
            return FailureType::TEMPORARY;
        }
        
        // Other 5xx server errors should be retried
        if (httpCode >= 500 && httpCode < 600) {
            LOG_DEBUG("Classified as TEMPORARY (5xx server error)");
            return FailureType::TEMPORARY;
        }
    }
    
    // Check CURL errors
    if (curlCode != CURLE_OK) {
        if (isPermanentCurlError(curlCode)) {
            LOG_DEBUG("Classified as PERMANENT (CURL error " + std::to_string(static_cast<int>(curlCode)) + ")");
            return FailureType::PERMANENT;
        }
        
        // Check if this CURL code is in retryable list
        if (config.retryableCurlCodes.find(curlCode) != config.retryableCurlCodes.end()) {
            LOG_DEBUG("Classified as TEMPORARY (retryable CURL error " + std::to_string(static_cast<int>(curlCode)) + ")");
            return FailureType::TEMPORARY;
        }
    }
    
    // Check error message for specific patterns
    std::string lowerError = errorMessage;
    std::transform(lowerError.begin(), lowerError.end(), lowerError.begin(), ::tolower);
    
    // DNS-related errors that might be permanent
    if (lowerError.find("name or service not known") != std::string::npos ||
        lowerError.find("no such host is known") != std::string::npos ||
        lowerError.find("nodename nor servname provided") != std::string::npos) {
        LOG_DEBUG("Classified as PERMANENT (DNS resolution failed)");
        return FailureType::PERMANENT;
    }
    
    // Network timeouts and connection issues are usually temporary
    if (lowerError.find("timeout") != std::string::npos ||
        lowerError.find("connection") != std::string::npos ||
        lowerError.find("network") != std::string::npos) {
        LOG_DEBUG("Classified as TEMPORARY (network/timeout issue)");
        return FailureType::TEMPORARY;
    }
    
    // Treat curl transport/argument issues specially when HTTP code is 0
    if (curlCode != CURLE_OK) {
        if (curlCode == CURLE_BAD_FUNCTION_ARGUMENT || curlCode == CURLE_URL_MALFORMAT) {
            LOG_DEBUG("Classified as PERMANENT (bad argument or malformed URL)");
            return FailureType::PERMANENT;
        }
    }
    // Default to UNKNOWN for unclassified errors
    LOG_DEBUG("Classified as UNKNOWN (unrecognized error pattern)");
    return FailureType::UNKNOWN;
}

bool FailureClassifier::shouldRetry(FailureType failureType, int retryCount, int maxRetries) {
    // Never retry permanent failures
    if (failureType == FailureType::PERMANENT) {
        return false;
    }
    
    // Check retry limit
    if (retryCount >= maxRetries) {
        return false;
    }
    
    // Always retry temporary and rate-limited failures (within limits)
    if (failureType == FailureType::TEMPORARY || failureType == FailureType::RATE_LIMITED) {
        return true;
    }
    
    // For unknown failures, be more conservative - only retry up to half max retries
    if (failureType == FailureType::UNKNOWN) {
        return retryCount < (maxRetries / 2);
    }
    
    return false;
}

std::chrono::milliseconds FailureClassifier::calculateRetryDelay(int retryCount, 
                                                               const CrawlConfig& config,
                                                               FailureType failureType) {
    std::chrono::milliseconds baseDelay = config.baseRetryDelay;
    
    // Special handling for rate-limited responses
    if (failureType == FailureType::RATE_LIMITED) {
        // Use longer base delay for rate limiting
        baseDelay = config.rateLimitDelay;
    }
    
    // Calculate exponential backoff: base * (multiplier ^ (retryCount - 1))
    double multiplier = std::pow(config.backoffMultiplier, retryCount - 1);
    auto calculatedDelay = std::chrono::milliseconds(
        static_cast<long>(baseDelay.count() * multiplier)
    );
    
    // Cap at maximum delay
    auto finalDelay = std::min(calculatedDelay, config.maxRetryDelay);
    
    LOG_DEBUG("Calculated retry delay for attempt " + std::to_string(retryCount) + 
              ": " + std::to_string(finalDelay.count()) + "ms (type: " + 
              getFailureTypeDescription(failureType) + ")");
    
    return finalDelay;
}

std::string FailureClassifier::getFailureTypeDescription(FailureType failureType) {
    switch (failureType) {
        case FailureType::TEMPORARY:
            return "TEMPORARY";
        case FailureType::RATE_LIMITED:
            return "RATE_LIMITED";
        case FailureType::PERMANENT:
            return "PERMANENT";
        case FailureType::UNKNOWN:
            return "UNKNOWN";
        default:
            return "INVALID";
    }
}

bool FailureClassifier::isPermanentHttpError(int httpCode) {
    // Client errors that are permanent
    switch (httpCode) {
        case 400: // Bad Request
        case 401: // Unauthorized
        case 403: // Forbidden
        case 404: // Not Found
        case 405: // Method Not Allowed
        case 406: // Not Acceptable
        case 407: // Proxy Authentication Required
        case 409: // Conflict
        case 410: // Gone
        case 411: // Length Required
        case 412: // Precondition Failed
        case 413: // Payload Too Large
        case 414: // URI Too Long
        case 415: // Unsupported Media Type
        case 416: // Range Not Satisfiable
        case 417: // Expectation Failed
        case 418: // I'm a teapot (RFC 2324)
        case 421: // Misdirected Request
        case 422: // Unprocessable Entity
        case 423: // Locked
        case 424: // Failed Dependency
        case 426: // Upgrade Required
        case 428: // Precondition Required
        case 431: // Request Header Fields Too Large
        case 451: // Unavailable For Legal Reasons
            return true;
        default:
            return false;
    }
}

bool FailureClassifier::isPermanentCurlError(CURLcode curlCode) {
    switch (curlCode) {
        case CURLE_UNSUPPORTED_PROTOCOL:     // Protocol not supported
        case CURLE_FAILED_INIT:              // Failed to initialize
        case CURLE_URL_MALFORMAT:            // URL malformed
        case CURLE_NOT_BUILT_IN:             // Feature not built-in
        case CURLE_COULDNT_RESOLVE_PROXY:    // Couldn't resolve proxy
        case CURLE_COULDNT_RESOLVE_HOST:     // Couldn't resolve host (DNS failure)
        case CURLE_FUNCTION_NOT_FOUND:       // Function not found
        case CURLE_ABORTED_BY_CALLBACK:      // Aborted by callback
        case CURLE_BAD_FUNCTION_ARGUMENT:    // Bad function argument
        case CURLE_INTERFACE_FAILED:         // Interface failed
        case CURLE_TOO_MANY_REDIRECTS:       // Too many redirects
        case CURLE_UNKNOWN_OPTION:           // Unknown option
        case CURLE_SETOPT_OPTION_SYNTAX:     // Setopt option syntax error
        case CURLE_OBSOLETE:                 // Obsolete
            return true;
        default:
            return false;
    }
}