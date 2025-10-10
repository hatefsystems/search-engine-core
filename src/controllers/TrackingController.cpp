#include "TrackingController.h"
#include "../../include/Logger.h"
#include <nlohmann/json.hpp>
#include <regex>

TrackingController::TrackingController() {
    // Empty constructor - use lazy initialization pattern
    LOG_DEBUG("TrackingController: Constructor called (lazy initialization)");
}

search_engine::storage::EmailTrackingStorage* TrackingController::getTrackingStorage() const {
    if (!trackingStorage_) {
        try {
            LOG_INFO("TrackingController: Lazy initializing EmailTrackingStorage");
            trackingStorage_ = std::make_unique<search_engine::storage::EmailTrackingStorage>();
        } catch (const std::exception& e) {
            LOG_ERROR("TrackingController: Failed to lazy initialize EmailTrackingStorage: " + std::string(e.what()));
            return nullptr;
        }
    }
    return trackingStorage_.get();
}

void TrackingController::trackEmailOpen(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    try {
        // Extract tracking ID from URL path
        std::string path = std::string(req->getUrl());
        
        // Remove /track/ prefix and optional .png suffix
        // Match hex characters (case insensitive) of any length
        std::regex trackingIdRegex("/track/([a-fA-F0-9]+)(?:\\.png)?");
        std::smatch matches;
        std::string trackingId;
        
        if (std::regex_search(path, matches, trackingIdRegex) && matches.size() > 1) {
            trackingId = matches[1].str();
        } else {
            LOG_WARNING("TrackingController: Invalid tracking URL format: " + path);
            serveTrackingPixel(res); // Still serve pixel to avoid broken images
            return;
        }
        
        LOG_DEBUG("TrackingController: Tracking email open for ID: " + trackingId);
        
        // Get client IP and user agent
        std::string clientIP = getClientIP(req);
        std::string userAgent = getUserAgent(req);
        
        LOG_DEBUG("TrackingController: Client IP: " + clientIP + ", User-Agent: " + userAgent);
        
        // Record email open event
        auto storage = getTrackingStorage();
        if (storage) {
            auto result = storage->recordEmailOpen(trackingId, clientIP, userAgent);
            
            if (result.success) {
                LOG_INFO("TrackingController: Email open recorded successfully for tracking ID: " + trackingId);
            } else {
                LOG_WARNING("TrackingController: Failed to record email open: " + result.message);
            }
        } else {
            LOG_ERROR("TrackingController: EmailTrackingStorage unavailable");
        }
        
        // Always serve the tracking pixel regardless of success/failure
        serveTrackingPixel(res);
        
    } catch (const std::exception& e) {
        LOG_ERROR("TrackingController: Exception in trackEmailOpen: " + std::string(e.what()));
        serveTrackingPixel(res); // Still serve pixel even on error
    }
}

void TrackingController::getTrackingStats(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    try {
        // Extract email parameter from query string
        std::string queryString = std::string(req->getQuery());
        std::string emailAddress;
        
        // Simple query string parsing for "email=" parameter
        size_t emailPos = queryString.find("email=");
        if (emailPos != std::string::npos) {
            size_t start = emailPos + 6; // Length of "email="
            size_t end = queryString.find("&", start);
            if (end == std::string::npos) {
                emailAddress = queryString.substr(start);
            } else {
                emailAddress = queryString.substr(start, end - start);
            }
        }
        
        if (emailAddress.empty()) {
            badRequest(res, "Email parameter is required");
            return;
        }
        
        LOG_DEBUG("TrackingController: Getting tracking stats for email: " + emailAddress);
        
        // Get tracking stats
        auto storage = getTrackingStorage();
        if (!storage) {
            LOG_ERROR("TrackingController: EmailTrackingStorage unavailable");
            serverError(res, "Tracking storage unavailable");
            return;
        }
        
        auto result = storage->getTrackingStats(emailAddress);
        
        if (result.success) {
            // Parse JSON to validate and format
            nlohmann::json stats = nlohmann::json::parse(result.value);
            
            nlohmann::json response;
            response["success"] = true;
            response["message"] = "Tracking stats retrieved successfully";
            response["data"] = stats;
            
            json(res, response);
            LOG_INFO("TrackingController: Retrieved tracking stats for email: " + emailAddress);
        } else {
            LOG_ERROR("TrackingController: Failed to get tracking stats: " + result.message);
            serverError(res, "Failed to retrieve tracking stats: " + result.message);
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("TrackingController: Exception in getTrackingStats: " + std::string(e.what()));
        serverError(res, "Internal server error");
    }
}

void TrackingController::serveTrackingPixel(uWS::HttpResponse<false>* res) {
    // 1x1 transparent PNG pixel (base64 decoded)
    // This is the smallest valid PNG image (67 bytes)
    static const unsigned char pixelData[] = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 
        0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 
        0x08, 0x06, 0x00, 0x00, 0x00, 0x1F, 0x15, 0xC4, 0x89, 0x00, 0x00, 0x00, 
        0x0A, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9C, 0x63, 0x00, 0x01, 0x00, 0x00, 
        0x05, 0x00, 0x01, 0x0D, 0x0A, 0x2D, 0xB4, 0x00, 0x00, 0x00, 0x00, 0x49, 
        0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82
    };
    
    // Set headers
    res->writeStatus("200 OK");
    res->writeHeader("Content-Type", "image/png");
    res->writeHeader("Content-Length", std::to_string(sizeof(pixelData)));
    res->writeHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    res->writeHeader("Pragma", "no-cache");
    res->writeHeader("Expires", "0");
    
    // Write pixel data
    res->end(std::string_view(reinterpret_cast<const char*>(pixelData), sizeof(pixelData)));
}

std::string TrackingController::getClientIP(uWS::HttpRequest* req) {
    // Try to get IP from X-Forwarded-For header first (for proxied requests)
    std::string xForwardedFor = std::string(req->getHeader("x-forwarded-for"));
    if (!xForwardedFor.empty()) {
        // X-Forwarded-For can contain multiple IPs, get the first one
        size_t commaPos = xForwardedFor.find(",");
        if (commaPos != std::string::npos) {
            return xForwardedFor.substr(0, commaPos);
        }
        return xForwardedFor;
    }
    
    // Try X-Real-IP header
    std::string xRealIP = std::string(req->getHeader("x-real-ip"));
    if (!xRealIP.empty()) {
        return xRealIP;
    }
    
    // Fallback to remote address
    std::string remoteAddress = std::string(req->getHeader("x-forwarded-for"));
    if (remoteAddress.empty()) {
        return "unknown";
    }
    
    return remoteAddress;
}

std::string TrackingController::getUserAgent(uWS::HttpRequest* req) {
    std::string userAgent = std::string(req->getHeader("user-agent"));
    if (userAgent.empty()) {
        return "unknown";
    }
    return userAgent;
}

