#pragma once

#include <string>
#include <memory>
#include <vector>
#include <chrono>
#include <curl/curl.h>

namespace search_engine { namespace storage {

// Forward declaration
class UnsubscribeService;

/**
 * @brief Email notification service for sending crawling notifications
 * 
 * This service handles sending email notifications to users about crawling results.
 * It supports HTML email templates and handles SMTP communication.
 */
class EmailService {
public:
    /**
     * @brief Email notification data structure
     */
    struct NotificationData {
        std::string recipientEmail;
        std::string recipientName;
        std::string subject;
        std::string htmlContent;
        std::string textContent;
        std::string language = "en"; // Default to English
        
        // Crawling specific data
        int crawledPagesCount = 0;
        std::string domainName;
        std::string crawlSessionId;
        std::chrono::system_clock::time_point crawlCompletedAt;
    };

    /**
     * @brief SMTP configuration structure
     */
    struct SMTPConfig {
        std::string smtpHost = "smtp.gmail.com";
        int smtpPort = 587;
        std::string username;
        std::string password;
        std::string fromEmail;
        std::string fromName;
        bool useTLS = true;
        bool useSSL = false;
        int timeoutSeconds = 30;
    };

public:
    /**
     * @brief Constructor with SMTP configuration
     * @param config SMTP configuration
     */
    explicit EmailService(const SMTPConfig& config);
    
    /**
     * @brief Destructor
     */
    ~EmailService();

    /**
     * @brief Send crawling completion notification
     * @param data Notification data including recipient and crawling results
     * @return true if email sent successfully, false otherwise
     */
    bool sendCrawlingNotification(const NotificationData& data);
    
    /**
     * @brief Send generic HTML email
     * @param to Recipient email address
     * @param subject Email subject
     * @param htmlContent HTML content
     * @param textContent Plain text fallback (optional)
     * @return true if email sent successfully, false otherwise
     */
    bool sendHtmlEmail(const std::string& to, 
                       const std::string& subject, 
                       const std::string& htmlContent, 
                       const std::string& textContent = "");

    /**
     * @brief Test SMTP connection
     * @return true if connection is successful, false otherwise
     */
    bool testConnection();
    
    /**
     * @brief Get last error message
     * @return Last error message
     */
    std::string getLastError() const { return lastError_; }

private:
    // CURL callback for reading email data
    static size_t readCallback(void* ptr, size_t size, size_t nmemb, void* userp);
    
    // Helper methods
    std::string formatEmailHeaders(const std::string& to, const std::string& subject, const std::string& unsubscribeToken = "");
    std::string formatEmailBody(const std::string& htmlContent, const std::string& textContent);
    std::string generateBoundary();
    bool performSMTPRequest(const std::string& to, const std::string& emailData);
    
    // Template rendering methods
    std::string renderEmailTemplate(const std::string& templateName, const NotificationData& data);
    std::string loadFile(const std::string& path);
    
    // Default notification template generators (fallback)
    std::string generateDefaultNotificationHTML(const NotificationData& data);
    std::string generateDefaultNotificationText(const NotificationData& data);
    
    // Configuration and state
    SMTPConfig config_;
    std::string lastError_;
    
    // CURL handle for connection reuse
    CURL* curlHandle_;
    
    // Unsubscribe service (lazy initialized)
    mutable std::unique_ptr<UnsubscribeService> unsubscribeService_;
    
    /**
     * @brief Get or create UnsubscribeService instance (lazy initialization)
     * @return UnsubscribeService instance or nullptr if initialization fails
     */
    UnsubscribeService* getUnsubscribeService() const;
    
    // Email content buffer for CURL callback
    struct EmailBuffer {
        std::string data;
        size_t position;
    };
};

} } // namespace search_engine::storage
