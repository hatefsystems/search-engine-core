#include "../../include/search_engine/storage/EmailService.h"
#include "../../include/search_engine/storage/UnsubscribeService.h"
#include "../../include/search_engine/storage/EmailLogsStorage.h"
#include "../../include/search_engine/storage/EmailTrackingStorage.h"
#include "../../include/Logger.h"
#include <sstream>
#include <iomanip>
#include <random>
#include <regex>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <inja/inja.hpp>
#include <nlohmann/json.hpp>

namespace search_engine { namespace storage {

EmailService::EmailService(const SMTPConfig& config) 
    : config_(config), curlHandle_(nullptr), shouldStop_(false), asyncEnabled_(false) {
    
    // Initialize CURL
    curlHandle_ = curl_easy_init();
    if (!curlHandle_) {
        setLastError("Failed to initialize CURL");
        LOG_ERROR("EmailService: Failed to initialize CURL");
        return;
    }
    
    // Check if async email processing is enabled
    const char* asyncEnabled = std::getenv("EMAIL_ASYNC_ENABLED");
    LOG_DEBUG("EmailService: EMAIL_ASYNC_ENABLED env var: " + (asyncEnabled ? std::string(asyncEnabled) : "null"));
    if (asyncEnabled) {
        std::string asyncStr = std::string(asyncEnabled);
        std::transform(asyncStr.begin(), asyncStr.end(), asyncStr.begin(), ::tolower);
        asyncEnabled_ = (asyncStr == "true" || asyncStr == "1" || asyncStr == "yes");
        LOG_DEBUG("EmailService: Parsed async enabled value: " + std::to_string(asyncEnabled_));
    } else {
        asyncEnabled_ = false;
        LOG_DEBUG("EmailService: EMAIL_ASYNC_ENABLED not set, defaulting to false");
    }
    
    if (asyncEnabled_) {
        LOG_INFO("EmailService: Asynchronous email processing enabled");
        startAsyncWorker();
    } else {
        LOG_INFO("EmailService: Synchronous email processing (async disabled)");
    }
    
    LOG_INFO("EmailService initialized with SMTP host: " + config_.smtpHost + ":" + std::to_string(config_.smtpPort));
}

EmailService::~EmailService() {
    // Stop async worker if running
    if (asyncEnabled_) {
        stopAsyncWorker();
    }
    
    if (curlHandle_) {
        curl_easy_cleanup(curlHandle_);
        curlHandle_ = nullptr;
    }
}

bool EmailService::sendCrawlingNotification(const NotificationData& data) {
    LOG_INFO("Sending crawling notification to: " + data.recipientEmail + 
             " for domain: " + data.domainName + 
             " (pages: " + std::to_string(data.crawledPagesCount) + ")");
    
    try {
        // Create a mutable copy to add unsubscribe token
        NotificationData mutableData = data;
        
        // Generate unsubscribe token ONCE (if not already provided)
        if (mutableData.unsubscribeToken.empty()) {
            auto unsubscribeService = getUnsubscribeService();
            if (unsubscribeService) {
                mutableData.unsubscribeToken = unsubscribeService->createUnsubscribeToken(
                    mutableData.recipientEmail, 
                    "", // IP address - not available during email sending
                    "Email Sending System" // User agent
                );
                if (!mutableData.unsubscribeToken.empty()) {
                    LOG_DEBUG("EmailService: Generated unsubscribe token once for: " + mutableData.recipientEmail);
                } else {
                    LOG_WARNING("EmailService: Failed to generate unsubscribe token for: " + mutableData.recipientEmail);
                }
            }
        }
        
        // Generate email content
        std::string subject = mutableData.subject.empty() ? 
            "Crawling Complete - " + std::to_string(mutableData.crawledPagesCount) + " pages indexed" : 
            mutableData.subject;
            
        std::string htmlContent = mutableData.htmlContent;
        std::string textContent = mutableData.textContent;
        
        // If no custom content provided, use default template
        if (htmlContent.empty()) {
            htmlContent = generateDefaultNotificationHTML(mutableData);
        }
        
        if (textContent.empty()) {
            textContent = generateDefaultNotificationText(mutableData);
        }
        
        // Embed tracking pixel if enabled
        if (mutableData.enableTracking) {
            htmlContent = embedTrackingPixel(htmlContent, mutableData.recipientEmail, "crawling_notification");
        }
        
        // Pass the unsubscribe token to sendHtmlEmail
        return sendHtmlEmail(mutableData.recipientEmail, subject, htmlContent, textContent, mutableData.unsubscribeToken);
        
    } catch (const std::exception& e) {
        lastError_ = "Exception in sendCrawlingNotification: " + std::string(e.what());
        LOG_ERROR("EmailService: " + lastError_);
        return false;
    }
}

bool EmailService::sendHtmlEmail(const std::string& to, 
                                const std::string& subject, 
                                const std::string& htmlContent, 
                                const std::string& textContent,
                                const std::string& unsubscribeToken) {
    
    if (!curlHandle_) {
        lastError_ = "CURL not initialized";
        LOG_ERROR("EmailService: CURL not initialized");
        return false;
    }
    
    LOG_DEBUG("Preparing to send email to: " + to + " with subject: " + subject);
    
    try {
        // Use provided unsubscribe token or generate a new one if not provided
        std::string finalUnsubscribeToken = unsubscribeToken;
        if (finalUnsubscribeToken.empty()) {
            auto unsubscribeService = getUnsubscribeService();
            if (unsubscribeService) {
                finalUnsubscribeToken = unsubscribeService->createUnsubscribeToken(
                    to, 
                    "", // IP address - not available during email sending
                    "Email Sending System" // User agent
                );
                if (!finalUnsubscribeToken.empty()) {
                    LOG_DEBUG("EmailService: Generated new unsubscribe token for email headers: " + to);
                }
            }
        } else {
            LOG_DEBUG("EmailService: Reusing existing unsubscribe token for email headers: " + to);
        }
        
        // Prepare email data
        std::string emailData = formatEmailHeaders(to, subject, finalUnsubscribeToken) + 
                               formatEmailBody(htmlContent, textContent);
        
        return performSMTPRequest(to, emailData);
        
    } catch (const std::exception& e) {
        lastError_ = "Exception in sendHtmlEmail: " + std::string(e.what());
        LOG_ERROR("EmailService: " + lastError_);
        return false;
    }
}

bool EmailService::testConnection() {
    LOG_INFO("EmailService: Testing SMTP connection to: " + config_.smtpHost + ":" + std::to_string(config_.smtpPort));
    
    if (!curlHandle_) {
        lastError_ = "CURL not initialized";
        LOG_ERROR("EmailService: " + lastError_);
        return false;
    }
    
    // Reset CURL handle to ensure clean state
    curl_easy_reset(curlHandle_);
    
    // Configure CURL for connection test
    std::string smtpUrl;
    if (config_.useSSL) {
        smtpUrl = "smtps://" + config_.smtpHost + ":" + std::to_string(config_.smtpPort);
        LOG_DEBUG("EmailService: Testing SSL connection (smtps://)");
    } else {
        smtpUrl = "smtp://" + config_.smtpHost + ":" + std::to_string(config_.smtpPort);
        LOG_DEBUG("EmailService: Testing plain SMTP connection (smtp://)");
    }
    
    // Set basic connection options with error checking
    CURLcode curlRes;
    
    curlRes = curl_easy_setopt(curlHandle_, CURLOPT_URL, smtpUrl.c_str());
    if (curlRes != CURLE_OK) {
        lastError_ = "Failed to set CURLOPT_URL for connection test: " + std::string(curl_easy_strerror(curlRes));
        LOG_ERROR("EmailService: " + lastError_);
        return false;
    }
    
    curlRes = curl_easy_setopt(curlHandle_, CURLOPT_USERNAME, config_.username.c_str());
    if (curlRes != CURLE_OK) {
        lastError_ = "Failed to set CURLOPT_USERNAME for connection test: " + std::string(curl_easy_strerror(curlRes));
        LOG_ERROR("EmailService: " + lastError_);
        return false;
    }
    
    curlRes = curl_easy_setopt(curlHandle_, CURLOPT_PASSWORD, config_.password.c_str());
    if (curlRes != CURLE_OK) {
        lastError_ = "Failed to set CURLOPT_PASSWORD for connection test: " + std::string(curl_easy_strerror(curlRes));
        LOG_ERROR("EmailService: " + lastError_);
        return false;
    }
    
    curlRes = curl_easy_setopt(curlHandle_, CURLOPT_TIMEOUT, config_.timeoutSeconds);
    if (curlRes != CURLE_OK) {
        lastError_ = "Failed to set CURLOPT_TIMEOUT for connection test: " + std::string(curl_easy_strerror(curlRes));
        LOG_ERROR("EmailService: " + lastError_);
        return false;
    }
    
    // Set connection timeout to prevent hanging
    long connectionTimeout;
    if (config_.connectionTimeoutSeconds > 0) {
        connectionTimeout = config_.connectionTimeoutSeconds;
    } else {
        // Auto-calculate: at least 10 seconds, but 1/3 of total timeout
        connectionTimeout = std::max(10L, config_.timeoutSeconds / 3L);
    }
    curlRes = curl_easy_setopt(curlHandle_, CURLOPT_CONNECTTIMEOUT, connectionTimeout);
    if (curlRes != CURLE_OK) {
        lastError_ = "Failed to set CURLOPT_CONNECTTIMEOUT for connection test: " + std::string(curl_easy_strerror(curlRes));
        LOG_ERROR("EmailService: " + lastError_);
        return false;
    }
    LOG_DEBUG("EmailService: Connection timeout set to: " + std::to_string(connectionTimeout) + " seconds");
    
    // TLS/SSL configuration with error checking
    if (config_.useSSL) {
        LOG_DEBUG("EmailService: Configuring SSL connection for test");
        curlRes = curl_easy_setopt(curlHandle_, CURLOPT_USE_SSL, CURLUSESSL_ALL);
        if (curlRes != CURLE_OK) {
            lastError_ = "Failed to set CURLOPT_USE_SSL for test: " + std::string(curl_easy_strerror(curlRes));
            LOG_ERROR("EmailService: " + lastError_);
            return false;
        }
        
        curlRes = curl_easy_setopt(curlHandle_, CURLOPT_SSL_VERIFYPEER, 0L);
        if (curlRes != CURLE_OK) {
            lastError_ = "Failed to set CURLOPT_SSL_VERIFYPEER for test: " + std::string(curl_easy_strerror(curlRes));
            LOG_ERROR("EmailService: " + lastError_);
            return false;
        }
        
        curlRes = curl_easy_setopt(curlHandle_, CURLOPT_SSL_VERIFYHOST, 0L);
        if (curlRes != CURLE_OK) {
            lastError_ = "Failed to set CURLOPT_SSL_VERIFYHOST for test: " + std::string(curl_easy_strerror(curlRes));
            LOG_ERROR("EmailService: " + lastError_);
            return false;
        }
    } else if (config_.useTLS) {
        LOG_DEBUG("EmailService: Configuring STARTTLS connection for test");
        curlRes = curl_easy_setopt(curlHandle_, CURLOPT_USE_SSL, CURLUSESSL_TRY);
        if (curlRes != CURLE_OK) {
            lastError_ = "Failed to set CURLOPT_USE_SSL for STARTTLS test: " + std::string(curl_easy_strerror(curlRes));
            LOG_ERROR("EmailService: " + lastError_);
            return false;
        }
        
        curlRes = curl_easy_setopt(curlHandle_, CURLOPT_SSL_VERIFYPEER, 0L);
        if (curlRes != CURLE_OK) {
            lastError_ = "Failed to set CURLOPT_SSL_VERIFYPEER for STARTTLS test: " + std::string(curl_easy_strerror(curlRes));
            LOG_ERROR("EmailService: " + lastError_);
            return false;
        }
        
        curlRes = curl_easy_setopt(curlHandle_, CURLOPT_SSL_VERIFYHOST, 0L);
        if (curlRes != CURLE_OK) {
            lastError_ = "Failed to set CURLOPT_SSL_VERIFYHOST for STARTTLS test: " + std::string(curl_easy_strerror(curlRes));
            LOG_ERROR("EmailService: " + lastError_);
            return false;
        }
        
        // Additional options for STARTTLS
        curlRes = curl_easy_setopt(curlHandle_, CURLOPT_TCP_KEEPALIVE, 1L);
        if (curlRes != CURLE_OK) {
            LOG_WARNING("EmailService: Failed to set CURLOPT_TCP_KEEPALIVE for STARTTLS test: " + std::string(curl_easy_strerror(curlRes)));
        }
        
        curlRes = curl_easy_setopt(curlHandle_, CURLOPT_TCP_KEEPIDLE, 10L);
        if (curlRes != CURLE_OK) {
            LOG_WARNING("EmailService: Failed to set CURLOPT_TCP_KEEPIDLE for STARTTLS test: " + std::string(curl_easy_strerror(curlRes)));
        }
        
        curlRes = curl_easy_setopt(curlHandle_, CURLOPT_TCP_KEEPINTVL, 10L);
        if (curlRes != CURLE_OK) {
            LOG_WARNING("EmailService: Failed to set CURLOPT_TCP_KEEPINTVL for STARTTLS test: " + std::string(curl_easy_strerror(curlRes)));
        }
    }
    
    LOG_DEBUG("EmailService: All CURL options set for connection test, attempting connection...");
    
    // Perform connection test with proper error handling
    CURLcode res;
    try {
        res = curl_easy_perform(curlHandle_);
        LOG_DEBUG("EmailService: Connection test completed with code: " + std::to_string(res));
    } catch (const std::exception& e) {
        lastError_ = "Exception during connection test: " + std::string(e.what());
        LOG_ERROR("EmailService: " + lastError_);
        return false;
    }
    
    if (res != CURLE_OK) {
        std::string errorMsg = curl_easy_strerror(res);
        lastError_ = "SMTP connection test failed: " + errorMsg;
        LOG_ERROR("EmailService: " + lastError_);
        
        // Log additional debugging information
        if (res == CURLE_COULDNT_CONNECT) {
            LOG_ERROR("EmailService: Connection test failed - check if SMTP server is running and accessible");
            LOG_ERROR("EmailService: SMTP URL: " + smtpUrl);
        } else if (res == CURLE_OPERATION_TIMEDOUT) {
            LOG_ERROR("EmailService: Connection test timed out - check network connectivity and firewall settings");
        } else if (res == CURLE_LOGIN_DENIED) {
            LOG_ERROR("EmailService: Authentication failed during connection test - check username and password");
        }
        
        return false;
    }
    
    LOG_INFO("EmailService: SMTP connection test successful");
    return true;
}

// Static callback for CURL
size_t EmailService::readCallback(void* ptr, size_t size, size_t nmemb, void* userp) {
    EmailBuffer* buffer = static_cast<EmailBuffer*>(userp);
    
    size_t available = buffer->data.size() - buffer->position;
    size_t requested = size * nmemb;
    size_t toWrite = std::min(available, requested);
    
    if (toWrite > 0) {
        std::memcpy(ptr, buffer->data.data() + buffer->position, toWrite);
        buffer->position += toWrite;
    }
    
    return toWrite;
}

std::string EmailService::encodeFromHeader(const std::string& name, const std::string& email) {
    // RFC 5322 and RFC 2047 compliant From header encoding
    
    // Check if name contains only ASCII printable characters (excluding special chars that need quoting)
    bool needsEncoding = false;
    bool needsQuoting = false;
    
    for (unsigned char c : name) {
        if (c > 127) {
            // Non-ASCII character - needs RFC 2047 encoding
            needsEncoding = true;
            break;
        }
        // Check for special characters that require quoting per RFC 5322
        if (c == '"' || c == '\\' || c == '(' || c == ')' || c == '<' || c == '>' || 
            c == '[' || c == ']' || c == ':' || c == ';' || c == '@' || c == ',' || c == '.') {
            needsQuoting = true;
        }
    }
    
    if (needsEncoding) {
        // RFC 2047: Encode as =?UTF-8?B?base64?=
        std::string encoded = "=?UTF-8?B?";
        
        // Base64 encode the name
        static const char* base64_chars = 
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789+/";
        
        std::string base64;
        int val = 0;
        int valb = -6;
        
        for (unsigned char c : name) {
            val = (val << 8) + c;
            valb += 8;
            while (valb >= 0) {
                base64.push_back(base64_chars[(val >> valb) & 0x3F]);
                valb -= 6;
            }
        }
        if (valb > -6) {
            base64.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
        }
        while (base64.size() % 4) {
            base64.push_back('=');
        }
        
        encoded += base64 + "?= <" + email + ">";
        return encoded;
        
    } else if (needsQuoting || name.find(' ') != std::string::npos) {
        // Quote the name if it contains spaces or special characters
        std::string quoted = "\"";
        for (char c : name) {
            if (c == '"' || c == '\\') {
                quoted += '\\'; // Escape quotes and backslashes
            }
            quoted += c;
        }
        quoted += "\" <" + email + ">";
        return quoted;
        
    } else if (name.empty()) {
        // No display name, just email
        return email;
        
    } else {
        // Simple ASCII name without special chars
        return name + " <" + email + ">";
    }
}

std::string EmailService::formatEmailHeaders(const std::string& to, const std::string& subject, const std::string& unsubscribeToken) {
    std::ostringstream headers;

    // Generate unique Message-ID (RFC 5322 requirement)
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    // Generate random component for uniqueness
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100000, 999999);
    std::string randomPart = std::to_string(dis(gen));

    // Extract domain from sender email for Message-ID
    std::string domain = "notify.hatef.ir"; // Default fallback
    size_t atPos = config_.fromEmail.find('@');
    if (atPos != std::string::npos) {
        domain = config_.fromEmail.substr(atPos + 1);
    }

    std::string messageId = "<" + std::to_string(timestamp) + "." + randomPart + "@" + domain + ">";

    // RFC 5322 compliant Date header
    auto timeT = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::gmtime(&timeT);
    char dateBuffer[100];
    std::strftime(dateBuffer, sizeof(dateBuffer), "%a, %d %b %Y %H:%M:%S +0000", &tm);

    headers << "Message-ID: " << messageId << "\r\n";
    headers << "Date: " << dateBuffer << "\r\n";
    headers << "Return-Path: " << config_.fromEmail << "\r\n";
    headers << "To: " << to << "\r\n";

    // RFC 5322 compliant From header with proper encoding
    headers << "From: " << encodeFromHeader(config_.fromName, config_.fromEmail) << "\r\n";

    headers << "Reply-To: info@hatef.ir\r\n";
    headers << "Subject: " << subject << "\r\n";
    headers << "MIME-Version: 1.0\r\n";

    // Add List-Unsubscribe headers if unsubscribe token is provided
    if (!unsubscribeToken.empty()) {
        // RFC 8058 compliant List-Unsubscribe header
        headers << "List-Unsubscribe: <https://notify.hatef.ir/u/" << unsubscribeToken << ">, <mailto:unsubscribe+" << unsubscribeToken << "@notify.hatef.ir>\r\n";

        // RFC 8058 List-Unsubscribe-Post header for one-click unsubscribe
        headers << "List-Unsubscribe-Post: List-Unsubscribe=One-Click\r\n";

        LOG_DEBUG("EmailService: Added List-Unsubscribe headers with token: " + unsubscribeToken.substr(0, 8) + "...");
    }

    LOG_DEBUG("EmailService: Generated Message-ID: " + messageId);

    return headers.str();
}

std::string EmailService::formatEmailBody(const std::string& htmlContent, const std::string& textContent) {
    std::string boundary = generateBoundary();
    std::ostringstream body;
    
    // Content-Type header for multipart
    body << "Content-Type: multipart/alternative; boundary=\"" << boundary << "\"\r\n\r\n";
    
    // Text part (if provided)
    if (!textContent.empty()) {
        body << "--" << boundary << "\r\n";
        body << "Content-Type: text/plain; charset=\"UTF-8\"\r\n";
        body << "Content-Transfer-Encoding: 8bit\r\n\r\n";
        body << textContent << "\r\n\r\n";
    }
    
    // HTML part
    body << "--" << boundary << "\r\n";
    body << "Content-Type: text/html; charset=\"UTF-8\"\r\n";
    body << "Content-Transfer-Encoding: 8bit\r\n\r\n";
    body << htmlContent << "\r\n\r\n";
    
    // End boundary
    body << "--" << boundary << "--\r\n";
    
    return body.str();
}

std::string EmailService::generateBoundary() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    
    std::ostringstream boundary;
    boundary << "boundary_";
    
    for (int i = 0; i < 16; ++i) {
        boundary << std::hex << dis(gen);
    }
    
    return boundary.str();
}

bool EmailService::performSMTPRequest(const std::string& to, const std::string& emailData) {
    LOG_DEBUG("EmailService: Starting SMTP request to: " + to);
    LOG_DEBUG("EmailService: SMTP host: " + config_.smtpHost + ":" + std::to_string(config_.smtpPort));
    
    // Reset CURL handle to ensure clean state
    curl_easy_reset(curlHandle_);
    
    // Prepare SMTP URL
    std::string smtpUrl;
    if (config_.useSSL) {
        smtpUrl = "smtps://" + config_.smtpHost + ":" + std::to_string(config_.smtpPort);
        LOG_DEBUG("EmailService: Using SSL connection (smtps://)");
    } else {
        smtpUrl = "smtp://" + config_.smtpHost + ":" + std::to_string(config_.smtpPort);
        LOG_DEBUG("EmailService: Using plain SMTP connection (smtp://)");
    }
    
    // Prepare recipients list
    struct curl_slist* recipients = nullptr;
    try {
        recipients = curl_slist_append(recipients, to.c_str());
        if (!recipients) {
            lastError_ = "Failed to create recipient list";
            LOG_ERROR("EmailService: " + lastError_);
            return false;
        }
        LOG_DEBUG("EmailService: Recipient list created successfully");
    } catch (const std::exception& e) {
        lastError_ = "Exception creating recipient list: " + std::string(e.what());
        LOG_ERROR("EmailService: " + lastError_);
        return false;
    }
    
    // Prepare email buffer with proper initialization
    EmailBuffer buffer;
    try {
        buffer.data = emailData;
        buffer.position = 0;
        LOG_DEBUG("EmailService: Email buffer prepared, size: " + std::to_string(buffer.data.size()) + " bytes");
    } catch (const std::exception& e) {
        curl_slist_free_all(recipients);
        lastError_ = "Exception preparing email buffer: " + std::string(e.what());
        LOG_ERROR("EmailService: " + lastError_);
        return false;
    }
    
    // Configure CURL options with error checking
    CURLcode curlRes;
    
    curlRes = curl_easy_setopt(curlHandle_, CURLOPT_URL, smtpUrl.c_str());
    if (curlRes != CURLE_OK) {
        curl_slist_free_all(recipients);
        lastError_ = "Failed to set CURLOPT_URL: " + std::string(curl_easy_strerror(curlRes));
        LOG_ERROR("EmailService: " + lastError_);
        return false;
    }
    
    curlRes = curl_easy_setopt(curlHandle_, CURLOPT_USERNAME, config_.username.c_str());
    if (curlRes != CURLE_OK) {
        curl_slist_free_all(recipients);
        lastError_ = "Failed to set CURLOPT_USERNAME: " + std::string(curl_easy_strerror(curlRes));
        LOG_ERROR("EmailService: " + lastError_);
        return false;
    }
    
    curlRes = curl_easy_setopt(curlHandle_, CURLOPT_PASSWORD, config_.password.c_str());
    if (curlRes != CURLE_OK) {
        curl_slist_free_all(recipients);
        lastError_ = "Failed to set CURLOPT_PASSWORD: " + std::string(curl_easy_strerror(curlRes));
        LOG_ERROR("EmailService: " + lastError_);
        return false;
    }
    
    curlRes = curl_easy_setopt(curlHandle_, CURLOPT_MAIL_FROM, config_.fromEmail.c_str());
    if (curlRes != CURLE_OK) {
        curl_slist_free_all(recipients);
        lastError_ = "Failed to set CURLOPT_MAIL_FROM: " + std::string(curl_easy_strerror(curlRes));
        LOG_ERROR("EmailService: " + lastError_);
        return false;
    }
    
    curlRes = curl_easy_setopt(curlHandle_, CURLOPT_MAIL_RCPT, recipients);
    if (curlRes != CURLE_OK) {
        curl_slist_free_all(recipients);
        lastError_ = "Failed to set CURLOPT_MAIL_RCPT: " + std::string(curl_easy_strerror(curlRes));
        LOG_ERROR("EmailService: " + lastError_);
        return false;
    }
    
    curlRes = curl_easy_setopt(curlHandle_, CURLOPT_READFUNCTION, readCallback);
    if (curlRes != CURLE_OK) {
        curl_slist_free_all(recipients);
        lastError_ = "Failed to set CURLOPT_READFUNCTION: " + std::string(curl_easy_strerror(curlRes));
        LOG_ERROR("EmailService: " + lastError_);
        return false;
    }
    
    curlRes = curl_easy_setopt(curlHandle_, CURLOPT_READDATA, &buffer);
    if (curlRes != CURLE_OK) {
        curl_slist_free_all(recipients);
        lastError_ = "Failed to set CURLOPT_READDATA: " + std::string(curl_easy_strerror(curlRes));
        LOG_ERROR("EmailService: " + lastError_);
        return false;
    }
    
    curlRes = curl_easy_setopt(curlHandle_, CURLOPT_UPLOAD, 1L);
    if (curlRes != CURLE_OK) {
        curl_slist_free_all(recipients);
        lastError_ = "Failed to set CURLOPT_UPLOAD: " + std::string(curl_easy_strerror(curlRes));
        LOG_ERROR("EmailService: " + lastError_);
        return false;
    }
    
    curlRes = curl_easy_setopt(curlHandle_, CURLOPT_TIMEOUT, config_.timeoutSeconds);
    if (curlRes != CURLE_OK) {
        curl_slist_free_all(recipients);
        lastError_ = "Failed to set CURLOPT_TIMEOUT: " + std::string(curl_easy_strerror(curlRes));
        LOG_ERROR("EmailService: " + lastError_);
        return false;
    }
    
    // TLS/SSL configuration with error checking
    if (config_.useSSL) {
        LOG_DEBUG("EmailService: Configuring SSL connection");
        curlRes = curl_easy_setopt(curlHandle_, CURLOPT_USE_SSL, CURLUSESSL_ALL);
        if (curlRes != CURLE_OK) {
            curl_slist_free_all(recipients);
            lastError_ = "Failed to set CURLOPT_USE_SSL: " + std::string(curl_easy_strerror(curlRes));
            LOG_ERROR("EmailService: " + lastError_);
            return false;
        }
        
        curlRes = curl_easy_setopt(curlHandle_, CURLOPT_SSL_VERIFYPEER, 0L);
        if (curlRes != CURLE_OK) {
            curl_slist_free_all(recipients);
            lastError_ = "Failed to set CURLOPT_SSL_VERIFYPEER: " + std::string(curl_easy_strerror(curlRes));
            LOG_ERROR("EmailService: " + lastError_);
            return false;
        }
        
        curlRes = curl_easy_setopt(curlHandle_, CURLOPT_SSL_VERIFYHOST, 0L);
        if (curlRes != CURLE_OK) {
            curl_slist_free_all(recipients);
            lastError_ = "Failed to set CURLOPT_SSL_VERIFYHOST: " + std::string(curl_easy_strerror(curlRes));
            LOG_ERROR("EmailService: " + lastError_);
            return false;
        }
    } else if (config_.useTLS) {
        LOG_DEBUG("EmailService: Configuring STARTTLS connection");
        curlRes = curl_easy_setopt(curlHandle_, CURLOPT_USE_SSL, CURLUSESSL_TRY);
        if (curlRes != CURLE_OK) {
            curl_slist_free_all(recipients);
            lastError_ = "Failed to set CURLOPT_USE_SSL for STARTTLS: " + std::string(curl_easy_strerror(curlRes));
            LOG_ERROR("EmailService: " + lastError_);
            return false;
        }
        
        curlRes = curl_easy_setopt(curlHandle_, CURLOPT_SSL_VERIFYPEER, 0L);
        if (curlRes != CURLE_OK) {
            curl_slist_free_all(recipients);
            lastError_ = "Failed to set CURLOPT_SSL_VERIFYPEER for STARTTLS: " + std::string(curl_easy_strerror(curlRes));
            LOG_ERROR("EmailService: " + lastError_);
            return false;
        }
        
        curlRes = curl_easy_setopt(curlHandle_, CURLOPT_SSL_VERIFYHOST, 0L);
        if (curlRes != CURLE_OK) {
            curl_slist_free_all(recipients);
            lastError_ = "Failed to set CURLOPT_SSL_VERIFYHOST for STARTTLS: " + std::string(curl_easy_strerror(curlRes));
            LOG_ERROR("EmailService: " + lastError_);
            return false;
        }
    }
    
    // Add connection timeout to prevent hanging
    long connectionTimeout;
    if (config_.connectionTimeoutSeconds > 0) {
        connectionTimeout = config_.connectionTimeoutSeconds;
    } else {
        // Auto-calculate: at least 10 seconds, but 1/3 of total timeout
        connectionTimeout = std::max(10L, config_.timeoutSeconds / 3L);
    }
    curlRes = curl_easy_setopt(curlHandle_, CURLOPT_CONNECTTIMEOUT, connectionTimeout);
    if (curlRes != CURLE_OK) {
        curl_slist_free_all(recipients);
        lastError_ = "Failed to set CURLOPT_CONNECTTIMEOUT: " + std::string(curl_easy_strerror(curlRes));
        LOG_ERROR("EmailService: " + lastError_);
        return false;
    }
    LOG_DEBUG("EmailService: Connection timeout set to: " + std::to_string(connectionTimeout) + " seconds");
    
    LOG_DEBUG("EmailService: All CURL options set successfully, attempting connection...");
    
    // Perform the request with proper error handling
    CURLcode res;
    try {
        res = curl_easy_perform(curlHandle_);
        LOG_DEBUG("EmailService: CURL operation completed with code: " + std::to_string(res));
    } catch (const std::exception& e) {
        curl_slist_free_all(recipients);
        lastError_ = "Exception during CURL operation: " + std::string(e.what());
        LOG_ERROR("EmailService: " + lastError_);
        return false;
    }
    
    // Clean up recipients list immediately after use
    curl_slist_free_all(recipients);
    recipients = nullptr;
    
    if (res != CURLE_OK) {
        std::string errorMsg = curl_easy_strerror(res);
        lastError_ = "SMTP request failed: " + errorMsg;
        LOG_ERROR("EmailService: " + lastError_);
        
        // Log additional debugging information
        if (res == CURLE_COULDNT_CONNECT) {
            LOG_ERROR("EmailService: Connection failed - check if SMTP server is running and accessible");
            LOG_ERROR("EmailService: SMTP URL: " + smtpUrl);
        } else if (res == CURLE_OPERATION_TIMEDOUT) {
            LOG_ERROR("EmailService: Connection timed out - check network connectivity and firewall settings");
        } else if (res == CURLE_LOGIN_DENIED) {
            LOG_ERROR("EmailService: Authentication failed - check username and password");
        }
        
        return false;
    }
    
    // Get response code with error checking
    long responseCode = 0;
    CURLcode infoRes = curl_easy_getinfo(curlHandle_, CURLINFO_RESPONSE_CODE, &responseCode);
    if (infoRes == CURLE_OK) {
        LOG_DEBUG("EmailService: SMTP response code: " + std::to_string(responseCode));
    } else {
        LOG_WARNING("EmailService: Could not get SMTP response code: " + std::string(curl_easy_strerror(infoRes)));
        responseCode = 0; // Default to failure if we can't get the code
    }
    
    if (responseCode >= 200 && responseCode < 300) {
        LOG_INFO("EmailService: Email sent successfully to: " + to + " (response code: " + std::to_string(responseCode) + ")");
        return true;
    } else {
        lastError_ = "SMTP server returned error code: " + std::to_string(responseCode);
        LOG_ERROR("EmailService: " + lastError_);
        return false;
    }
}

std::string EmailService::generateDefaultNotificationHTML(const NotificationData& data) {
    LOG_INFO("EmailService: Using Inja template-based email generation for language: " + data.language);
    
    // Render the email template
    std::string templateHTML = renderEmailTemplate("email-crawling-notification.inja", data);
    
    if (templateHTML.empty()) {
        LOG_ERROR("EmailService: Template rendering failed and no fallback available");
        throw std::runtime_error("Failed to render email template");
    }
    
    LOG_DEBUG("EmailService: Generated HTML content length: " + std::to_string(templateHTML.length()) + " bytes for language: " + data.language);
    LOG_DEBUG("EmailService: HTML preview (first 200 chars): " + templateHTML.substr(0, std::min(size_t(200), templateHTML.length())));
    
    return templateHTML;
}

std::string EmailService::generateDefaultNotificationText(const NotificationData& data) {
    std::ostringstream text;
    
    // Format crawl completion time
    auto time_t = std::chrono::system_clock::to_time_t(data.crawlCompletedAt);
    std::ostringstream timeStr;
    timeStr << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S UTC");
    
    text << "CRAWLING COMPLETE\n";
    text << "===================\n\n";
    text << "Hello " << data.recipientName << ",\n\n";
    text << "We're excited to let you know that we've successfully crawled and indexed your website!\n\n";
    text << "CRAWLING RESULTS:\n";
    text << "- Domain: " << data.domainName << "\n";
    text << "- Pages Indexed: " << data.crawledPagesCount << "\n";
    text << "- Completed At: " << timeStr.str() << "\n";
    text << "- Session ID: " << data.crawlSessionId << "\n\n";
    text << "Your pages are now searchable in our search engine. If you'd like to crawl and index more pages from your site, please visit: https://hatef.ir/crawl-request\n\n";
    text << "Thank you for using our search engine service!\n\n";
    text << "---\n";
    text << "This is an automated notification from Hatef.ir Search Engine\n";
    text << "© 2024 Hatef.ir - All rights reserved\n";
    
    return text.str();
}

std::string EmailService::loadFile(const std::string& path) {
    LOG_DEBUG("EmailService: Attempting to load file: " + path);
    
    if (!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path)) {
        LOG_ERROR("EmailService: File does not exist or is not a regular file: " + path);
        return "";
    }
    
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR("EmailService: Could not open file: " + path);
        return "";
    }
    
    file.seekg(0, std::ios::end);
    std::streamsize length = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::string content(length, '\0');
    if (!file.read(content.data(), length)) {
        LOG_ERROR("EmailService: Failed to read file: " + path);
        return "";
    }
    
    if (content.empty()) {
        LOG_WARNING("EmailService: File is empty: " + path);
    } else {
        LOG_DEBUG("EmailService: Successfully loaded file: " + path + " (size: " + std::to_string(content.length()) + " bytes)");
    }
    
    return content;
}

std::string EmailService::renderEmailTemplate(const std::string& templateName, const NotificationData& data) {
    try {
        LOG_DEBUG("EmailService: Rendering email template: " + templateName);
        
        // Load localization data
        std::string localesPath = "locales/" + data.language + "/crawling-notification.json";
        std::string localeContent = loadFile(localesPath);
        
        if (localeContent.empty() && data.language != "en") {
            LOG_WARNING("EmailService: Failed to load locale file: " + localesPath + ", falling back to English");
            localesPath = "locales/en/crawling-notification.json";
            localeContent = loadFile(localesPath);
        }
        
        if (localeContent.empty()) {
            LOG_ERROR("EmailService: Failed to load any localization file");
            throw std::runtime_error("Failed to load localization file for language: " + data.language);
        }
        
        // Parse localization data
        nlohmann::json localeData = nlohmann::json::parse(localeContent);
        
        // Prepare template data - copy the entire locale structure
        nlohmann::json templateData = localeData;
        templateData["recipientName"] = data.recipientName;
        templateData["domainName"] = data.domainName;
        templateData["crawledPagesCount"] = data.crawledPagesCount;
        templateData["crawlSessionId"] = data.crawlSessionId;
        
        // Format completion time based on language
        templateData["completionTime"] = formatCompletionTime(data.crawlCompletedAt, data.language);
        
        // Extract sender name from locale data
        if (localeData.contains("email") && localeData["email"].contains("sender_name")) {
            templateData["senderName"] = localeData["email"]["sender_name"];
            LOG_DEBUG("EmailService: Using localized sender name: " + std::string(localeData["email"]["sender_name"]));
        } else {
            // Fallback to default sender name
            templateData["senderName"] = "Hatef Search Engine";
            LOG_WARNING("EmailService: sender_name not found in locale file, using default");
        }
        
        // Use existing unsubscribe token from data or generate a new one
        if (!data.unsubscribeToken.empty()) {
            templateData["unsubscribeToken"] = data.unsubscribeToken;
            LOG_DEBUG("EmailService: Using pre-generated unsubscribe token for: " + data.recipientEmail);
        } else {
            // Fallback: generate token if not provided (shouldn't happen in normal flow)
            auto unsubscribeService = getUnsubscribeService();
            if (unsubscribeService) {
                std::string unsubscribeToken = unsubscribeService->createUnsubscribeToken(
                    data.recipientEmail, 
                    "", // IP address - not available during email generation
                    "Email Template System" // User agent
                );
                if (!unsubscribeToken.empty()) {
                    templateData["unsubscribeToken"] = unsubscribeToken;
                    LOG_DEBUG("EmailService: Generated unsubscribe token for: " + data.recipientEmail);
                } else {
                    LOG_WARNING("EmailService: Failed to generate unsubscribe token for: " + data.recipientEmail);
                }
            } else {
                LOG_WARNING("EmailService: UnsubscribeService unavailable, skipping token generation");
            }
        }
        
        // Initialize Inja environment
        inja::Environment env("templates/");
        
        // Register template functions (same as HomeController)
        env.add_callback("formatThousands", 1, [](inja::Arguments& args) {
            try {
                if (args.empty()) return std::string("0");
                
                long long value = 0;
                if (args[0]->is_number_integer()) {
                    value = args[0]->get<long long>();
                } else if (args[0]->is_number()) {
                    value = static_cast<long long>(args[0]->get<double>());
                }
                
                std::string result = std::to_string(value);
                int insertPosition = result.length() - 3;
                while (insertPosition > 0) {
                    result.insert(insertPosition, ",");
                    insertPosition -= 3;
                }
                return result;
            } catch (...) {
                return std::string("0");
            }
        });
        
        env.add_callback("formatDateTime", 1, [](inja::Arguments& args) {
            try {
                if (args.empty()) return std::string("1970-01-01 00:00:00");
                
                long long timestamp = 0;
                if (args[0]->is_number_integer()) {
                    timestamp = args[0]->get<long long>();
                } else if (args[0]->is_number()) {
                    timestamp = static_cast<long long>(args[0]->get<double>());
                }
                
                std::time_t time = static_cast<std::time_t>(timestamp);
                std::tm* tm = std::localtime(&time);
                char buffer[64];
                std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm);
                return std::string(buffer);
            } catch (...) {
                return std::string("1970-01-01 00:00:00");
            }
        });
        
        // Render template
        std::string result = env.render_file(templateName, templateData);
        LOG_INFO("EmailService: Successfully rendered email template: " + templateName);
        return result;
        
    } catch (const std::exception& e) {
        LOG_ERROR("EmailService: Template rendering error: " + std::string(e.what()));
        throw std::runtime_error("Failed to render email template: " + std::string(e.what()));
    }
}

UnsubscribeService* EmailService::getUnsubscribeService() const {
    if (!unsubscribeService_) {
        try {
            LOG_INFO("EmailService: Lazy initializing UnsubscribeService");
            unsubscribeService_ = std::make_unique<UnsubscribeService>();
        } catch (const std::exception& e) {
            LOG_ERROR("EmailService: Failed to lazy initialize UnsubscribeService: " + std::string(e.what()));
            return nullptr;
        }
    }
    return unsubscribeService_.get();
}

EmailLogsStorage* EmailService::getEmailLogsStorage() const {
    if (!emailLogsStorage_) {
        try {
            LOG_INFO("EmailService: Lazy initializing EmailLogsStorage for async processing");
            emailLogsStorage_ = std::make_unique<EmailLogsStorage>();
            LOG_INFO("EmailService: EmailLogsStorage lazy initialization completed successfully");
        } catch (const std::exception& e) {
            LOG_ERROR("EmailService: Failed to lazy initialize EmailLogsStorage: " + std::string(e.what()));
            return nullptr;
        }
    }
    return emailLogsStorage_.get();
}

EmailTrackingStorage* EmailService::getEmailTrackingStorage() const {
    if (!emailTrackingStorage_) {
        try {
            LOG_INFO("EmailService: Lazy initializing EmailTrackingStorage for email tracking");
            emailTrackingStorage_ = std::make_unique<EmailTrackingStorage>();
            LOG_INFO("EmailService: EmailTrackingStorage lazy initialization completed successfully");
        } catch (const std::exception& e) {
            LOG_ERROR("EmailService: Failed to lazy initialize EmailTrackingStorage: " + std::string(e.what()));
            return nullptr;
        }
    }
    return emailTrackingStorage_.get();
}

std::string EmailService::embedTrackingPixel(const std::string& htmlContent, 
                                            const std::string& emailAddress, 
                                            const std::string& emailType) {
    try {
        LOG_DEBUG("EmailService: Embedding tracking pixel for email: " + emailAddress + ", type: " + emailType);
        
        // Get tracking storage
        auto trackingStorage = getEmailTrackingStorage();
        if (!trackingStorage) {
            LOG_WARNING("EmailService: EmailTrackingStorage unavailable, skipping tracking pixel");
            return htmlContent;
        }
        
        // Create tracking record
        auto result = trackingStorage->createTrackingRecord(emailAddress, emailType);
        if (!result.success) {
            LOG_WARNING("EmailService: Failed to create tracking record: " + result.message);
            return htmlContent;
        }
        
        std::string trackingId = result.value;
        LOG_DEBUG("EmailService: Created tracking record with ID: " + trackingId);
        
        // Get base URL from environment or use default
        const char* baseUrl = std::getenv("BASE_URL");
        std::string trackingUrl = baseUrl ? std::string(baseUrl) : "https://hatef.ir";
        trackingUrl += "/track/" + trackingId + ".png";
        
        // Create tracking pixel HTML
        std::string trackingPixel = "<img src=\"" + trackingUrl + "\" width=\"1\" height=\"1\" alt=\"\" style=\"display:block; border:0; margin:0; padding:0;\" />";
        
        // Insert tracking pixel before closing </body> tag
        std::string modifiedHtml = htmlContent;
        size_t bodyEndPos = modifiedHtml.rfind("</body>");
        
        if (bodyEndPos != std::string::npos) {
            modifiedHtml.insert(bodyEndPos, trackingPixel);
            LOG_DEBUG("EmailService: Tracking pixel embedded successfully");
        } else {
            // If no </body> tag, append to end
            modifiedHtml += trackingPixel;
            LOG_WARNING("EmailService: No </body> tag found, appending tracking pixel to end");
        }
        
        return modifiedHtml;
        
    } catch (const std::exception& e) {
        LOG_ERROR("EmailService: Exception in embedTrackingPixel: " + std::string(e.what()));
        return htmlContent; // Return original content on error
    }
}

// Asynchronous email sending methods

bool EmailService::sendCrawlingNotificationAsync(const NotificationData& data, const std::string& logId) {
    if (!asyncEnabled_) {
        LOG_WARNING("EmailService: Async email processing is disabled, falling back to synchronous sending");
        return sendCrawlingNotification(data);
    }
    
    LOG_INFO("EmailService: Queuing crawling notification for async processing to: " + data.recipientEmail);
    
    try {
        std::lock_guard<std::mutex> lock(taskQueueMutex_);
        emailTaskQueue_.emplace(EmailTask::CRAWLING_NOTIFICATION, data, logId);
        taskQueueCondition_.notify_one();
        
        LOG_DEBUG("EmailService: Crawling notification queued successfully");
        return true;
    } catch (const std::exception& e) {
        lastError_ = "Failed to queue crawling notification: " + std::string(e.what());
        LOG_ERROR("EmailService: " + lastError_);
        return false;
    }
}

bool EmailService::sendCrawlingNotificationAsync(const NotificationData& data, const std::string& senderName, const std::string& logId) {
    if (!asyncEnabled_) {
        LOG_WARNING("EmailService: Async email processing is disabled, falling back to synchronous sending");
        // For synchronous sending, we need to temporarily update the sender name
        std::string originalFromName = config_.fromName;
        config_.fromName = senderName;
        bool result = sendCrawlingNotification(data);
        config_.fromName = originalFromName; // Restore original name
        return result;
    }
    
    LOG_INFO("EmailService: Queuing crawling notification with localized sender name '" + senderName + 
             "' for async processing to: " + data.recipientEmail);
    
    try {
        std::lock_guard<std::mutex> lock(taskQueueMutex_);
        // Create a copy of data with sender name
        NotificationData dataWithSender = data;
        dataWithSender.senderName = senderName; // Add sender name to data
        emailTaskQueue_.emplace(EmailTask::CRAWLING_NOTIFICATION, dataWithSender, logId);
        taskQueueCondition_.notify_one();
        
        LOG_DEBUG("EmailService: Crawling notification with localized sender name queued successfully");
        return true;
    } catch (const std::exception& e) {
        lastError_ = "Failed to queue crawling notification with sender name: " + std::string(e.what());
        LOG_ERROR("EmailService: " + lastError_);
        return false;
    }
}

bool EmailService::sendHtmlEmailAsync(const std::string& to, 
                                     const std::string& subject, 
                                     const std::string& htmlContent, 
                                     const std::string& textContent,
                                     const std::string& logId) {
    if (!asyncEnabled_) {
        LOG_WARNING("EmailService: Async email processing is disabled, falling back to synchronous sending");
        return sendHtmlEmail(to, subject, htmlContent, textContent);
    }
    
    LOG_INFO("EmailService: Queuing generic email for async processing to: " + to);
    
    try {
        std::lock_guard<std::mutex> lock(taskQueueMutex_);
        emailTaskQueue_.emplace(EmailTask::GENERIC_EMAIL, to, subject, htmlContent, textContent, logId);
        taskQueueCondition_.notify_one();
        
        LOG_DEBUG("EmailService: Generic email queued successfully");
        return true;
    } catch (const std::exception& e) {
        lastError_ = "Failed to queue generic email: " + std::string(e.what());
        LOG_ERROR("EmailService: " + lastError_);
        return false;
    }
}

void EmailService::startAsyncWorker() {
    shouldStop_ = false;
    workerThread_ = std::thread(&EmailService::processEmailTasks, this);
    LOG_INFO("EmailService: Async worker thread started");
}

void EmailService::stopAsyncWorker() {
    if (workerThread_.joinable()) {
        shouldStop_ = true;
        taskQueueCondition_.notify_all();
        workerThread_.join();
        LOG_INFO("EmailService: Async worker thread stopped");
    }
}

void EmailService::processEmailTasks() {
    LOG_INFO("EmailService: Async email worker thread started");
    
    while (!shouldStop_) {
        std::unique_lock<std::mutex> lock(taskQueueMutex_);
        
        // Wait for tasks or stop signal
        taskQueueCondition_.wait(lock, [this] { return !emailTaskQueue_.empty() || shouldStop_; });
        
        // Process all available tasks
        while (!emailTaskQueue_.empty() && !shouldStop_) {
            EmailTask task = std::move(emailTaskQueue_.front());
            emailTaskQueue_.pop();
            lock.unlock();
            
            // Process the task
            bool success = processEmailTask(task);
            
            if (success) {
                LOG_DEBUG("EmailService: Async email task processed successfully");
            } else {
                LOG_ERROR("EmailService: Async email task failed: " + lastError_);
            }
            
            lock.lock();
        }
    }
    
    LOG_INFO("EmailService: Async email worker thread exiting");
}

bool EmailService::processEmailTask(const EmailTask& task) {
    try {
        bool success = false;
        
        switch (task.type) {
            case EmailTask::CRAWLING_NOTIFICATION:
                LOG_DEBUG("EmailService: Processing async crawling notification for: " + task.notificationData.recipientEmail);
                // Use localized sender name if provided
                if (!task.notificationData.senderName.empty()) {
                    std::string originalFromName = config_.fromName;
                    config_.fromName = task.notificationData.senderName;
                    success = sendCrawlingNotification(task.notificationData);
                    config_.fromName = originalFromName; // Restore original name
                } else {
                    success = sendCrawlingNotification(task.notificationData);
                }
                break;
                
            case EmailTask::GENERIC_EMAIL:
                LOG_DEBUG("EmailService: Processing async generic email for: " + task.to);
                success = sendHtmlEmail(task.to, task.subject, task.htmlContent, task.textContent);
                break;
                
            default:
                LOG_ERROR("EmailService: Unknown email task type: " + std::to_string(static_cast<int>(task.type)));
                return false;
        }
        
        // Update email log if logId is provided
        if (!task.logId.empty()) {
            auto logsStorage = getEmailLogsStorage();
            if (logsStorage) {
                try {
                    if (success) {
                        if (logsStorage->updateEmailLogSent(task.logId)) {
                            LOG_DEBUG("EmailService: Updated email log status to SENT for async task, logId: " + task.logId);
                        } else {
                            LOG_WARNING("EmailService: Failed to update email log status to SENT for async task, logId: " + task.logId + 
                                       ", error: " + logsStorage->getLastError());
                        }
                    } else {
                        if (logsStorage->updateEmailLogFailed(task.logId, lastError_)) {
                            LOG_DEBUG("EmailService: Updated email log status to FAILED for async task, logId: " + task.logId);
                        } else {
                            LOG_WARNING("EmailService: Failed to update email log status to FAILED for async task, logId: " + task.logId + 
                                       ", error: " + logsStorage->getLastError());
                        }
                    }
                } catch (const std::exception& e) {
                    LOG_ERROR("EmailService: Exception updating email log status for async task: " + std::string(e.what()));
                }
            } else {
                LOG_ERROR("EmailService: EmailLogsStorage unavailable for async task log update, logId: " + task.logId);
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        setLastError("Exception in processEmailTask: " + std::string(e.what()));
        LOG_ERROR("EmailService: " + getLastError());
        return false;
    }
}

std::string EmailService::formatCompletionTime(const std::chrono::system_clock::time_point& timePoint, const std::string& language) {
    try {
        // Convert to time_t
        auto time_t = std::chrono::system_clock::to_time_t(timePoint);
        
        // Convert UTC to Tehran time (UTC+3:30) manually
        std::tm* utcTime = std::gmtime(&time_t);
        if (!utcTime) {
            LOG_WARNING("EmailService: Failed to convert time to UTC");
            return "Unknown time";
        }
        
        // Create Tehran time by adding 3 hours 30 minutes
        std::tm tehranTime = *utcTime;
        tehranTime.tm_hour += 3;
        tehranTime.tm_min += 30;
        
        // Handle minute overflow
        if (tehranTime.tm_min >= 60) {
            tehranTime.tm_min -= 60;
            tehranTime.tm_hour++;
        }
        
        // Handle hour overflow
        if (tehranTime.tm_hour >= 24) {
            tehranTime.tm_hour -= 24;
            tehranTime.tm_mday++;
            
            // Handle day overflow (simplified - doesn't handle month/year boundaries perfectly)
            if (tehranTime.tm_mday > 31) {
                tehranTime.tm_mday = 1;
                tehranTime.tm_mon++;
                if (tehranTime.tm_mon >= 12) {
                    tehranTime.tm_mon = 0;
                    tehranTime.tm_year++;
                }
            }
        }
        
        // Format based on language
        if (language == "fa" || language == "fa-IR") {
            // Persian (Shamsi) date formatting
            return convertToPersianDate(tehranTime);
            
        } else {
            // English (Gregorian) date formatting
            char buffer[100];
            std::strftime(buffer, sizeof(buffer), "%B %d, %Y at %H:%M:%S", &tehranTime);
            
            // Add timezone info
            return std::string(buffer) + " (Tehran time)";
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("EmailService: Exception in formatCompletionTime: " + std::string(e.what()));
        return "Unknown time";
    }
}

// Helper function to convert Gregorian date to Persian (Shamsi) date
std::string EmailService::convertToPersianDate(const std::tm& gregorianDate) {
    try {
        int gYear = gregorianDate.tm_year + 1900;
        int gMonth = gregorianDate.tm_mon + 1;
        int gDay = gregorianDate.tm_mday;
        
        // Determine Persian year based on Gregorian date
        // Persian new year (Nowruz) is around March 20/21
        int persianYear;
        if (gMonth < 3 || (gMonth == 3 && gDay < 20)) {
            // Before March 20: still in previous Persian year
            persianYear = gYear - 621;
        } else {
            // March 20 onwards: new Persian year has started
            persianYear = gYear - 621;
        }
        
        // Calculate day of year in Persian calendar
        int persianDayOfYear;
        
        if (gMonth >= 3 && (gMonth > 3 || gDay >= 20)) {
            // From March 20 onwards in current Gregorian year
            int daysInGregorianMonths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
            
            // Check for leap year
            if ((gYear % 4 == 0 && gYear % 100 != 0) || (gYear % 400 == 0)) {
                daysInGregorianMonths[1] = 29;
            }
            
            persianDayOfYear = 0;
            // Add days from March 20 to end of March
            if (gMonth == 3) {
                persianDayOfYear = gDay - 20 + 1;
            } else {
                persianDayOfYear = daysInGregorianMonths[2] - 20 + 1; // Days left in March (12 days)
                // Add full months between April and current month
                for (int m = 4; m < gMonth; m++) {
                    persianDayOfYear += daysInGregorianMonths[m - 1];
                }
                // Add days in current month
                persianDayOfYear += gDay;
            }
        } else {
            // Before March 20: in previous Persian year
            persianYear--;
            
            int daysInGregorianMonths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
            
            // Check for leap year of previous Gregorian year
            int prevGYear = gYear - 1;
            if ((prevGYear % 4 == 0 && prevGYear % 100 != 0) || (prevGYear % 400 == 0)) {
                daysInGregorianMonths[1] = 29;
            }
            
            // Days from March 20 to Dec 31 of previous year
            persianDayOfYear = daysInGregorianMonths[2] - 20 + 1; // Rest of March (12 days)
            for (int m = 4; m <= 12; m++) {
                persianDayOfYear += daysInGregorianMonths[m - 1];
            }
            
            // Add days from Jan 1 to current date
            for (int m = 1; m < gMonth; m++) {
                // Use current year's month days for Jan-Feb
                int currentYearMonthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
                if ((gYear % 4 == 0 && gYear % 100 != 0) || (gYear % 400 == 0)) {
                    currentYearMonthDays[1] = 29;
                }
                persianDayOfYear += currentYearMonthDays[m - 1];
            }
            persianDayOfYear += gDay;
        }
        
        // Persian months: 31, 31, 31, 31, 31, 31, 30, 30, 30, 30, 30, 29/30
        int persianMonthDays[] = {31, 31, 31, 31, 31, 31, 30, 30, 30, 30, 30, 29};
        
        int persianMonth = 1;
        int persianDay = persianDayOfYear;
        
        for (int i = 0; i < 12; i++) {
            if (persianDay <= persianMonthDays[i]) {
                persianMonth = i + 1;
                break;
            }
            persianDay -= persianMonthDays[i];
        }
        
        // Persian month names
        const std::vector<std::string> persianMonths = {
            "فروردین", "اردیبهشت", "خرداد", "تیر", "مرداد", "شهریور",
            "مهر", "آبان", "آذر", "دی", "بهمن", "اسفند"
        };
        
        // Format time
        char timeBuffer[20];
        std::strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &gregorianDate);
        
        // Format Persian date
        std::string persianDate = std::to_string(persianYear) + "/" + 
                                 std::to_string(persianMonth) + "/" + 
                                 std::to_string(persianDay) + 
                                 " (" + persianMonths[persianMonth - 1] + ") " +
                                 "ساعت " + std::string(timeBuffer) + " (تهران)";
        
        LOG_DEBUG("EmailService: Converted Gregorian " + std::to_string(gYear) + "/" + 
                  std::to_string(gMonth) + "/" + std::to_string(gDay) + 
                  " to Persian: " + persianDate);
        
        return persianDate;
        
    } catch (const std::exception& e) {
        LOG_ERROR("EmailService: Exception in convertToPersianDate: " + std::string(e.what()));
        return "تاریخ نامشخص";
    }
}

} } // namespace search_engine::storage
