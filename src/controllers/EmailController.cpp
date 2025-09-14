#include "EmailController.h"
#include "../../include/Logger.h"
#include <regex>
#include <cstdlib>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

EmailController::EmailController() {
    // Empty constructor - using lazy initialization pattern
    // CRITICAL: Never initialize services in constructor to avoid static initialization order fiasco
    LOG_DEBUG("EmailController constructed (services will be lazy-initialized)");
}

search_engine::storage::EmailService* EmailController::getEmailService() const {
    if (!emailService_) {
        try {
            LOG_INFO("Lazy initializing EmailService");
            auto config = loadSMTPConfig();
            emailService_ = std::make_unique<search_engine::storage::EmailService>(config);
            
            // Skip connection test during initialization - test will be done during actual email sending
            // Connection test can be flaky in Docker environments, but actual email sending works
            LOG_INFO("EmailService initialized (connection test skipped during init)");
            
            LOG_INFO("EmailService lazy initialization completed successfully");
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to lazy initialize EmailService: " + std::string(e.what()));
            emailService_.reset();
            return nullptr;
        }
    }
    return emailService_.get();
}

search_engine::storage::EmailService::SMTPConfig EmailController::loadSMTPConfig() const {
    search_engine::storage::EmailService::SMTPConfig config;
    
    // Load configuration from environment variables (works with Docker Compose and .env files)
    const char* smtpHost = std::getenv("SMTP_HOST");
    config.smtpHost = smtpHost ? smtpHost : "smtp.gmail.com";
    
    const char* smtpPort = std::getenv("SMTP_PORT");
    config.smtpPort = smtpPort ? std::stoi(smtpPort) : 587;
    
    const char* smtpUsername = std::getenv("SMTP_USERNAME");
    config.username = smtpUsername ? smtpUsername : "";
    
    const char* smtpPassword = std::getenv("SMTP_PASSWORD");
    config.password = smtpPassword ? smtpPassword : "";
    
    const char* fromEmail = std::getenv("FROM_EMAIL");
    config.fromEmail = fromEmail ? fromEmail : "noreply@hatef.ir";
    
    const char* fromName = std::getenv("FROM_NAME");
    config.fromName = fromName ? fromName : "Hatef.ir Search Engine";
    
    const char* useTLS = std::getenv("SMTP_USE_TLS");
    if (useTLS) {
        std::string tlsStr = std::string(useTLS);
        std::transform(tlsStr.begin(), tlsStr.end(), tlsStr.begin(), ::tolower);
        config.useTLS = (tlsStr == "true" || tlsStr == "1" || tlsStr == "yes");
    } else {
        config.useTLS = true; // Default
    }
    
    const char* useSSL = std::getenv("SMTP_USE_SSL");
    if (useSSL) {
        std::string sslStr = std::string(useSSL);
        std::transform(sslStr.begin(), sslStr.end(), sslStr.begin(), ::tolower);
        config.useSSL = (sslStr == "true" || sslStr == "1" || sslStr == "yes");
    } else {
        config.useSSL = false; // Default
    }
    
    const char* timeout = std::getenv("SMTP_TIMEOUT");
    config.timeoutSeconds = timeout ? std::stoi(timeout) : 30;
    
    LOG_DEBUG("SMTP Config loaded from environment - Host: " + config.smtpHost + 
              ", Port: " + std::to_string(config.smtpPort) + 
              ", Username: " + config.username + 
              ", From: " + config.fromName + " <" + config.fromEmail + ">");
    
    // Check if required configuration is present
    if (config.username.empty() || config.password.empty()) {
        LOG_WARNING("SMTP credentials not configured. Email service may not work properly.");
        LOG_WARNING("Please set SMTP_USERNAME and SMTP_PASSWORD environment variables.");
    }
    
    return config;
}

bool EmailController::isValidEmail(const std::string& email) const {
    // Simple email validation regex
    const std::regex emailRegex(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
    return std::regex_match(email, emailRegex);
}

void EmailController::sendCrawlingNotification(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    LOG_INFO("EmailController::sendCrawlingNotification - Processing crawling notification request");
    LOG_DEBUG("Request from: " + std::string(req->getHeader("user-agent")).substr(0, 50) + "...");
    
    std::string buffer;
    res->onData([this, res, buffer = std::move(buffer)](std::string_view data, bool last) mutable {
        buffer.append(data.data(), data.length());
        
        if (last) {
            try {
                auto jsonBody = nlohmann::json::parse(buffer);
                processCrawlingNotificationRequest(jsonBody, res);
            } catch (const nlohmann::json::parse_error& e) {
                LOG_ERROR("EmailController::sendCrawlingNotification - JSON parse error: " + std::string(e.what()));
                badRequest(res, "Invalid JSON format");
            } catch (const std::exception& e) {
                LOG_ERROR("EmailController::sendCrawlingNotification - Exception: " + std::string(e.what()));
                serverError(res, "Internal server error occurred");
            }
        }
    });

    // CRITICAL: Always add onAborted callback to prevent server crashes
    res->onAborted([this]() {
        LOG_WARNING("EmailController::sendCrawlingNotification - Client disconnected during request processing");
    });
}

void EmailController::sendEmail(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    LOG_INFO("EmailController::sendEmail - Processing generic email request");
    LOG_DEBUG("Request from: " + std::string(req->getHeader("user-agent")).substr(0, 50) + "...");
    
    std::string buffer;
    res->onData([this, res, buffer = std::move(buffer)](std::string_view data, bool last) mutable {
        buffer.append(data.data(), data.length());
        
        if (last) {
            try {
                auto jsonBody = nlohmann::json::parse(buffer);
                processEmailRequest(jsonBody, res);
            } catch (const nlohmann::json::parse_error& e) {
                LOG_ERROR("EmailController::sendEmail - JSON parse error: " + std::string(e.what()));
                badRequest(res, "Invalid JSON format");
            } catch (const std::exception& e) {
                LOG_ERROR("EmailController::sendEmail - Exception: " + std::string(e.what()));
                serverError(res, "Internal server error occurred");
            }
        }
    });

    // CRITICAL: Always add onAborted callback to prevent server crashes
    res->onAborted([this]() {
        LOG_WARNING("EmailController::sendEmail - Client disconnected during request processing");
    });
}

void EmailController::getEmailServiceStatus(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    LOG_INFO("EmailController::getEmailServiceStatus - Checking email service status");
    
    try {
        auto service = getEmailService();
        
        nlohmann::json response;
        response["success"] = true;
        
        if (service) {
            bool connectionOk = service->testConnection();
            response["data"] = {
                {"connected", connectionOk},
                {"lastError", connectionOk ? "" : service->getLastError()},
                {"status", connectionOk ? "operational" : "connection_failed"}
            };
            response["message"] = connectionOk ? "Email service is operational" : "Email service connection failed";
        } else {
            response["data"] = {
                {"connected", false},
                {"lastError", "Service initialization failed"},
                {"status", "initialization_failed"}
            };
            response["message"] = "Email service initialization failed";
        }
        
        json(res, response);
        LOG_INFO("EmailController::getEmailServiceStatus - Status check completed");
        
    } catch (const std::exception& e) {
        LOG_ERROR("EmailController::getEmailServiceStatus - Exception: " + std::string(e.what()));
        serverError(res, "Failed to check email service status");
    }
}

void EmailController::processCrawlingNotificationRequest(const nlohmann::json& jsonBody, uWS::HttpResponse<false>* res) {
    // Validate required fields
    if (!jsonBody.contains("recipientEmail") || !jsonBody.contains("recipientName") || 
        !jsonBody.contains("domainName") || !jsonBody.contains("crawledPagesCount")) {
        badRequest(res, "Missing required fields: recipientEmail, recipientName, domainName, crawledPagesCount");
        return;
    }
    
    std::string recipientEmail = jsonBody["recipientEmail"].get<std::string>();
    std::string recipientName = jsonBody["recipientName"].get<std::string>();
    std::string domainName = jsonBody["domainName"].get<std::string>();
    int crawledPagesCount = jsonBody["crawledPagesCount"].get<int>();
    
    // Optional fields
    std::string crawlSessionId = jsonBody.value("crawlSessionId", "");
    std::string language = jsonBody.value("language", "en");
    
    // Validate email format
    if (!isValidEmail(recipientEmail)) {
        badRequest(res, "Invalid email format");
        return;
    }
    
    // Validate other fields
    if (recipientName.empty() || domainName.empty() || crawledPagesCount < 0) {
        badRequest(res, "Invalid field values");
        return;
    }
    
    LOG_DEBUG("Processing crawling notification for: " + recipientEmail + 
              ", domain: " + domainName + 
              ", pages: " + std::to_string(crawledPagesCount));
    
    // Get email service
    auto service = getEmailService();
    if (!service) {
        serverError(res, "Email service unavailable");
        return;
    }
    
    // Prepare notification data
    search_engine::storage::EmailService::NotificationData data;
    data.recipientEmail = recipientEmail;
    data.recipientName = recipientName;
    data.domainName = domainName;
    data.crawledPagesCount = crawledPagesCount;
    data.crawlSessionId = crawlSessionId;
    data.language = language;
    data.crawlCompletedAt = std::chrono::system_clock::now();
    
    // Load localized subject
    try {
        LOG_DEBUG("Attempting to load localized subject for language: " + language);
        std::string localesPath = "locales/" + language + "/crawling-notification.json";
        std::string localeContent = loadFile(localesPath);
        
        if (localeContent.empty() && language != "en") {
            LOG_WARNING("Failed to load locale file: " + localesPath + ", falling back to English");
            localesPath = "locales/en/crawling-notification.json";
            localeContent = loadFile(localesPath);
        }
        
        if (!localeContent.empty()) {
            LOG_DEBUG("Parsing locale JSON for subject, content size: " + std::to_string(localeContent.length()));
            LOG_DEBUG("First 200 chars of file: " + localeContent.substr(0, 200));
            
            nlohmann::json localeData;
            try {
                localeData = nlohmann::json::parse(localeContent);
                LOG_DEBUG("JSON parsed successfully");
            } catch (const nlohmann::json::parse_error& e) {
                LOG_ERROR("JSON parse error: " + std::string(e.what()) + " at position " + std::to_string(e.byte));
                LOG_ERROR("Content around error: " + localeContent.substr(std::max(0, (int)e.byte - 50), 100));
                throw;
            }
            
            // Debug: Print all top-level keys
            std::string keys = "Available keys: ";
            for (auto& [key, value] : localeData.items()) {
                keys += "'" + key + "' ";
            }
            LOG_DEBUG(keys);
            
            if (localeData.contains("email")) {
                LOG_DEBUG("Found 'email' section in locale data");
                if (localeData["email"].contains("subject")) {
                    LOG_DEBUG("Found 'subject' in email section");
                    std::string subject = localeData["email"]["subject"].get<std::string>();
                    LOG_DEBUG("Raw subject template: " + subject);
                    
                    // Replace {pages} placeholder with actual count
                    size_t pos = subject.find("{pages}");
                    if (pos != std::string::npos) {
                        subject.replace(pos, 7, std::to_string(crawledPagesCount));
                        LOG_DEBUG("Replaced {pages} placeholder");
                    }
                    data.subject = subject;
                    LOG_INFO("Successfully loaded localized subject: " + subject);
                } else {
                    LOG_WARNING("No 'subject' field found in email section");
                }
            } else {
                LOG_WARNING("No 'email' section found in locale data - using hardcoded fallback");
                // Temporary hardcoded fallback while debugging JSON parsing issue
                if (language == "fa") {
                    std::string subject = "خزش تکمیل شد - " + std::to_string(crawledPagesCount) + " صفحه نمایه‌سازی شد";
                    data.subject = subject;
                    LOG_INFO("Using hardcoded Persian subject: " + subject);
                } else {
                    std::string subject = "Crawling Complete - " + std::to_string(crawledPagesCount) + " pages indexed";
                    data.subject = subject;
                    LOG_INFO("Using hardcoded English subject: " + subject);
                }
            }
        } else {
            LOG_WARNING("Locale content is empty after loading");
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Exception while loading localized subject: " + std::string(e.what()));
    }
    
    // Send notification
    bool success = service->sendCrawlingNotification(data);
    
    nlohmann::json response;
    response["success"] = success;
    
    if (success) {
        response["message"] = "Crawling notification sent successfully";
        response["data"] = {
            {"recipientEmail", recipientEmail},
            {"domainName", domainName},
            {"crawledPagesCount", crawledPagesCount},
            {"sentAt", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
        
        LOG_INFO("Crawling notification sent successfully to: " + recipientEmail);
        json(res, response);
    } else {
        response["message"] = "Failed to send crawling notification";
        response["error"] = service->getLastError();
        
        LOG_ERROR("Failed to send crawling notification to: " + recipientEmail + 
                  ", error: " + service->getLastError());
        serverError(res, response);
    }
}

void EmailController::processEmailRequest(const nlohmann::json& jsonBody, uWS::HttpResponse<false>* res) {
    // Validate required fields
    if (!jsonBody.contains("to") || !jsonBody.contains("subject") || !jsonBody.contains("htmlContent")) {
        badRequest(res, "Missing required fields: to, subject, htmlContent");
        return;
    }
    
    std::string to = jsonBody["to"].get<std::string>();
    std::string subject = jsonBody["subject"].get<std::string>();
    std::string htmlContent = jsonBody["htmlContent"].get<std::string>();
    std::string textContent = jsonBody.value("textContent", "");
    
    // Validate email format
    if (!isValidEmail(to)) {
        badRequest(res, "Invalid email format");
        return;
    }
    
    // Validate other fields
    if (subject.empty() || htmlContent.empty()) {
        badRequest(res, "Subject and HTML content cannot be empty");
        return;
    }
    
    LOG_DEBUG("Processing email request to: " + to + ", subject: " + subject);
    
    // Get email service
    auto service = getEmailService();
    if (!service) {
        serverError(res, "Email service unavailable");
        return;
    }
    
    // Send email
    bool success = service->sendHtmlEmail(to, subject, htmlContent, textContent);
    
    nlohmann::json response;
    response["success"] = success;
    
    if (success) {
        response["message"] = "Email sent successfully";
        response["data"] = {
            {"to", to},
            {"subject", subject},
            {"sentAt", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
        
        LOG_INFO("Email sent successfully to: " + to);
        json(res, response);
    } else {
        response["message"] = "Failed to send email";
        response["error"] = service->getLastError();
        
        LOG_ERROR("Failed to send email to: " + to + ", error: " + service->getLastError());
        serverError(res, response);
    }
}

std::string EmailController::loadFile(const std::string& path) const {
    LOG_DEBUG("Attempting to load file: " + path);
    
    if (!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path)) {
        LOG_ERROR("Error: File does not exist or is not a regular file: " + path);
        return "";
    }
    
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR("Error: Could not open file: " + path);
        return "";
    }
    
    file.seekg(0, std::ios::end);
    std::streamsize length = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::string content(length, '\0');
    if (!file.read(content.data(), length)) {
        LOG_ERROR("Error: Failed to read file: " + path);
        return "";
    }
    
    if (content.empty()) {
        LOG_WARNING("Warning: File is empty: " + path);
    } else {
        LOG_INFO("Successfully loaded file: " + path + " (size: " + std::to_string(content.length()) + " bytes)");
    }
    
    return content;
}
