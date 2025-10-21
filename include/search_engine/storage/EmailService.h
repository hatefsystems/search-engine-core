#pragma once

#include <string>
#include <memory>
#include <vector>
#include <chrono>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <curl/curl.h>

namespace search_engine { namespace storage {

// Forward declarations
class UnsubscribeService;
class EmailLogsStorage;
class EmailTrackingStorage;

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
        std::string senderName; // Localized sender name
        std::string unsubscribeToken; // Unsubscribe token (generate once and reuse)
        bool enableTracking = true; // Enable email tracking pixel by default
        
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
        int connectionTimeoutSeconds = 0; // 0 means auto-calculate (timeoutSeconds / 3)
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
     * @brief Send crawling completion notification asynchronously
     * @param data Notification data including recipient and crawling results
     * @param logId Email log ID for tracking (optional)
     * @return true if email queued successfully, false otherwise
     */
    bool sendCrawlingNotificationAsync(const NotificationData& data, const std::string& logId = "");
    
    /**
     * @brief Send crawling completion notification asynchronously with localized sender name
     * @param data Notification data including recipient and crawling results
     * @param senderName Localized sender name based on language
     * @param logId Email log ID for tracking (optional)
     * @return true if email queued successfully, false otherwise
     */
    bool sendCrawlingNotificationAsync(const NotificationData& data, const std::string& senderName, const std::string& logId = "");
    
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
                       const std::string& textContent = "",
                       const std::string& unsubscribeToken = "");

    /**
     * @brief Send generic HTML email asynchronously
     * @param to Recipient email address
     * @param subject Email subject
     * @param htmlContent HTML content
     * @param textContent Plain text fallback (optional)
     * @param logId Email log ID for tracking (optional)
     * @return true if email queued successfully, false otherwise
     */
    bool sendHtmlEmailAsync(const std::string& to, 
                           const std::string& subject, 
                           const std::string& htmlContent, 
                           const std::string& textContent = "",
                           const std::string& logId = "");

    /**
     * @brief Test SMTP connection
     * @return true if connection is successful, false otherwise
     */
    bool testConnection();
    
    /**
     * @brief Get last error message
     * @return Last error message
     */
    std::string getLastError() const { 
        std::lock_guard<std::mutex> lock(lastErrorMutex_);
        return lastError_; 
    }
    
    void setLastError(const std::string& error) {
        std::lock_guard<std::mutex> lock(lastErrorMutex_);
        lastError_ = error;
    }
    
    /**
     * @brief Get configured from email address
     * @return From email address
     */
    std::string getFromEmail() const { return config_.fromEmail; }

private:
    // CURL callback for reading email data
    static size_t readCallback(void* ptr, size_t size, size_t nmemb, void* userp);
    
    // Helper methods
    std::string encodeFromHeader(const std::string& name, const std::string& email);
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
    
    // Date formatting helpers
    std::string formatCompletionTime(const std::chrono::system_clock::time_point& timePoint, const std::string& language);
    std::string convertToPersianDate(const std::tm& gregorianDate);
    
    // Configuration and state
    SMTPConfig config_;
    mutable std::mutex lastErrorMutex_;
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
    
    // Email task for asynchronous processing
    struct EmailTask {
        enum Type {
            CRAWLING_NOTIFICATION,
            GENERIC_EMAIL
        };
        
        Type type;
        NotificationData notificationData;
        std::string to;
        std::string subject;
        std::string htmlContent;
        std::string textContent;
        std::string logId;
        std::chrono::system_clock::time_point queuedAt;
        
        EmailTask() = default;
        
        EmailTask(Type t, const NotificationData& data, const std::string& id = "")
            : type(t), notificationData(data), logId(id), queuedAt(std::chrono::system_clock::now()) {}
            
        EmailTask(Type t, const std::string& recipient, const std::string& subj, 
                 const std::string& html, const std::string& text = "", const std::string& id = "")
            : type(t), to(recipient), subject(subj), htmlContent(html), textContent(text), 
              logId(id), queuedAt(std::chrono::system_clock::now()) {}
    };
    
    // Asynchronous email processing
    std::queue<EmailTask> emailTaskQueue_;
    std::mutex taskQueueMutex_;
    std::condition_variable taskQueueCondition_;
    std::thread workerThread_;
    std::atomic<bool> shouldStop_;
    std::atomic<bool> asyncEnabled_;
    
    // Async processing methods
    void startAsyncWorker();
    void stopAsyncWorker();
    void processEmailTasks();
    bool processEmailTask(const EmailTask& task);
    
    // EmailLogsStorage access for async processing
    mutable std::unique_ptr<EmailLogsStorage> emailLogsStorage_;
    EmailLogsStorage* getEmailLogsStorage() const;
    
    // EmailTrackingStorage for email tracking pixel support
    mutable std::unique_ptr<EmailTrackingStorage> emailTrackingStorage_;
    EmailTrackingStorage* getEmailTrackingStorage() const;
    
    /**
     * @brief Create tracking record and embed tracking pixel in HTML
     * @param htmlContent Original HTML content
     * @param emailAddress Recipient email address
     * @param emailType Type of email (e.g., "crawling_notification")
     * @return HTML with embedded tracking pixel
     */
    std::string embedTrackingPixel(const std::string& htmlContent, 
                                   const std::string& emailAddress, 
                                   const std::string& emailType);
};

} } // namespace search_engine::storage
