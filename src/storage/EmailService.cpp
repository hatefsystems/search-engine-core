#include "../../include/search_engine/storage/EmailService.h"
#include "../../include/search_engine/storage/UnsubscribeService.h"
#include "../../include/Logger.h"
#include <sstream>
#include <iomanip>
#include <random>
#include <regex>
#include <filesystem>
#include <fstream>
#include <inja/inja.hpp>
#include <nlohmann/json.hpp>

namespace search_engine { namespace storage {

EmailService::EmailService(const SMTPConfig& config) 
    : config_(config), curlHandle_(nullptr) {
    
    // Initialize CURL
    curlHandle_ = curl_easy_init();
    if (!curlHandle_) {
        lastError_ = "Failed to initialize CURL";
        LOG_ERROR("EmailService: Failed to initialize CURL");
        return;
    }
    
    LOG_INFO("EmailService initialized with SMTP host: " + config_.smtpHost + ":" + std::to_string(config_.smtpPort));
}

EmailService::~EmailService() {
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
        // Generate email content
        std::string subject = data.subject.empty() ? 
            "Crawling Complete - " + std::to_string(data.crawledPagesCount) + " pages indexed" : 
            data.subject;
            
        std::string htmlContent = data.htmlContent;
        std::string textContent = data.textContent;
        
        // If no custom content provided, use default template
        if (htmlContent.empty()) {
            htmlContent = generateDefaultNotificationHTML(data);
        }
        
        if (textContent.empty()) {
            textContent = generateDefaultNotificationText(data);
        }
        
        return sendHtmlEmail(data.recipientEmail, subject, htmlContent, textContent);
        
    } catch (const std::exception& e) {
        lastError_ = "Exception in sendCrawlingNotification: " + std::string(e.what());
        LOG_ERROR("EmailService: " + lastError_);
        return false;
    }
}

bool EmailService::sendHtmlEmail(const std::string& to, 
                                const std::string& subject, 
                                const std::string& htmlContent, 
                                const std::string& textContent) {
    
    if (!curlHandle_) {
        lastError_ = "CURL not initialized";
        LOG_ERROR("EmailService: CURL not initialized");
        return false;
    }
    
    LOG_DEBUG("Preparing to send email to: " + to + " with subject: " + subject);
    
    try {
        // Generate unsubscribe token for List-Unsubscribe headers
        std::string unsubscribeToken = "";
        auto unsubscribeService = getUnsubscribeService();
        if (unsubscribeService) {
            unsubscribeToken = unsubscribeService->createUnsubscribeToken(
                to, 
                "", // IP address - not available during email sending
                "Email Sending System" // User agent
            );
            if (!unsubscribeToken.empty()) {
                LOG_DEBUG("EmailService: Generated unsubscribe token for email headers: " + to);
            }
        }
        
        // Prepare email data
        std::string emailData = formatEmailHeaders(to, subject, unsubscribeToken) + 
                               formatEmailBody(htmlContent, textContent);
        
        return performSMTPRequest(to, emailData);
        
    } catch (const std::exception& e) {
        lastError_ = "Exception in sendHtmlEmail: " + std::string(e.what());
        LOG_ERROR("EmailService: " + lastError_);
        return false;
    }
}

bool EmailService::testConnection() {
    LOG_INFO("Testing SMTP connection to: " + config_.smtpHost + ":" + std::to_string(config_.smtpPort));
    
    if (!curlHandle_) {
        lastError_ = "CURL not initialized";
        return false;
    }
    
    // Reset CURL handle
    curl_easy_reset(curlHandle_);
    
    // Configure CURL for connection test
    std::string smtpUrl = "smtps://" + config_.smtpHost + ":" + std::to_string(config_.smtpPort);
    if (!config_.useSSL) {
        smtpUrl = "smtp://" + config_.smtpHost + ":" + std::to_string(config_.smtpPort);
    }
    
    curl_easy_setopt(curlHandle_, CURLOPT_URL, smtpUrl.c_str());
    curl_easy_setopt(curlHandle_, CURLOPT_USERNAME, config_.username.c_str());
    curl_easy_setopt(curlHandle_, CURLOPT_PASSWORD, config_.password.c_str());
    curl_easy_setopt(curlHandle_, CURLOPT_TIMEOUT, config_.timeoutSeconds);
    
    if (config_.useSSL) {
        // For SSL connections (port 465), use CURLUSESSL_ALL
        curl_easy_setopt(curlHandle_, CURLOPT_USE_SSL, CURLUSESSL_ALL);
        curl_easy_setopt(curlHandle_, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curlHandle_, CURLOPT_SSL_VERIFYHOST, 0L);
    } else if (config_.useTLS) {
        // For STARTTLS connections (port 587), use CURLUSESSL_TRY
        curl_easy_setopt(curlHandle_, CURLOPT_USE_SSL, CURLUSESSL_TRY);
        curl_easy_setopt(curlHandle_, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curlHandle_, CURLOPT_SSL_VERIFYHOST, 0L);
        // Additional options for STARTTLS
        curl_easy_setopt(curlHandle_, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(curlHandle_, CURLOPT_TCP_KEEPIDLE, 10L);
        curl_easy_setopt(curlHandle_, CURLOPT_TCP_KEEPINTVL, 10L);
    }
    
    // Perform connection test (just connect, don't send)
    CURLcode res = curl_easy_perform(curlHandle_);
    
    if (res != CURLE_OK) {
        lastError_ = "SMTP connection failed: " + std::string(curl_easy_strerror(res));
        LOG_ERROR("EmailService: " + lastError_);
        return false;
    }
    
    LOG_INFO("SMTP connection test successful");
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

std::string EmailService::formatEmailHeaders(const std::string& to, const std::string& subject, const std::string& unsubscribeToken) {
    std::ostringstream headers;
    
    headers << "To: " << to << "\r\n";
    headers << "From: " << config_.fromName << " <" << config_.fromEmail << ">\r\n";
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
    // Reset CURL handle
    curl_easy_reset(curlHandle_);
    
    // Prepare SMTP URL
    std::string smtpUrl;
    if (config_.useSSL) {
        smtpUrl = "smtps://" + config_.smtpHost + ":" + std::to_string(config_.smtpPort);
    } else {
        smtpUrl = "smtp://" + config_.smtpHost + ":" + std::to_string(config_.smtpPort);
    }
    
    // Prepare recipients list
    struct curl_slist* recipients = nullptr;
    recipients = curl_slist_append(recipients, to.c_str());
    
    // Prepare email buffer
    EmailBuffer buffer;
    buffer.data = emailData;
    buffer.position = 0;
    
    // Configure CURL options
    curl_easy_setopt(curlHandle_, CURLOPT_URL, smtpUrl.c_str());
    curl_easy_setopt(curlHandle_, CURLOPT_USERNAME, config_.username.c_str());
    curl_easy_setopt(curlHandle_, CURLOPT_PASSWORD, config_.password.c_str());
    curl_easy_setopt(curlHandle_, CURLOPT_MAIL_FROM, config_.fromEmail.c_str());
    curl_easy_setopt(curlHandle_, CURLOPT_MAIL_RCPT, recipients);
    curl_easy_setopt(curlHandle_, CURLOPT_READFUNCTION, readCallback);
    curl_easy_setopt(curlHandle_, CURLOPT_READDATA, &buffer);
    curl_easy_setopt(curlHandle_, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curlHandle_, CURLOPT_TIMEOUT, config_.timeoutSeconds);
    
    // TLS/SSL configuration
    if (config_.useSSL) {
        // For SSL connections (port 465), use CURLUSESSL_ALL
        curl_easy_setopt(curlHandle_, CURLOPT_USE_SSL, CURLUSESSL_ALL);
        curl_easy_setopt(curlHandle_, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curlHandle_, CURLOPT_SSL_VERIFYHOST, 0L);
    } else if (config_.useTLS) {
        // For STARTTLS connections (port 587), use CURLUSESSL_TRY
        curl_easy_setopt(curlHandle_, CURLOPT_USE_SSL, CURLUSESSL_TRY);
        curl_easy_setopt(curlHandle_, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curlHandle_, CURLOPT_SSL_VERIFYHOST, 0L);
    }
    
    // Perform the request
    CURLcode res = curl_easy_perform(curlHandle_);
    
    // Clean up
    curl_slist_free_all(recipients);
    
    if (res != CURLE_OK) {
        lastError_ = "SMTP request failed: " + std::string(curl_easy_strerror(res));
        LOG_ERROR("EmailService: " + lastError_);
        return false;
    }
    
    // Get response code
    long responseCode = 0;
    curl_easy_getinfo(curlHandle_, CURLINFO_RESPONSE_CODE, &responseCode);
    
    LOG_DEBUG("SMTP response code: " + std::to_string(responseCode));
    
    if (responseCode >= 200 && responseCode < 300) {
        LOG_INFO("Email sent successfully to: " + to);
        return true;
    } else {
        lastError_ = "SMTP server returned error code: " + std::to_string(responseCode);
        LOG_ERROR("EmailService: " + lastError_);
        return false;
    }
}

std::string EmailService::generateDefaultNotificationHTML(const NotificationData& data) {
    LOG_INFO("EmailService: Using Inja template-based email generation");
    
    // Render the email template
    std::string templateHTML = renderEmailTemplate("email-crawling-notification.inja", data);
    
    if (templateHTML.empty()) {
        LOG_ERROR("EmailService: Template rendering failed and no fallback available");
        throw std::runtime_error("Failed to render email template");
    }
    
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
        
        // Prepare template data
        nlohmann::json templateData = localeData;
        // Don't overwrite the language object, just add the current language as a separate field
        templateData["currentLanguage"] = data.language;
        templateData["recipientName"] = data.recipientName;
        templateData["domainName"] = data.domainName;
        templateData["crawledPagesCount"] = data.crawledPagesCount;
        templateData["crawlSessionId"] = data.crawlSessionId;
        
        // Format completion time
        auto time_t = std::chrono::system_clock::to_time_t(data.crawlCompletedAt);
        templateData["completionTime"] = static_cast<long long>(time_t);
        
        // Generate unsubscribe token
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

} } // namespace search_engine::storage
