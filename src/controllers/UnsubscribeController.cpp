#include "UnsubscribeController.h"
#include "../../include/Logger.h"
#include <regex>
#include <iomanip>
#include <sstream>
#include <chrono>

UnsubscribeController::UnsubscribeController() {
    // Empty constructor - using lazy initialization pattern
    LOG_DEBUG("UnsubscribeController: Constructor called");
}

search_engine::storage::UnsubscribeService* UnsubscribeController::getUnsubscribeService() const {
    if (!unsubscribeService_) {
        try {
            LOG_INFO("UnsubscribeController: Lazy initializing UnsubscribeService");
            unsubscribeService_ = std::make_unique<search_engine::storage::UnsubscribeService>();
        } catch (const std::exception& e) {
            LOG_ERROR("UnsubscribeController: Failed to lazy initialize UnsubscribeService: " + std::string(e.what()));
            return nullptr;
        }
    }
    return unsubscribeService_.get();
}

void UnsubscribeController::unsubscribeGet(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    LOG_INFO("UnsubscribeController::unsubscribeGet - Processing GET unsubscribe request");
    
    try {
        // Extract token from URL
        std::string url = std::string(req->getUrl());
        std::string token = extractTokenFromUrl(url);
        
        if (token.empty()) {
            LOG_WARNING("UnsubscribeController: Invalid unsubscribe URL - no token found");
            std::string errorPage = renderUnsubscribeErrorPage("Invalid unsubscribe link. Please use the link from your email.");
            html(res, errorPage);
            return;
        }
        
        LOG_DEBUG("UnsubscribeController: Processing token: " + token.substr(0, 8) + "...");
        
        // Get client information
        std::string ipAddress = getClientIpAddress(req);
        std::string userAgent = getUserAgent(req);
        
        // Get unsubscribe service
        auto service = getUnsubscribeService();
        if (!service) {
            LOG_ERROR("UnsubscribeController: UnsubscribeService unavailable");
            std::string errorPage = renderUnsubscribeErrorPage("Service temporarily unavailable. Please try again later.");
            html(res, errorPage);
            return;
        }
        
        // Process unsubscribe
        bool success = service->processUnsubscribe(token, ipAddress, userAgent);
        
        if (success) {
            // Get the email address for confirmation page
            auto tokenRecord = service->getUnsubscribeByToken(token);
            std::string email = tokenRecord.has_value() ? tokenRecord->email : "your email";
            
            LOG_INFO("UnsubscribeController: Successfully unsubscribed: " + email);
            std::string successPage = renderUnsubscribeSuccessPage(email);
            html(res, successPage);
        } else {
            LOG_WARNING("UnsubscribeController: Failed to process unsubscribe for token: " + token.substr(0, 8) + "...");
            std::string errorPage = renderUnsubscribeErrorPage("Unable to process unsubscribe request. The link may be invalid or expired.");
            html(res, errorPage);
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("UnsubscribeController::unsubscribeGet - Exception: " + std::string(e.what()));
        std::string errorPage = renderUnsubscribeErrorPage("An error occurred processing your request. Please try again later.");
        html(res, errorPage);
    }
}

void UnsubscribeController::unsubscribePost(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    LOG_INFO("UnsubscribeController::unsubscribePost - Processing POST unsubscribe request");
    
    // Extract token from URL
    std::string url = std::string(req->getUrl());
    std::string token = extractTokenFromUrl(url);
    
    if (token.empty()) {
        LOG_WARNING("UnsubscribeController: Invalid unsubscribe URL - no token found");
        badRequest(res, "Invalid unsubscribe URL");
        return;
    }
    
    std::string buffer;
    res->onData([this, res, token, buffer = std::move(buffer)](std::string_view data, bool last) mutable {
        buffer.append(data.data(), data.length());
        
        if (last) {
            try {
                LOG_DEBUG("UnsubscribeController: Processing POST data: " + buffer);
                
                // Check for List-Unsubscribe-Post compliance
                if (buffer.find("List-Unsubscribe=One-Click") == std::string::npos) {
                    LOG_WARNING("UnsubscribeController: POST request missing List-Unsubscribe=One-Click");
                    badRequest(res, "Invalid unsubscribe request");
                    return;
                }
                
                // Get client information
                std::string ipAddress = getClientIpAddress(nullptr); // Can't get from lambda context
                std::string userAgent = getUserAgent(nullptr); // Can't get from lambda context
                
                // Get unsubscribe service
                auto service = getUnsubscribeService();
                if (!service) {
                    LOG_ERROR("UnsubscribeController: UnsubscribeService unavailable");
                    serverError(res, "Service temporarily unavailable");
                    return;
                }
                
                // Process unsubscribe
                bool success = service->processUnsubscribe(token, ipAddress, userAgent);
                
                nlohmann::json response;
                response["success"] = success;
                
                if (success) {
                    auto tokenRecord = service->getUnsubscribeByToken(token);
                    std::string email = tokenRecord.has_value() ? tokenRecord->email : "";
                    
                    response["message"] = "Successfully unsubscribed";
                    response["email"] = email;
                    
                    LOG_INFO("UnsubscribeController: Successfully unsubscribed via POST: " + email);
                } else {
                    response["message"] = "Failed to process unsubscribe request";
                    LOG_WARNING("UnsubscribeController: Failed to process POST unsubscribe for token: " + token.substr(0, 8) + "...");
                }
                
                json(res, response);
                
            } catch (const std::exception& e) {
                LOG_ERROR("UnsubscribeController::unsubscribePost - Exception: " + std::string(e.what()));
                serverError(res, "Internal server error occurred");
            }
        }
    });

    // CRITICAL: Always add onAborted callback to prevent server crashes
    res->onAborted([this]() {
        LOG_WARNING("UnsubscribeController::unsubscribePost - Client disconnected during request processing");
    });
}

void UnsubscribeController::unsubscribeApi(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    LOG_INFO("UnsubscribeController::unsubscribeApi - Processing API unsubscribe request");
    LOG_DEBUG("Request from: " + std::string(req->getHeader("user-agent")).substr(0, 50) + "...");
    
    std::string buffer;
    res->onData([this, res, buffer = std::move(buffer)](std::string_view data, bool last) mutable {
        buffer.append(data.data(), data.length());
        
        if (last) {
            try {
                auto jsonBody = nlohmann::json::parse(buffer);
                
                // Validate required fields
                if (!jsonBody.contains("token")) {
                    badRequest(res, "Missing required field: token");
                    return;
                }
                
                std::string token = jsonBody["token"].get<std::string>();
                std::optional<std::string> reason = std::nullopt;
                
                if (jsonBody.contains("reason") && !jsonBody["reason"].is_null()) {
                    reason = jsonBody["reason"].get<std::string>();
                }
                
                if (token.empty()) {
                    badRequest(res, "Token cannot be empty");
                    return;
                }
                
                LOG_DEBUG("UnsubscribeController: Processing API unsubscribe for token: " + token.substr(0, 8) + "...");
                
                // Get client information
                std::string ipAddress = getClientIpAddress(nullptr); // Can't get from lambda context
                std::string userAgent = getUserAgent(nullptr); // Can't get from lambda context
                
                // Get unsubscribe service
                auto service = getUnsubscribeService();
                if (!service) {
                    LOG_ERROR("UnsubscribeController: UnsubscribeService unavailable");
                    serverError(res, "Service temporarily unavailable");
                    return;
                }
                
                // Process unsubscribe
                bool success = service->processUnsubscribe(token, ipAddress, userAgent, reason);
                
                nlohmann::json response;
                response["success"] = success;
                
                if (success) {
                    auto tokenRecord = service->getUnsubscribeByToken(token);
                    std::string email = tokenRecord.has_value() ? tokenRecord->email : "";
                    
                    response["message"] = "Successfully unsubscribed";
                    response["data"] = {
                        {"email", email},
                        {"unsubscribedAt", formatTimestamp(std::chrono::system_clock::now())}
                    };
                    
                    LOG_INFO("UnsubscribeController: Successfully processed API unsubscribe for: " + email);
                    json(res, response);
                } else {
                    response["message"] = "Failed to process unsubscribe request";
                    response["error"] = "UNSUBSCRIBE_FAILED";
                    
                    LOG_WARNING("UnsubscribeController: Failed to process API unsubscribe for token: " + token.substr(0, 8) + "...");
                    badRequest(res, response);
                }
                
            } catch (const nlohmann::json::parse_error& e) {
                LOG_ERROR("UnsubscribeController::unsubscribeApi - JSON parse error: " + std::string(e.what()));
                badRequest(res, "Invalid JSON format");
            } catch (const std::exception& e) {
                LOG_ERROR("UnsubscribeController::unsubscribeApi - Exception: " + std::string(e.what()));
                serverError(res, "Internal server error occurred");
            }
        }
    });

    // CRITICAL: Always add onAborted callback to prevent server crashes
    res->onAborted([this]() {
        LOG_WARNING("UnsubscribeController::unsubscribeApi - Client disconnected during request processing");
    });
}

void UnsubscribeController::getUnsubscribeStatus(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    LOG_INFO("UnsubscribeController::getUnsubscribeStatus - Checking unsubscribe status");
    
    try {
        // Extract email from URL
        std::string url = std::string(req->getUrl());
        std::string email = extractEmailFromUrl(url);
        
        if (email.empty()) {
            LOG_WARNING("UnsubscribeController: Invalid status check URL - no email found");
            badRequest(res, "Invalid email parameter");
            return;
        }
        
        LOG_DEBUG("UnsubscribeController: Checking status for: " + email);
        
        // Get unsubscribe service
        auto service = getUnsubscribeService();
        if (!service) {
            LOG_ERROR("UnsubscribeController: UnsubscribeService unavailable");
            serverError(res, "Service temporarily unavailable");
            return;
        }
        
        // Check unsubscribe status
        bool isUnsubscribed = service->isEmailUnsubscribed(email);
        
        nlohmann::json response;
        response["success"] = true;
        response["email"] = email;
        response["isUnsubscribed"] = isUnsubscribed;
        
        if (isUnsubscribed) {
            auto record = service->getUnsubscribeByEmail(email);
            if (record.has_value()) {
                response["unsubscribedAt"] = formatTimestamp(record->unsubscribedAt);
                if (record->reason.has_value()) {
                    response["reason"] = record->reason.value();
                }
            }
        }
        
        LOG_DEBUG("UnsubscribeController: Status check result for " + email + ": " + (isUnsubscribed ? "UNSUBSCRIBED" : "SUBSCRIBED"));
        json(res, response);
        
    } catch (const std::exception& e) {
        LOG_ERROR("UnsubscribeController::getUnsubscribeStatus - Exception: " + std::string(e.what()));
        serverError(res, "Internal server error occurred");
    }
}

void UnsubscribeController::reactivateEmail(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    LOG_INFO("UnsubscribeController::reactivateEmail - Processing email reactivation request");
    
    std::string buffer;
    res->onData([this, res, buffer = std::move(buffer)](std::string_view data, bool last) mutable {
        buffer.append(data.data(), data.length());
        
        if (last) {
            try {
                auto jsonBody = nlohmann::json::parse(buffer);
                
                // Validate required fields
                if (!jsonBody.contains("email")) {
                    badRequest(res, "Missing required field: email");
                    return;
                }
                
                std::string email = jsonBody["email"].get<std::string>();
                
                if (email.empty()) {
                    badRequest(res, "Email cannot be empty");
                    return;
                }
                
                LOG_DEBUG("UnsubscribeController: Reactivating email: " + email);
                
                // Get unsubscribe service
                auto service = getUnsubscribeService();
                if (!service) {
                    LOG_ERROR("UnsubscribeController: UnsubscribeService unavailable");
                    serverError(res, "Service temporarily unavailable");
                    return;
                }
                
                // Reactivate email
                bool success = service->reactivateEmail(email);
                
                nlohmann::json response;
                response["success"] = success;
                
                if (success) {
                    response["message"] = "Email successfully reactivated";
                    response["data"] = {
                        {"email", email},
                        {"reactivatedAt", formatTimestamp(std::chrono::system_clock::now())}
                    };
                    
                    LOG_INFO("UnsubscribeController: Successfully reactivated email: " + email);
                    json(res, response);
                } else {
                    response["message"] = "Failed to reactivate email";
                    response["error"] = "REACTIVATION_FAILED";
                    
                    LOG_WARNING("UnsubscribeController: Failed to reactivate email: " + email);
                    serverError(res, response);
                }
                
            } catch (const nlohmann::json::parse_error& e) {
                LOG_ERROR("UnsubscribeController::reactivateEmail - JSON parse error: " + std::string(e.what()));
                badRequest(res, "Invalid JSON format");
            } catch (const std::exception& e) {
                LOG_ERROR("UnsubscribeController::reactivateEmail - Exception: " + std::string(e.what()));
                serverError(res, "Internal server error occurred");
            }
        }
    });

    // CRITICAL: Always add onAborted callback to prevent server crashes
    res->onAborted([this]() {
        LOG_WARNING("UnsubscribeController::reactivateEmail - Client disconnected during request processing");
    });
}

std::string UnsubscribeController::extractTokenFromUrl(const std::string& url) const {
    // Expected URL format: /u/{token} or /api/v2/unsubscribe/{token}
    std::regex tokenRegex(R"(/u/([a-fA-F0-9]{64}))");
    std::smatch matches;
    
    if (std::regex_search(url, matches, tokenRegex)) {
        return matches[1].str();
    }
    
    LOG_DEBUG("UnsubscribeController: No token found in URL: " + url);
    return "";
}

std::string UnsubscribeController::extractEmailFromUrl(const std::string& url) const {
    // Expected URL format: /api/v2/unsubscribe/status/{email}
    std::regex emailRegex(R"(/status/([^/]+)$)");
    std::smatch matches;
    
    if (std::regex_search(url, matches, emailRegex)) {
        std::string email = matches[1].str();
        // URL decode if needed (basic implementation)
        std::regex percentRegex(R"(%40)");
        email = std::regex_replace(email, percentRegex, "@");
        return email;
    }
    
    LOG_DEBUG("UnsubscribeController: No email found in URL: " + url);
    return "";
}

std::string UnsubscribeController::getClientIpAddress(uWS::HttpRequest* req) const {
    if (!req) return "127.0.0.1"; // Default for cases where req is not available
    
    // Check X-Forwarded-For header (proxy/load balancer)
    std::string forwardedFor = std::string(req->getHeader("x-forwarded-for"));
    if (!forwardedFor.empty()) {
        // Take the first IP from comma-separated list
        size_t commaPos = forwardedFor.find(',');
        return commaPos != std::string::npos ? forwardedFor.substr(0, commaPos) : forwardedFor;
    }
    
    // Check X-Real-IP header (nginx)
    std::string realIp = std::string(req->getHeader("x-real-ip"));
    if (!realIp.empty()) {
        return realIp;
    }
    
    // Fallback to remote address (note: uWS doesn't provide direct access to remote IP)
    return "127.0.0.1";
}

std::string UnsubscribeController::getUserAgent(uWS::HttpRequest* req) const {
    if (!req) return "Unknown";
    
    std::string userAgent = std::string(req->getHeader("user-agent"));
    return userAgent.empty() ? "Unknown" : userAgent;
}

std::string UnsubscribeController::renderUnsubscribeSuccessPage(const std::string& email) const {
    return R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Unsubscribed Successfully</title>
    <style>
        body { font-family: Arial, sans-serif; max-width: 600px; margin: 50px auto; padding: 20px; text-align: center; }
        .success { color: #28a745; font-size: 24px; margin-bottom: 20px; }
        .email { background: #f8f9fa; padding: 10px; border-radius: 4px; margin: 20px 0; }
        .footer { margin-top: 40px; font-size: 14px; color: #666; }
    </style>
</head>
<body>
    <div class="success">✓ Successfully Unsubscribed</div>
    <p>You have been successfully unsubscribed from email notifications.</p>
    <div class="email">)" + email + R"(</div>
    <p>You will no longer receive crawling notification emails at this address.</p>
    <div class="footer">
        <p>If you change your mind, you can request to be re-subscribed by contacting support.</p>
        <p>&copy; 2025 Hatef.ir Search Engine</p>
    </div>
</body>
</html>)";
}

std::string UnsubscribeController::renderUnsubscribeErrorPage(const std::string& errorMessage) const {
    return R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Unsubscribe Error</title>
    <style>
        body { font-family: Arial, sans-serif; max-width: 600px; margin: 50px auto; padding: 20px; text-align: center; }
        .error { color: #dc3545; font-size: 24px; margin-bottom: 20px; }
        .message { background: #f8f9fa; padding: 20px; border-radius: 4px; margin: 20px 0; }
        .footer { margin-top: 40px; font-size: 14px; color: #666; }
    </style>
</head>
<body>
    <div class="error">✗ Unsubscribe Error</div>
    <div class="message">)" + errorMessage + R"(</div>
    <p>If you continue to have problems, please contact support.</p>
    <div class="footer">
        <p>&copy; 2025 Hatef.ir Search Engine</p>
    </div>
</body>
</html>)";
}

std::string UnsubscribeController::formatTimestamp(const std::chrono::system_clock::time_point& timePoint) const {
    auto time_t = std::chrono::system_clock::to_time_t(timePoint);
    auto tm = std::gmtime(&time_t);
    
    std::stringstream ss;
    ss << std::put_time(tm, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}
