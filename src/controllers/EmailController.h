#pragma once
#include "../../include/routing/Controller.h"
#include "../../include/search_engine/storage/EmailService.h"
#include <memory>
#include <nlohmann/json.hpp>

/**
 * @brief Controller for email notification functionality
 * 
 * This controller handles API endpoints for sending email notifications,
 * particularly for crawling completion notifications.
 */
class EmailController : public routing::Controller {
public:
    /**
     * @brief Constructor - follows lazy initialization pattern
     */
    EmailController();
    
    /**
     * @brief POST /api/v2/send-crawling-notification
     * Send crawling completion notification email
     * 
     * Expected JSON payload:
     * {
     *   "recipientEmail": "user@example.com",
     *   "recipientName": "John Doe",
     *   "domainName": "example.com",
     *   "crawledPagesCount": 150,
     *   "crawlSessionId": "session_123",
     *   "language": "en" // optional, defaults to "en"
     * }
     */
    void sendCrawlingNotification(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    
    /**
     * @brief POST /api/v2/send-email
     * Send generic HTML email
     * 
     * Expected JSON payload:
     * {
     *   "to": "user@example.com",
     *   "subject": "Email Subject",
     *   "htmlContent": "<html>...</html>",
     *   "textContent": "Plain text fallback" // optional
     * }
     */
    void sendEmail(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    
    /**
     * @brief GET /api/v2/email-service-status
     * Test email service connection and return status
     */
    void getEmailServiceStatus(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);

private:
    // Lazy initialization pattern - CRITICAL for avoiding static initialization order fiasco
    mutable std::unique_ptr<search_engine::storage::EmailService> emailService_;
    
    /**
     * @brief Get or create EmailService instance (lazy initialization)
     * @return EmailService instance or nullptr if initialization fails
     */
    search_engine::storage::EmailService* getEmailService() const;
    
    /**
     * @brief Load SMTP configuration from environment variables
     * @return SMTP configuration
     */
    search_engine::storage::EmailService::SMTPConfig loadSMTPConfig() const;
    
    /**
     * @brief Validate email address format
     * @param email Email address to validate
     * @return true if valid, false otherwise
     */
    bool isValidEmail(const std::string& email) const;
    
    /**
     * @brief Load file contents from filesystem
     * @param path Path to file to load
     * @return File contents as string, or empty string if error
     */
    std::string loadFile(const std::string& path) const;
    
    /**
     * @brief Process crawling notification request
     * @param jsonBody Parsed JSON request body
     * @param res HTTP response object
     */
    void processCrawlingNotificationRequest(const nlohmann::json& jsonBody, uWS::HttpResponse<false>* res);
    
    /**
     * @brief Process generic email request
     * @param jsonBody Parsed JSON request body
     * @param res HTTP response object
     */
    void processEmailRequest(const nlohmann::json& jsonBody, uWS::HttpResponse<false>* res);
};

// Route registration using macros (similar to .NET Core attributes)
ROUTE_CONTROLLER(EmailController) {
    using namespace routing;
    REGISTER_ROUTE(HttpMethod::POST, "/api/v2/send-crawling-notification", sendCrawlingNotification, EmailController);
    REGISTER_ROUTE(HttpMethod::POST, "/api/v2/send-email", sendEmail, EmailController);
    REGISTER_ROUTE(HttpMethod::GET, "/api/v2/email-service-status", getEmailServiceStatus, EmailController);
}
