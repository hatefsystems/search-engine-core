#include "HomeController.h"
#include "../../include/Logger.h"
#include "../../include/api.h"
#include "../../include/mongodb.h"
#include "../../include/search_engine/storage/SponsorStorage.h"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <curl/curl.h>

// Deep merge helper: fill missing keys in dst with values from src (recursively for objects)
static void jsonDeepMergeMissing(nlohmann::json &dst, const nlohmann::json &src) {
    if (!dst.is_object() || !src.is_object()) return;
    for (auto it = src.begin(); it != src.end(); ++it) {
        const std::string &key = it.key();
        if (dst.contains(key)) {
            if (dst[key].is_object() && it.value().is_object()) {
                jsonDeepMergeMissing(dst[key], it.value());
            }
        } else {
            dst[key] = it.value();
        }
    }
}

// Format integer with thousands separators (e.g., 1000000 -> 1,000,000)
static std::string formatThousands(long long value) {
    bool isNegative = value < 0;
    unsigned long long v = isNegative ? static_cast<unsigned long long>(-value) : static_cast<unsigned long long>(value);
    std::string digits = std::to_string(v);
    std::string out;
    out.reserve(digits.size() + digits.size() / 3);
    int count = 0;
    for (int i = static_cast<int>(digits.size()) - 1; i >= 0; --i) {
        out.push_back(digits[static_cast<size_t>(i)]);
        count++;
        if (i > 0 && count % 3 == 0) {
            out.push_back(',');
        }
    }
    std::reverse(out.begin(), out.end());
    if (isNegative) out.insert(out.begin(), '-');
    return out;
}

std::string HomeController::loadFile(const std::string& path) {
    LOG_DEBUG("Attempting to load file: " + path);
    
    if (!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path)) {
        LOG_ERROR("Error: File does not exist or is not a regular file: " + path);
        return "";
    }
    
    std::ifstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("Error: Could not open file: " + path);
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    if (content.empty()) {
        LOG_WARNING("Warning: File is empty: " + path);
    } else {
        LOG_INFO("Successfully loaded file: " + path + " (size: " + std::to_string(content.length()) + " bytes)");
    }
    
    return content;
}

void HomeController::index(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    LOG_INFO("HomeController::index - Serving coming soon page");
    LOG_DEBUG("HomeController::index called from: " + std::string(req->getHeader("user-agent")).substr(0, 50) + "...");

    // Load and serve the coming soon page
    static std::string comingSoonHtml = loadFile("public/coming-soon.html");

    if (comingSoonHtml.empty()) {
        LOG_ERROR("HomeController::index - Failed to load coming soon page");
        serverError(res, "Failed to load page");
        return;
    }

    LOG_DEBUG("HomeController::index - Serving coming soon page (size: " + std::to_string(comingSoonHtml.size()) + " bytes)");
    html(res, comingSoonHtml);
    LOG_TRACE("HomeController::index - Response sent successfully");
}

void HomeController::searchPage(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    LOG_INFO("HomeController::searchPage - Serving search engine interface");
    LOG_DEBUG("HomeController::searchPage called from: " + std::string(req->getHeader("user-agent")).substr(0, 50) + "...");

    // Load and serve the search engine page
    static std::string searchIndexHtml = loadFile("public/index.html");

    if (searchIndexHtml.empty()) {
        LOG_ERROR("HomeController::searchPage - Failed to load search interface");
        serverError(res, "Failed to load page");
        return;
    }

    LOG_DEBUG("HomeController::searchPage - Serving search interface (size: " + std::to_string(searchIndexHtml.size()) + " bytes)");
    html(res, searchIndexHtml);
    LOG_TRACE("HomeController::searchPage - Response sent successfully");
}

std::string HomeController::renderTemplate(const std::string& templateName, const nlohmann::json& data) {
    try {
        // Initialize Inja environment
        inja::Environment env("templates/");
        
        // Register template functions
        env.add_callback("formatThousands", 1, [](inja::Arguments& args) {
            try {
                if (args.empty()) return std::string("0");
                
                // Handle different numeric types
                if (args[0]->is_number_integer()) {
                    long long value = args[0]->get<long long>();
                    return formatThousands(value);
                } else if (args[0]->is_number()) {
                    double value = args[0]->get<double>();
                    return formatThousands(static_cast<long long>(value));
                }
                return std::string("0");
            } catch (...) {
                return std::string("0");
            }
        });
        
        env.add_callback("formatTime", 1, [](inja::Arguments& args) {
            try {
                if (args.empty()) return std::string("00:00:00");
                
                long long timestamp = 0;
                if (args[0]->is_number_integer()) {
                    timestamp = args[0]->get<long long>();
                } else if (args[0]->is_number()) {
                    timestamp = static_cast<long long>(args[0]->get<double>());
                }
                
                std::time_t time = static_cast<std::time_t>(timestamp);
                std::tm* tm = std::localtime(&time);
                char buffer[32];
                std::strftime(buffer, sizeof(buffer), "%H:%M:%S", tm);
                return std::string(buffer);
            } catch (...) {
                return std::string("00:00:00");
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
        
        // Load the template and render with data  
        std::string result = env.render_file(templateName, data);
        return result;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Template rendering error: " + std::string(e.what()));
        return "";
    }
}

void HomeController::sponsorPage(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    LOG_INFO("HomeController::sponsorPage - Serving sponsor page");
    LOG_DEBUG("HomeController::sponsorPage called from: " + std::string(req->getHeader("user-agent")).substr(0, 50) + "...");

    try {
        LOG_DEBUG("HomeController::sponsorPage - Loading default locale configuration");
        std::string defaultLang = getDefaultLocale();
        // Load language metadata (code/dir) from root locale file
        std::string metaData = loadFile("locales/" + defaultLang + ".json");
        if (metaData.empty()) {
            LOG_ERROR("HomeController::sponsorPage - Failed to load localization metadata for language: " + defaultLang);
            serverError(res, "Failed to load localization metadata");
            return;
        }

        LOG_DEBUG("HomeController::sponsorPage - Parsing metadata JSON for language: " + defaultLang);
        nlohmann::json metaJson = nlohmann::json::parse(metaData);

        LOG_DEBUG("HomeController::sponsorPage - Loading sponsor page translations");
        // Load sponsor page translations (primary=default for base route)
        std::string sponsorPrimaryStr = loadFile("locales/" + defaultLang + "/sponsor.json");
        std::string sponsorFallbackStr = loadFile("locales/" + getDefaultLocale() + "/sponsor.json");
        nlohmann::json sponsorPrimary = sponsorPrimaryStr.empty() ? nlohmann::json::object() : nlohmann::json::parse(sponsorPrimaryStr);
        nlohmann::json sponsorFallback = sponsorFallbackStr.empty() ? nlohmann::json::object() : nlohmann::json::parse(sponsorFallbackStr);
        jsonDeepMergeMissing(sponsorPrimary, sponsorFallback);

        LOG_DEBUG("HomeController::sponsorPage - Merged sponsor translations successfully");

        // Pre-format tier prices with thousands separators
        try {
            if (sponsorPrimary.contains("tiers") && sponsorPrimary["tiers"].is_array()) {
                for (auto &tier : sponsorPrimary["tiers"]) {
                    if (tier.contains("priceUsdYear") && tier["priceUsdYear"].is_number_integer()) {
                        long long py = tier["priceUsdYear"].get<long long>();
                        tier["priceUsdYearFmt"] = formatThousands(py);
                    }
                    if (tier.contains("priceUsdMonth") && (tier["priceUsdMonth"].is_number_integer() || tier["priceUsdMonth"].is_number_float())) {
                        // treat as integer display
                        long long pm = 0;
                        if (tier["priceUsdMonth"].is_number_integer()) pm = tier["priceUsdMonth"].get<long long>();
                        else pm = static_cast<long long>(tier["priceUsdMonth"].get<double>());
                        tier["priceUsdMonthFmt"] = formatThousands(pm);
                    }
                }
            }
        } catch (...) { /* ignore formatting errors */ }

        nlohmann::json t;
        if (metaJson.contains("language")) t["language"] = metaJson["language"];
        t["sponsor"] = sponsorPrimary;

        // Get the host from the request headers
        std::string host = std::string(req->getHeader("host"));
        std::string protocol = "http://";
        
        // Check if we're behind a proxy (X-Forwarded-Proto header)
        std::string forwardedProto = std::string(req->getHeader("x-forwarded-proto"));
        if (!forwardedProto.empty()) {
            protocol = forwardedProto + "://";
        }
        
        std::string baseUrl = protocol + host;

        nlohmann::json templateData = {
            {"t", t},
            {"base_url", baseUrl}
        };
        std::string renderedHtml = renderTemplate("sponsor.inja", templateData);
        if (renderedHtml.empty()) {
            serverError(res, "Failed to render sponsor template");
            return;
        }
        html(res, renderedHtml);
    } catch (const std::exception& e) {
        LOG_ERROR("Error in sponsorPage: " + std::string(e.what()));
        serverError(res, "Failed to load sponsor page");
    }
}

void HomeController::sponsorPageWithLang(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    LOG_INFO("HomeController::sponsorPageWithLang called");
    try {
        std::string url = std::string(req->getUrl());
        size_t lastSlash = url.find_last_of('/');
        std::string langCode;
        if (lastSlash != std::string::npos && lastSlash < url.length() - 1) {
            langCode = url.substr(lastSlash + 1);
        }
        std::string metaFile = "locales/" + langCode + ".json";
        if (!std::filesystem::exists(metaFile)) {
            langCode = getDefaultLocale();
            metaFile = "locales/" + langCode + ".json";
        }
        std::string metaData = loadFile(metaFile);
        if (metaData.empty()) {
            serverError(res, "Failed to load localization metadata for language: " + langCode);
            return;
        }
        nlohmann::json metaJson = nlohmann::json::parse(metaData);

        // Load sponsor translations for requested lang with fallback to default
        std::string sponsorPrimaryStr = loadFile("locales/" + langCode + "/sponsor.json");
        std::string sponsorFallbackStr = loadFile("locales/" + getDefaultLocale() + "/sponsor.json");
        nlohmann::json sponsorPrimary = sponsorPrimaryStr.empty() ? nlohmann::json::object() : nlohmann::json::parse(sponsorPrimaryStr);
        nlohmann::json sponsorFallback = sponsorFallbackStr.empty() ? nlohmann::json::object() : nlohmann::json::parse(sponsorFallbackStr);
        jsonDeepMergeMissing(sponsorPrimary, sponsorFallback);

        // Pre-format tier prices with thousands separators
        try {
            if (sponsorPrimary.contains("tiers") && sponsorPrimary["tiers"].is_array()) {
                for (auto &tier : sponsorPrimary["tiers"]) {
                    if (tier.contains("priceUsdYear") && tier["priceUsdYear"].is_number_integer()) {
                        long long py = tier["priceUsdYear"].get<long long>();
                        tier["priceUsdYearFmt"] = formatThousands(py);
                    }
                    if (tier.contains("priceUsdMonth") && (tier["priceUsdMonth"].is_number_integer() || tier["priceUsdMonth"].is_number_float())) {
                        long long pm = 0;
                        if (tier["priceUsdMonth"].is_number_integer()) pm = tier["priceUsdMonth"].get<long long>();
                        else pm = static_cast<long long>(tier["priceUsdMonth"].get<double>());
                        tier["priceUsdMonthFmt"] = formatThousands(pm);
                    }
                }
            }
        } catch (...) { /* ignore formatting errors */ }

        nlohmann::json t;
        if (metaJson.contains("language")) t["language"] = metaJson["language"];
        t["sponsor"] = sponsorPrimary;

        // Get the host from the request headers
        std::string host = std::string(req->getHeader("host"));
        std::string protocol = "http://";
        
        // Check if we're behind a proxy (X-Forwarded-Proto header)
        std::string forwardedProto = std::string(req->getHeader("x-forwarded-proto"));
        if (!forwardedProto.empty()) {
            protocol = forwardedProto + "://";
        }
        
        std::string baseUrl = protocol + host;

        nlohmann::json templateData = {
            {"t", t},
            {"base_url", baseUrl}
        };
        std::string renderedHtml = renderTemplate("sponsor.inja", templateData);
        if (renderedHtml.empty()) {
            serverError(res, "Failed to render sponsor template");
            return;
        }
        html(res, renderedHtml);
    } catch (const std::exception& e) {
        LOG_ERROR("Error in sponsorPageWithLang: " + std::string(e.what()));
        serverError(res, "Failed to load sponsor page with language");
    }
}

void HomeController::crawlRequestPage(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    LOG_INFO("HomeController::crawlRequestPage called");
    
    try {
        // Load default language metadata
        std::string defaultLang = getDefaultLocale();
        std::string metaStr = loadFile("locales/" + defaultLang + ".json");
        if (metaStr.empty()) { serverError(res, "Failed to load localization metadata"); return; }
        nlohmann::json metaJson = nlohmann::json::parse(metaStr);

        // Load page-specific translations for default lang with fallback to default root (for compatibility)
        std::string pagePrimaryStr = loadFile("locales/" + defaultLang + "/crawl-request.json");
        std::string pageFallbackStr = loadFile("locales/" + defaultLang + ".json");
        nlohmann::json pagePrimary = pagePrimaryStr.empty() ? nlohmann::json::object() : nlohmann::json::parse(pagePrimaryStr);
        nlohmann::json pageFallback = pageFallbackStr.empty() ? nlohmann::json::object() : nlohmann::json::parse(pageFallbackStr);
        jsonDeepMergeMissing(pagePrimary, pageFallback);

        // Compose template data
        nlohmann::json t = pagePrimary;
        if (metaJson.contains("language")) t["language"] = metaJson["language"];
        
        // Get the host from the request headers
        std::string host = std::string(req->getHeader("host"));
        std::string protocol = "http://";
        
        // Check if we're behind a proxy (X-Forwarded-Proto header)
        std::string forwardedProto = std::string(req->getHeader("x-forwarded-proto"));
        if (!forwardedProto.empty()) {
            protocol = forwardedProto + "://";
        }
        
        std::string baseUrl = protocol + host;
        
        nlohmann::json templateData = {
            {"t", t},
            {"base_url", baseUrl}
        };

        // Render template with data
        std::string renderedHtml = renderTemplate("crawl-request-full.inja", templateData);
        
        if (renderedHtml.empty()) {
            serverError(res, "Failed to render template");
            return;
        }
        
        html(res, renderedHtml);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error in crawlRequestPage: " + std::string(e.what()));
        serverError(res, "Failed to load crawl request page");
    }
}

void HomeController::crawlRequestPageWithLang(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    LOG_INFO("HomeController::crawlRequestPageWithLang called");
    
    try {
        // Extract language code from URL path
        std::string url = std::string(req->getUrl());
        LOG_INFO("Full URL: " + url);
        
        // Extract language code from /crawl-request/lang format
        std::string langCode;
        size_t lastSlash = url.find_last_of('/');
        if (lastSlash != std::string::npos && lastSlash < url.length() - 1) {
            langCode = url.substr(lastSlash + 1);
        }
        
        LOG_INFO("Extracted language code: " + langCode);
        
        // Check if language meta file exists, fallback to default if not
        std::string metaFile = "locales/" + langCode + ".json";
        if (!std::filesystem::exists(metaFile)) {
            LOG_WARNING("Language file not found: " + metaFile + ", falling back to default");
            langCode = getDefaultLocale();
            metaFile = "locales/" + langCode + ".json";
        }

        // Load language metadata
        std::string metaStr = loadFile(metaFile);
        if (metaStr.empty()) { serverError(res, "Failed to load localization metadata for language: " + langCode); return; }
        nlohmann::json metaJson = nlohmann::json::parse(metaStr);

        // Load page-specific translations with layered fallback: page(lang) <- root(lang) <- page(default) <- root(default)
        std::string pagePrimaryStr = loadFile("locales/" + langCode + "/crawl-request.json");
        std::string rootLangStr    = loadFile("locales/" + langCode + ".json");
        std::string pageDefaultStr = loadFile("locales/" + getDefaultLocale() + "/crawl-request.json");
        std::string rootDefaultStr = loadFile("locales/" + getDefaultLocale() + ".json");
        nlohmann::json j = nlohmann::json::object();
        if (!rootDefaultStr.empty()) j = nlohmann::json::parse(rootDefaultStr);
        if (!pageDefaultStr.empty()) jsonDeepMergeMissing(j, nlohmann::json::parse(pageDefaultStr));
        if (!rootLangStr.empty())    jsonDeepMergeMissing(j, nlohmann::json::parse(rootLangStr));
        if (!pagePrimaryStr.empty()) jsonDeepMergeMissing(j, nlohmann::json::parse(pagePrimaryStr));

        nlohmann::json t = j;
        if (metaJson.contains("language")) t["language"] = metaJson["language"];
        
        // Get the host from the request headers
        std::string host = std::string(req->getHeader("host"));
        std::string protocol = "http://";
        
        // Check if we're behind a proxy (X-Forwarded-Proto header)
        std::string forwardedProto = std::string(req->getHeader("x-forwarded-proto"));
        if (!forwardedProto.empty()) {
            protocol = forwardedProto + "://";
        }
        
        std::string baseUrl = protocol + host;
        
        LOG_INFO("Setting base_url for template: " + baseUrl);
        LOG_INFO("Host header: " + host);
        LOG_INFO("Protocol: " + protocol);
        LOG_INFO("Forwarded proto: " + forwardedProto);
        
        nlohmann::json templateData = {
            {"t", t},
            {"base_url", baseUrl}
        };

        // Render template with data
        std::string renderedHtml = renderTemplate("crawl-request-full.inja", templateData);
        
        if (renderedHtml.empty()) {
            serverError(res, "Failed to render template");
            return;
        }
        
        html(res, renderedHtml);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error in crawlRequestPageWithLang: " + std::string(e.what()));
        serverError(res, "Failed to load crawl request page with language");
    }
}

void HomeController::emailSubscribe(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    LOG_INFO("HomeController::emailSubscribe - Processing email subscription request");
    LOG_DEBUG("HomeController::emailSubscribe - Request from: " + std::string(req->getHeader("user-agent")).substr(0, 50) + "...");

    // Read the request body
    std::string buffer;
    res->onData([this, res, req, buffer = std::move(buffer)](std::string_view data, bool last) mutable {
        buffer.append(data.data(), data.length());

        if (last) {
            LOG_DEBUG("HomeController::emailSubscribe - Received complete request body (" + std::to_string(buffer.size()) + " bytes)");

            try {
                LOG_TRACE("HomeController::emailSubscribe - Parsing JSON request body");
                // Parse JSON body
                auto jsonBody = nlohmann::json::parse(buffer);
                std::string email = jsonBody.value("email", "");

                if (email.empty()) {
                    LOG_WARNING("HomeController::emailSubscribe - Empty email field in request");
                    badRequest(res, "Email is required");
                    return;
                }

                LOG_DEBUG("HomeController::emailSubscribe - Processing subscription for email: " + email);
                
                // Get IP address and user agent
                LOG_TRACE("HomeController::emailSubscribe - Extracting client information");
                std::string ipAddress = std::string(req->getHeader("x-forwarded-for"));
                if (ipAddress.empty()) {
                    ipAddress = std::string(req->getHeader("x-real-ip"));
                }
                if (ipAddress.empty()) {
                    ipAddress = "unknown";
                }

                std::string userAgent = std::string(req->getHeader("user-agent"));
                if (userAgent.empty()) {
                    userAgent = "unknown";
                }

                LOG_DEBUG("HomeController::emailSubscribe - Client info: IP=" + ipAddress + ", UA=" + userAgent.substr(0, 30) + "...");

                // Save email to MongoDB with additional data
                try {
                    LOG_DEBUG("HomeController::emailSubscribe - Saving subscription to database");
                    auto result = mongodb().subscribeEmail(email, ipAddress, userAgent);

                    if (result.success) {
                        LOG_INFO("✅ Email subscription successful: " + email + " from IP: " + ipAddress);
                        LOG_DEBUG("HomeController::emailSubscribe - Database message: " + result.message);
                        nlohmann::json response = {
                            {"success", true},
                            {"message", result.message}
                        };
                        json(res, response);
                        LOG_TRACE("HomeController::emailSubscribe - Success response sent");
                    } else {
                        LOG_WARNING("❌ Email subscription failed: " + email + " - " + result.message);
                        if (result.message == "duplicate") {
                            LOG_DEBUG("HomeController::emailSubscribe - Duplicate subscription detected");
                            badRequest(res, "You are already subscribed!");
                        } else {
                            LOG_DEBUG("HomeController::emailSubscribe - Subscription failed with message: " + result.message);
                            badRequest(res, "Failed to subscribe: " + result.message);
                        }
                    }
                } catch (const std::exception& e) {
                    LOG_ERROR("💥 MongoDB error in email subscription: " + std::string(e.what()));
                    badRequest(res, "Database error occurred");
                }
                
            } catch (const std::exception& e) {
                LOG_ERROR("❌ Failed to parse email subscription JSON: " + std::string(e.what()));
                badRequest(res, "Invalid request body");
            }
        }
    });

    res->onAborted([]() {
        LOG_WARNING("⚠️ Email subscription request aborted by client");
    });
}

std::string HomeController::getAvailableLocales() {
    try {
        std::vector<std::string> locales;
        
        // Scan the locales directory for JSON files
        for (const auto& entry : std::filesystem::directory_iterator("locales/")) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                std::string filename = entry.path().stem().string();
                // Skip test files or other non-locale files
                if (filename != "test-data") {
                    locales.push_back(filename);
                }
            }
        }
        
        // Convert to JSON array string for client-side usage if needed
        nlohmann::json localeArray = locales;
        return localeArray.dump();
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error getting available locales: " + std::string(e.what()));
        return "[\"en\"]"; // Fallback to English only
    }
}

std::string HomeController::getDefaultLocale() {
    return "fa"; // Persian as default
}

void HomeController::sponsorSubmit(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    LOG_INFO("🏢 HomeController::sponsorSubmit - Processing sponsor application");
    LOG_DEBUG("HomeController::sponsorSubmit - Request from: " + std::string(req->getHeader("user-agent")).substr(0, 50) + "...");

    // Read the request body
    std::string buffer;
    res->onData([this, res, req, buffer = std::move(buffer)](std::string_view data, bool last) mutable {
        buffer.append(data.data(), data.length());

        if (last) {
            LOG_DEBUG("HomeController::sponsorSubmit - Received complete request body (" + std::to_string(buffer.size()) + " bytes)");

            try {
                LOG_TRACE("HomeController::sponsorSubmit - Parsing JSON request body");
                // Parse JSON body
                auto jsonBody = nlohmann::json::parse(buffer);

                LOG_TRACE("HomeController::sponsorSubmit - Extracting and validating form fields");
                // Validate required fields
                std::string fullname = jsonBody.value("name", "");
                std::string email = jsonBody.value("email", "");
                std::string mobile = jsonBody.value("mobile", "");
                std::string plan = jsonBody.value("tier", "");

                if (fullname.empty() || email.empty() || mobile.empty() || plan.empty()) {
                    LOG_WARNING("HomeController::sponsorSubmit - Missing required fields");
                    LOG_DEBUG("HomeController::sponsorSubmit - Received: name='" + fullname + "', email='" + email + "', mobile='" + mobile + "', tier='" + plan + "'");
                    badRequest(res, "Missing required fields: name, email, mobile, tier");
                    return;
                }

                LOG_DEBUG("HomeController::sponsorSubmit - Validated sponsor: " + fullname + " (" + email + ") - Plan: " + plan);
                
                // Get amount
                LOG_TRACE("HomeController::sponsorSubmit - Processing amount field");
                double amount = 0.0;
                if (jsonBody.contains("amount")) {
                    if (jsonBody["amount"].is_number()) {
                        amount = jsonBody["amount"];
                        LOG_DEBUG("HomeController::sponsorSubmit - Amount parsed as number: " + std::to_string(amount));
                    } else if (jsonBody["amount"].is_string()) {
                        try {
                            amount = std::stod(jsonBody["amount"].get<std::string>());
                            LOG_DEBUG("HomeController::sponsorSubmit - Amount parsed from string: " + std::to_string(amount));
                        } catch (const std::exception&) {
                            LOG_ERROR("HomeController::sponsorSubmit - Invalid amount format in string");
                            badRequest(res, "Invalid amount format");
                            return;
                        }
                    } else {
                        LOG_WARNING("HomeController::sponsorSubmit - Amount field has unexpected type");
                    }
                } else {
                    LOG_DEBUG("HomeController::sponsorSubmit - No amount field provided");
                }

                // Get optional company
                std::string company = jsonBody.value("company", "");
                if (!company.empty()) {
                    LOG_DEBUG("HomeController::sponsorSubmit - Company provided: " + company);
                }

                // Get IP address and user agent
                LOG_TRACE("HomeController::sponsorSubmit - Extracting client information");
                std::string ipAddress = std::string(req->getHeader("x-forwarded-for"));
                if (ipAddress.empty()) {
                    ipAddress = std::string(req->getHeader("x-real-ip"));
                    if (!ipAddress.empty()) {
                        LOG_TRACE("HomeController::sponsorSubmit - Using X-Real-IP header: " + ipAddress);
                    }
                } else {
                    LOG_TRACE("HomeController::sponsorSubmit - Using X-Forwarded-For header: " + ipAddress);
                }
                if (ipAddress.empty()) {
                    // Fallback to connection IP if no forwarded headers
                    ipAddress = "unknown";
                    LOG_DEBUG("HomeController::sponsorSubmit - No forwarded IP headers found, using 'unknown'");
                }

                std::string userAgent = std::string(req->getHeader("user-agent"));
                LOG_DEBUG("HomeController::sponsorSubmit - Client info: IP=" + ipAddress + ", UA=" + userAgent.substr(0, 30) + "...");
                
                // Create sponsor profile
                LOG_TRACE("HomeController::sponsorSubmit - Creating sponsor profile object");
                search_engine::storage::SponsorProfile profile;
                profile.fullName = fullname;
                profile.email = email;
                profile.mobile = mobile;
                profile.plan = plan;
                profile.amount = amount;

                if (!company.empty()) {
                    profile.company = company;
                    LOG_TRACE("HomeController::sponsorSubmit - Company field set: " + company);
                }

                profile.ipAddress = ipAddress;
                profile.userAgent = userAgent;
                profile.submissionTime = std::chrono::system_clock::now();
                profile.lastModified = std::chrono::system_clock::now();
                profile.status = search_engine::storage::SponsorStatus::PENDING;
                profile.currency = "IRR"; // Default to Iranian Rial

                LOG_DEBUG("HomeController::sponsorSubmit - Sponsor profile created:");
                LOG_DEBUG("  Name: " + fullname + ", Email: " + email + ", Mobile: " + mobile);
                LOG_DEBUG("  Plan: " + plan + ", Amount: " + std::to_string(amount) + " " + profile.currency);
                LOG_DEBUG("  Status: PENDING, IP: " + ipAddress);
                
                // Save to database with better error handling
                LOG_INFO("💾 Starting database save process for sponsor: " + fullname);
                LOG_DEBUG("HomeController::sponsorSubmit - Preparing to save sponsor profile to MongoDB");

                try {
                    LOG_TRACE("HomeController::sponsorSubmit - Retrieving MongoDB connection configuration");
                    // Get MongoDB connection string from environment
                    const char* mongoUri = std::getenv("MONGODB_URI");
                    std::string mongoConnectionString = mongoUri ? mongoUri : "mongodb://localhost:27017";

                    LOG_INFO("📊 MongoDB URI from environment: " + mongoConnectionString);
                    LOG_DEBUG("HomeController::sponsorSubmit - Database connection string configured");

                    // Now try to actually save to MongoDB
                    LOG_INFO("💾 Attempting to save sponsor data to MongoDB:");
                    LOG_DEBUG("HomeController::sponsorSubmit - Initiating database transaction");
                    LOG_INFO("  Name: " + fullname);
                    LOG_INFO("  Email: " + email);
                    LOG_INFO("  Mobile: " + mobile);
                    LOG_INFO("  Plan: " + plan);
                    LOG_INFO("  Amount: " + std::to_string(amount));
                    LOG_INFO("  Company: " + company);
                    LOG_INFO("  IP: " + ipAddress);
                    LOG_INFO("  User Agent: " + userAgent);
                    
                    // Save directly to MongoDB database
                    std::string actualSubmissionId;
                    bool savedToDatabase = false;
                    
                    try {
                        LOG_TRACE("HomeController::sponsorSubmit - Establishing database connection");
                        // Get MongoDB connection string from environment
                        const char* mongoUri = std::getenv("MONGODB_URI");
                        std::string mongoConnectionString = mongoUri ? mongoUri : "mongodb://admin:password123@mongodb_test:27017/search-engine";

                        LOG_INFO("🔗 Attempting to save sponsor data to MongoDB: " + mongoConnectionString);
                        LOG_DEBUG("HomeController::sponsorSubmit - Connection string: " + mongoConnectionString);

                        LOG_TRACE("HomeController::sponsorSubmit - Creating SponsorStorage instance");
                        // Create SponsorStorage and save the profile
                        search_engine::storage::SponsorStorage storage(mongoConnectionString, "search-engine");

                        LOG_TRACE("HomeController::sponsorSubmit - Calling storage.store() method");
                        auto result = storage.store(profile);

                        if (result.success) {
                            actualSubmissionId = result.value;
                            savedToDatabase = true;
                            LOG_INFO("✅ Successfully saved sponsor data to MongoDB with ID: " + actualSubmissionId);
                            LOG_DEBUG("HomeController::sponsorSubmit - Database transaction completed successfully");
                        } else {
                            LOG_ERROR("❌ Failed to save to MongoDB: " + result.message);
                            LOG_DEBUG("HomeController::sponsorSubmit - Generating fallback submission ID");
                            // Generate fallback ID
                            auto now = std::chrono::system_clock::now();
                            auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
                            actualSubmissionId = "temp_" + std::to_string(timestamp);
                            LOG_WARNING("HomeController::sponsorSubmit - Using temporary ID: " + actualSubmissionId);
                        }

                    } catch (const std::exception& e) {
                        LOG_ERROR("💥 Exception while saving to MongoDB: " + std::string(e.what()));
                        LOG_DEBUG("HomeController::sponsorSubmit - Generating fallback submission ID due to exception");
                        // Generate fallback ID
                        auto now = std::chrono::system_clock::now();
                        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
                        actualSubmissionId = "temp_" + std::to_string(timestamp);
                        LOG_WARNING("HomeController::sponsorSubmit - Using temporary ID due to exception: " + actualSubmissionId);
                    }
                    
                    // Fetch payment accounts from JSON file
                    LOG_DEBUG("HomeController::sponsorSubmit - Fetching payment account information");
                    nlohmann::json bankInfo;
                    try {
                        std::string url = "https://cdn.hatef.ir/sponsor_payment_accounts.json";
                        LOG_TRACE("HomeController::sponsorSubmit - Payment accounts URL: " + url);

                        LOG_TRACE("HomeController::sponsorSubmit - Initializing CURL for payment accounts fetch");
                        CURL* curl = curl_easy_init();
                        if (curl) {
                            LOG_TRACE("HomeController::sponsorSubmit - CURL initialized successfully");
                            std::string response_data;
                            
                            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](void* contents, size_t size, size_t nmemb, std::string* userp) {
                                userp->append((char*)contents, size * nmemb);
                                return size * nmemb;
                            });
                            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
                            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
                            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
                            curl_easy_setopt(curl, CURLOPT_USERAGENT, "SearchEngine/1.0");
                            
                            CURLcode res_code = curl_easy_perform(curl);
                            long http_code = 0;
                            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
                            curl_easy_cleanup(curl);
                            
                            if (res_code == CURLE_OK && http_code == 200) {
                                auto json_data = nlohmann::json::parse(response_data);
                                
                                // Get the first active account
                                if (json_data.contains("sponsor_payment_accounts") && json_data["sponsor_payment_accounts"].is_array()) {
                                    for (const auto& account : json_data["sponsor_payment_accounts"]) {
                                        if (account.contains("is_active") && account["is_active"].get<bool>()) {
                                            bankInfo = {
                                                {"bankName", account.value("bank_name", "بانک پاسارگاد")},
                                                {"cardNumber", account.value("card_number", "5022-2913-3025-8516")},
                                                {"accountNumber", account.value("account_number", "287.8000.10618503.1")},
                                                {"iban", account.value("shaba_number", "IR750570028780010618503101")},
                                                {"accountHolder", account.value("account_holder_name", "هاتف رستمخانی")},
                                                {"currency", "IRR"}
                                            };
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    } catch (const std::exception& e) {
                        LOG_WARNING("Failed to fetch payment accounts, using fallback: " + std::string(e.what()));
                    }
                    
                    // Fallback to default values if fetching failed
                    if (bankInfo.empty()) {
                        bankInfo = {
                            {"bankName", "بانک پاسارگاد"},
                            {"cardNumber", "5022-2913-3025-8516"},
                            {"accountNumber", "287.8000.10618503.1"},
                            {"iban", "IR750570028780010618503101"},
                            {"accountHolder", "هاتف رستمخانی"},
                            {"currency", "IRR"}
                        };
                    }
                    
                nlohmann::json response = {
                    {"success", true},
                    {"message", savedToDatabase ? "فرم حمایت با موفقیت ارسال و ذخیره شد" : "فرم حمایت دریافت شد"},
                    {"submissionId", actualSubmissionId},
                    {"bankInfo", bankInfo},
                    {"note", "لطفاً پس از واریز مبلغ، رسید پرداخت را به آدرس ایمیل sponsors@hatef.ir ارسال کنید."},
                    {"savedToDatabase", savedToDatabase}
                };

                LOG_INFO("🎉 Sponsor submission completed successfully for: " + fullname + " (ID: " + actualSubmissionId + ")");
                LOG_DEBUG("HomeController::sponsorSubmit - Sending success response with submission ID: " + actualSubmissionId);
                json(res, response);
                LOG_TRACE("HomeController::sponsorSubmit - Success response sent");
                return;
                    
                } catch (const std::exception& e) {
                    LOG_ERROR("Exception in sponsor data logging: " + std::string(e.what()));
                    // Continue to fallback response below
                }
                
                // Fallback response if anything goes wrong - try to fetch payment accounts
                nlohmann::json bankInfo;
                try {
                    std::string url = "https://cdn.hatef.ir/sponsor_payment_accounts.json";
                    
                    CURL* curl = curl_easy_init();
                    if (curl) {
                        std::string response_data;
                        
                        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](void* contents, size_t size, size_t nmemb, std::string* userp) {
                            userp->append((char*)contents, size * nmemb);
                            return size * nmemb;
                        });
                        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
                        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
                        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
                        curl_easy_setopt(curl, CURLOPT_USERAGENT, "SearchEngine/1.0");
                        
                        CURLcode res_code = curl_easy_perform(curl);
                        long http_code = 0;
                        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
                        curl_easy_cleanup(curl);
                        
                        if (res_code == CURLE_OK && http_code == 200) {
                            auto json_data = nlohmann::json::parse(response_data);
                            
                            // Get the first active account
                            if (json_data.contains("sponsor_payment_accounts") && json_data["sponsor_payment_accounts"].is_array()) {
                                for (const auto& account : json_data["sponsor_payment_accounts"]) {
                                    if (account.contains("is_active") && account["is_active"].get<bool>()) {
                                        bankInfo = {
                                            {"bankName", account.value("bank_name", "بانک پاسارگاد")},
                                            {"accountNumber", account.value("card_number", "5022-2913-3025-8516")},
                                            {"iban", account.value("shaba_number", "IR750570028780010618503101")},
                                            {"accountHolder", account.value("account_holder_name", "هاتف رستمخانی")},
                                            {"currency", "IRR"}
                                        };
                                        break;
                                    }
                                }
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    LOG_WARNING("Failed to fetch payment accounts in fallback: " + std::string(e.what()));
                }
                
                // Final fallback to default values if fetching failed
                if (bankInfo.empty()) {
                    bankInfo = {
                        {"bankName", "بانک پاسارگاد"},
                        {"accountNumber", "5022-2913-3025-8516"},
                        {"iban", "IR750570028780010618503101"},
                        {"accountHolder", "هاتف رستمخانی"},
                        {"currency", "IRR"}
                    };
                }
                
                nlohmann::json response = {
                    {"success", true},
                    {"message", "فرم حمایت دریافت شد"},
                    {"submissionId", "fallback-" + std::to_string(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count())},
                    {"bankInfo", bankInfo},
                    {"note", "لطفاً پس از واریز مبلغ، رسید پرداخت را به آدرس ایمیل sponsors@hatef.ir ارسال کنید."}
                };
                
                LOG_INFO("📝 Sponsor submission completed with fallback response for: " + fullname);
                LOG_DEBUG("HomeController::sponsorSubmit - Sending fallback response due to processing issues");
                json(res, response);

            } catch (const std::exception& e) {
                LOG_ERROR("❌ Failed to parse sponsor form data: " + std::string(e.what()));
                badRequest(res, "Invalid JSON format");
            }
        }
    });

    res->onAborted([]() {
        LOG_WARNING("⚠️ Sponsor form submission request aborted by client");
    });
} 

void HomeController::getSponsorPaymentAccounts(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    LOG_INFO("🏦 HomeController::getSponsorPaymentAccounts - Fetching payment account information");
    LOG_DEBUG("HomeController::getSponsorPaymentAccounts called from: " + std::string(req->getHeader("user-agent")).substr(0, 50) + "...");

    try {
        LOG_TRACE("HomeController::getSponsorPaymentAccounts - Preparing to fetch payment accounts");
        // Fetch payment accounts from the JSON file
        std::string url = "https://cdn.hatef.ir/sponsor_payment_accounts.json";
        LOG_DEBUG("HomeController::getSponsorPaymentAccounts - Target URL: " + url);

        LOG_TRACE("HomeController::getSponsorPaymentAccounts - Initializing CURL");
        // Use libcurl to fetch the JSON data
        CURL* curl = curl_easy_init();
        if (!curl) {
            LOG_ERROR("❌ Failed to initialize CURL for fetching payment accounts");
            serverError(res, "Failed to fetch payment accounts");
            return;
        }
        LOG_DEBUG("HomeController::getSponsorPaymentAccounts - CURL initialized successfully");
        
        std::string response_data;
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](void* contents, size_t size, size_t nmemb, std::string* userp) {
            userp->append((char*)contents, size * nmemb);
            return size * nmemb;
        });
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "SearchEngine/1.0");
        
        LOG_TRACE("HomeController::getSponsorPaymentAccounts - Executing CURL request");
        CURLcode res_code = curl_easy_perform(curl);
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        curl_easy_cleanup(curl);

        LOG_DEBUG("HomeController::getSponsorPaymentAccounts - HTTP response code: " + std::to_string(http_code) + ", CURL code: " + std::to_string(res_code));

        if (res_code != CURLE_OK || http_code != 200) {
            LOG_ERROR("❌ Failed to fetch payment accounts from " + url + ". HTTP code: " + std::to_string(http_code) + ", CURL error: " + std::to_string(res_code));
            serverError(res, "Failed to fetch payment accounts");
            return;
        }

        LOG_DEBUG("HomeController::getSponsorPaymentAccounts - Successfully fetched data (" + std::to_string(response_data.size()) + " bytes)");
        LOG_TRACE("HomeController::getSponsorPaymentAccounts - Parsing JSON response");

        // Parse the JSON response
        auto json_data = nlohmann::json::parse(response_data);

        LOG_TRACE("HomeController::getSponsorPaymentAccounts - Extracting active accounts");
        // Extract active accounts only
        std::vector<nlohmann::json> active_accounts;
        if (json_data.contains("sponsor_payment_accounts") && json_data["sponsor_payment_accounts"].is_array()) {
            for (const auto& account : json_data["sponsor_payment_accounts"]) {
                if (account.contains("is_active") && account["is_active"].get<bool>()) {
                    active_accounts.push_back(account);
                }
            }
        }

        LOG_INFO("✅ Payment accounts fetched successfully - " + std::to_string(active_accounts.size()) + " active accounts found");
        LOG_DEBUG("HomeController::getSponsorPaymentAccounts - Preparing response with " + std::to_string(active_accounts.size()) + " accounts");

        // Return the active accounts
        nlohmann::json response = {
            {"success", true},
            {"accounts", active_accounts},
            {"total_accounts", active_accounts.size()},
            {"source_url", url}
        };

        LOG_TRACE("HomeController::getSponsorPaymentAccounts - Sending JSON response");
        json(res, response);
        LOG_TRACE("HomeController::getSponsorPaymentAccounts - Response sent successfully");
        
    } catch (const std::exception& e) {
        LOG_ERROR("💥 Exception in getSponsorPaymentAccounts: " + std::string(e.what()));
        serverError(res, "Failed to process payment accounts");
    }
}

void HomeController::crawlingNotificationPage(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    LOG_INFO("HomeController::crawlingNotificationPage - Serving crawling notification page");
    LOG_DEBUG("HomeController::crawlingNotificationPage called from: " + std::string(req->getHeader("user-agent")).substr(0, 50) + "...");

    try {
        std::string defaultLang = getDefaultLocale();
        crawlingNotificationPageWithLang(res, req, defaultLang);
    } catch (const std::exception& e) {
        LOG_ERROR("HomeController::crawlingNotificationPage - Exception: " + std::string(e.what()));
        serverError(res, "Failed to load crawling notification page");
    }
}

void HomeController::crawlingNotificationPageWithLang(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    LOG_INFO("HomeController::crawlingNotificationPageWithLang - Serving localized crawling notification page");
    
    try {
        // Extract language from URL path
        std::string fullUrl = std::string(req->getUrl());
        std::string lang = "en"; // Default language
        
        // Extract language from path like "/crawling-notification/fa"
        size_t lastSlash = fullUrl.find_last_of('/');
        if (lastSlash != std::string::npos && lastSlash < fullUrl.length() - 1) {
            std::string extractedLang = fullUrl.substr(lastSlash + 1);
            // Validate language code (simple check for now)
            if (extractedLang == "fa" || extractedLang == "en") {
                lang = extractedLang;
            }
        }
        
        crawlingNotificationPageWithLang(res, req, lang);
        
    } catch (const std::exception& e) {
        LOG_ERROR("HomeController::crawlingNotificationPageWithLang - Exception: " + std::string(e.what()));
        serverError(res, "Failed to load localized crawling notification page");
    }
}

void HomeController::crawlingNotificationPageWithLang(uWS::HttpResponse<false>* res, uWS::HttpRequest* req, const std::string& lang) {
    LOG_INFO("HomeController::crawlingNotificationPageWithLang - Serving crawling notification page for language: " + lang);
    LOG_DEBUG("Request from: " + std::string(req->getHeader("user-agent")).substr(0, 50) + "...");

    try {
        // Load localization data using new folder structure
        std::string localesPath = "locales/" + lang + "/crawling-notification.json";
        std::string localeContent = loadFile(localesPath);
        
        if (localeContent.empty()) {
            LOG_WARNING("Failed to load locale file: " + localesPath + ", falling back to English");
            localesPath = "locales/en/crawling-notification.json";
            localeContent = loadFile(localesPath);
        }

        if (localeContent.empty()) {
            LOG_ERROR("Failed to load fallback locale file");
            serverError(res, "Localization data unavailable");
            return;
        }

        nlohmann::json localeData = nlohmann::json::parse(localeContent);
        LOG_DEBUG("Loaded locale data for: " + lang);

        // Prepare template data with sample crawling results
        // In a real implementation, you would get this data from query parameters or database
        nlohmann::json templateData = localeData;
        
        // Sample crawling data - this would typically come from URL parameters or database
        templateData["domainName"] = "example.com";
        templateData["crawledPagesCount"] = 1250;
        templateData["crawlSessionId"] = "session_" + std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        templateData["completionTime"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        templateData["base_url"] = "https://hatef.ir";

        // Override with URL query parameters if provided
        std::string queryString = std::string(req->getQuery());
        if (!queryString.empty()) {
            // Parse query parameters (simple implementation)
            std::istringstream queryStream(queryString);
            std::string param;
            
            while (std::getline(queryStream, param, '&')) {
                size_t equalPos = param.find('=');
                if (equalPos != std::string::npos) {
                    std::string key = param.substr(0, equalPos);
                    std::string value = param.substr(equalPos + 1);
                    
                    // URL decode value (basic implementation)
                    // Replace %20 with space, etc.
                    size_t pos = 0;
                    while ((pos = value.find("%20", pos)) != std::string::npos) {
                        value.replace(pos, 3, " ");
                        pos += 1;
                    }
                    
                    if (key == "domain") {
                        templateData["domainName"] = value;
                    } else if (key == "pages") {
                        try {
                            templateData["crawledPagesCount"] = std::stoi(value);
                        } catch (...) {
                            LOG_WARNING("Invalid pages parameter: " + value);
                        }
                    } else if (key == "session") {
                        templateData["crawlSessionId"] = value;
                    }
                }
            }
        }

        LOG_DEBUG("Template data prepared for domain: " + templateData["domainName"].get<std::string>() + 
                  ", pages: " + std::to_string(templateData["crawledPagesCount"].get<int>()));

        // Render template
        std::string renderedHtml = renderTemplate("crawling-notification.inja", templateData);
        
        if (renderedHtml.empty()) {
            LOG_ERROR("Failed to render crawling notification template");
            serverError(res, "Template rendering failed");
            return;
        }

        LOG_INFO("Successfully rendered crawling notification page for language: " + lang);
        html(res, renderedHtml);
        LOG_TRACE("Crawling notification page response sent successfully");
        
    } catch (const nlohmann::json::parse_error& e) {
        LOG_ERROR("HomeController::crawlingNotificationPageWithLang - JSON parse error: " + std::string(e.what()));
        serverError(res, "Localization data parse error");
    } catch (const std::exception& e) {
        LOG_ERROR("HomeController::crawlingNotificationPageWithLang - Exception: " + std::string(e.what()));
        serverError(res, "Failed to load crawling notification page");
    }
} 