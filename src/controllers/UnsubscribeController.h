#pragma once
#include "../../include/routing/Controller.h"
#include "../../include/search_engine/storage/UnsubscribeService.h"
#include <memory>
#include <nlohmann/json.hpp>

/**
 * @brief Controller for email unsubscribe functionality
 * 
 * This controller handles API endpoints for unsubscribing from emails,
 * including one-click unsubscribe functionality as per RFC 8058.
 */
class UnsubscribeController : public routing::Controller {
public:
    /**
     * @brief Constructor - follows lazy initialization pattern
     */
    UnsubscribeController();
    
    /**
     * @brief GET /u/{token}
     * One-click unsubscribe via GET request (RFC 8058 compliant)
     * 
     * URL Parameters:
     * - token: Unsubscribe token from email link
     * 
     * Response: HTML page confirming unsubscribe status
     */
    void unsubscribeGet(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    
    /**
     * @brief POST /u/{token}
     * One-click unsubscribe via POST request (List-Unsubscribe-Post header)
     * 
     * URL Parameters:
     * - token: Unsubscribe token
     * 
     * Expected form data:
     * List-Unsubscribe=One-Click
     * 
     * Response: JSON success/failure
     */
    void unsubscribePost(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    
    /**
     * @brief POST /api/v2/unsubscribe
     * API endpoint for unsubscribe requests with additional data
     * 
     * Expected JSON payload:
     * {
     *   "token": "unsubscribe_token_here",
     *   "reason": "Too many emails" // optional
     * }
     */
    void unsubscribeApi(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    
    /**
     * @brief GET /api/v2/unsubscribe/status/{email}
     * Check unsubscribe status for an email address
     * 
     * Response:
     * {
     *   "success": true,
     *   "email": "user@example.com",
     *   "isUnsubscribed": true,
     *   "unsubscribedAt": "2023-01-01T12:00:00Z" // if unsubscribed
     * }
     */
    void getUnsubscribeStatus(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    
    /**
     * @brief POST /api/v2/unsubscribe/reactivate
     * Admin endpoint to reactivate a previously unsubscribed email
     * 
     * Expected JSON payload:
     * {
     *   "email": "user@example.com"
     * }
     */
    void reactivateEmail(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);

private:
    // Lazy initialization pattern - CRITICAL for avoiding static initialization order fiasco
    mutable std::unique_ptr<search_engine::storage::UnsubscribeService> unsubscribeService_;
    
    /**
     * @brief Get or create UnsubscribeService instance (lazy initialization)
     * @return UnsubscribeService instance or nullptr if initialization fails
     */
    search_engine::storage::UnsubscribeService* getUnsubscribeService() const;
    
    /**
     * @brief Extract token from URL path
     * @param url Full URL path
     * @return Token string or empty if not found
     */
    std::string extractTokenFromUrl(const std::string& url) const;
    
    /**
     * @brief Extract email from URL path
     * @param url Full URL path
     * @return Email string or empty if not found
     */
    std::string extractEmailFromUrl(const std::string& url) const;
    
    /**
     * @brief Get client IP address from request headers
     * @param req HTTP request object
     * @return IP address string
     */
    std::string getClientIpAddress(uWS::HttpRequest* req) const;
    
    /**
     * @brief Get user agent from request headers
     * @param req HTTP request object
     * @return User agent string
     */
    std::string getUserAgent(uWS::HttpRequest* req) const;
    
    /**
     * @brief Render unsubscribe success page
     * @param email Email that was unsubscribed
     * @return HTML content
     */
    std::string renderUnsubscribeSuccessPage(const std::string& email) const;
    
    /**
     * @brief Render unsubscribe error page
     * @param errorMessage Error message to display
     * @return HTML content
     */
    std::string renderUnsubscribeErrorPage(const std::string& errorMessage) const;
    
    /**
     * @brief Format timestamp for JSON response
     * @param timePoint Time point to format
     * @return ISO 8601 formatted string
     */
    std::string formatTimestamp(const std::chrono::system_clock::time_point& timePoint) const;
};

// Route registration using macros (similar to .NET Core attributes)
ROUTE_CONTROLLER(UnsubscribeController) {
    using namespace routing;
    REGISTER_ROUTE(HttpMethod::GET, "/u/*", unsubscribeGet, UnsubscribeController);
    REGISTER_ROUTE(HttpMethod::POST, "/u/*", unsubscribePost, UnsubscribeController);
    REGISTER_ROUTE(HttpMethod::POST, "/api/v2/unsubscribe", unsubscribeApi, UnsubscribeController);
    REGISTER_ROUTE(HttpMethod::GET, "/api/v2/unsubscribe/status/*", getUnsubscribeStatus, UnsubscribeController);
    REGISTER_ROUTE(HttpMethod::POST, "/api/v2/unsubscribe/reactivate", reactivateEmail, UnsubscribeController);
}
