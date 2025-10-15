#include "WebsiteProfileController.h"
#include "../../include/Logger.h"
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <curl/curl.h>

WebsiteProfileController::WebsiteProfileController() {
    // Empty constructor - use lazy initialization pattern
    LOG_DEBUG("WebsiteProfileController created (lazy initialization)");
}

search_engine::storage::WebsiteProfileStorage* WebsiteProfileController::getStorage() const {
    if (!storage_) {
        try {
            LOG_INFO("Lazy initializing WebsiteProfileStorage");
            storage_ = std::make_unique<search_engine::storage::WebsiteProfileStorage>();
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to lazy initialize WebsiteProfileStorage: " + std::string(e.what()));
            throw;
        }
    }
    return storage_.get();
}

search_engine::storage::WebsiteProfile WebsiteProfileController::parseProfileFromJson(const nlohmann::json& json) {
    search_engine::storage::WebsiteProfile profile;
    
    if (json.contains("business_name") && json["business_name"].is_string()) {
        profile.business_name = json["business_name"].get<std::string>();
    }
    
    if (json.contains("website_url") && json["website_url"].is_string()) {
        profile.website_url = json["website_url"].get<std::string>();
    }
    
    if (json.contains("owner_name") && json["owner_name"].is_string()) {
        profile.owner_name = json["owner_name"].get<std::string>();
    }
    
    // Parse grant_date
    if (json.contains("grant_date") && json["grant_date"].is_object()) {
        if (json["grant_date"].contains("persian")) {
            profile.grant_date.persian = json["grant_date"]["persian"].get<std::string>();
        }
        if (json["grant_date"].contains("gregorian")) {
            profile.grant_date.gregorian = json["grant_date"]["gregorian"].get<std::string>();
        }
    }
    
    // Parse expiry_date
    if (json.contains("expiry_date") && json["expiry_date"].is_object()) {
        if (json["expiry_date"].contains("persian")) {
            profile.expiry_date.persian = json["expiry_date"]["persian"].get<std::string>();
        }
        if (json["expiry_date"].contains("gregorian")) {
            profile.expiry_date.gregorian = json["expiry_date"]["gregorian"].get<std::string>();
        }
    }
    
    if (json.contains("address") && json["address"].is_string()) {
        profile.address = json["address"].get<std::string>();
    }
    
    if (json.contains("phone") && json["phone"].is_string()) {
        profile.phone = json["phone"].get<std::string>();
    }
    
    if (json.contains("email") && json["email"].is_string()) {
        profile.email = json["email"].get<std::string>();
    }
    
    // Parse location
    if (json.contains("location") && json["location"].is_object()) {
        if (json["location"].contains("latitude")) {
            profile.location.latitude = json["location"]["latitude"].get<double>();
        }
        if (json["location"].contains("longitude")) {
            profile.location.longitude = json["location"]["longitude"].get<double>();
        }
    }
    
    if (json.contains("business_experience") && json["business_experience"].is_string()) {
        profile.business_experience = json["business_experience"].get<std::string>();
    }
    
    if (json.contains("business_hours") && json["business_hours"].is_string()) {
        profile.business_hours = json["business_hours"].get<std::string>();
    }
    
    // Parse business_services array
    if (json.contains("business_services") && json["business_services"].is_array()) {
        for (const auto& service_json : json["business_services"]) {
            search_engine::storage::BusinessService service;
            
            if (service_json.contains("row_number")) {
                service.row_number = service_json["row_number"].get<std::string>();
            }
            if (service_json.contains("service_title")) {
                service.service_title = service_json["service_title"].get<std::string>();
            }
            if (service_json.contains("permit_issuer")) {
                service.permit_issuer = service_json["permit_issuer"].get<std::string>();
            }
            if (service_json.contains("permit_number")) {
                service.permit_number = service_json["permit_number"].get<std::string>();
            }
            if (service_json.contains("validity_start_date")) {
                service.validity_start_date = service_json["validity_start_date"].get<std::string>();
            }
            if (service_json.contains("validity_end_date")) {
                service.validity_end_date = service_json["validity_end_date"].get<std::string>();
            }
            if (service_json.contains("status")) {
                service.status = service_json["status"].get<std::string>();
            }
            
            profile.business_services.push_back(service);
        }
    }
    
    if (json.contains("extraction_timestamp") && json["extraction_timestamp"].is_string()) {
        profile.extraction_timestamp = json["extraction_timestamp"].get<std::string>();
    }
    
    // Parse domain_info
    if (json.contains("domain_info") && json["domain_info"].is_object()) {
        if (json["domain_info"].contains("page_number")) {
            profile.domain_info.page_number = json["domain_info"]["page_number"].get<int>();
        }
        if (json["domain_info"].contains("row_index")) {
            profile.domain_info.row_index = json["domain_info"]["row_index"].get<int>();
        }
        if (json["domain_info"].contains("row_number")) {
            profile.domain_info.row_number = json["domain_info"]["row_number"].get<std::string>();
        }
        if (json["domain_info"].contains("province")) {
            profile.domain_info.province = json["domain_info"]["province"].get<std::string>();
        }
        if (json["domain_info"].contains("city")) {
            profile.domain_info.city = json["domain_info"]["city"].get<std::string>();
        }
        if (json["domain_info"].contains("domain_url")) {
            profile.domain_info.domain_url = json["domain_info"]["domain_url"].get<std::string>();
        }
    }
    
    return profile;
}

void WebsiteProfileController::saveProfile(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    std::string buffer;
    
    res->onData([this, res, buffer = std::move(buffer)](std::string_view data, bool last) mutable {
        buffer.append(data.data(), data.length());
        
        if (last) {
            try {
                // Parse JSON body
                auto jsonBody = nlohmann::json::parse(buffer);
                
                // Validate required fields
                if (!jsonBody.contains("website_url") || jsonBody["website_url"].get<std::string>().empty()) {
                    badRequest(res, "Missing required field: website_url");
                    return;
                }
                
                // Parse profile from JSON
                auto profile = parseProfileFromJson(jsonBody);
                
                // Save to database
                auto result = getStorage()->saveProfile(profile);
                
                if (result.success) {
                    nlohmann::json response = {
                        {"success", true},
                        {"message", result.message},
                        {"data", {
                            {"website_url", result.value}
                        }}
                    };
                    json(res, response);
                    LOG_INFO("Website profile saved: " + profile.website_url);
                    
                    // Trigger crawl for the website (async, non-blocking)
                    triggerCrawlForWebsite(profile.website_url, profile.email, profile.owner_name);
                } else {
                    // Check if it's a duplicate error
                    if (result.message.find("already exists") != std::string::npos) {
                        badRequest(res, result.message);
                    } else {
                        serverError(res, result.message);
                    }
                }
                
            } catch (const nlohmann::json::parse_error& e) {
                LOG_ERROR("JSON parse error in saveProfile: " + std::string(e.what()));
                badRequest(res, "Invalid JSON format");
            } catch (const std::exception& e) {
                LOG_ERROR("Error in saveProfile: " + std::string(e.what()));
                serverError(res, "Internal server error");
            }
        }
    });
    
    res->onAborted([]() {
        LOG_WARNING("Client disconnected during saveProfile request");
    });
}

void WebsiteProfileController::getProfile(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    try {
        std::string url = std::string(req->getParameter(0));
        
        if (url.empty()) {
            badRequest(res, "Missing website URL parameter");
            return;
        }
        
        auto result = getStorage()->getProfileByUrl(url);
        
        if (result.success) {
            auto& profile = result.value;
            
            nlohmann::json services_json = nlohmann::json::array();
            for (const auto& service : profile.business_services) {
                services_json.push_back({
                    {"row_number", service.row_number},
                    {"service_title", service.service_title},
                    {"permit_issuer", service.permit_issuer},
                    {"permit_number", service.permit_number},
                    {"validity_start_date", service.validity_start_date},
                    {"validity_end_date", service.validity_end_date},
                    {"status", service.status}
                });
            }
            
            nlohmann::json response = {
                {"success", true},
                {"message", result.message},
                {"data", {
                    {"business_name", profile.business_name},
                    {"website_url", profile.website_url},
                    {"owner_name", profile.owner_name},
                    {"grant_date", {
                        {"persian", profile.grant_date.persian},
                        {"gregorian", profile.grant_date.gregorian}
                    }},
                    {"expiry_date", {
                        {"persian", profile.expiry_date.persian},
                        {"gregorian", profile.expiry_date.gregorian}
                    }},
                    {"address", profile.address},
                    {"phone", profile.phone},
                    {"email", profile.email},
                    {"location", {
                        {"latitude", profile.location.latitude},
                        {"longitude", profile.location.longitude}
                    }},
                    {"business_experience", profile.business_experience},
                    {"business_hours", profile.business_hours},
                    {"business_services", services_json},
                    {"extraction_timestamp", profile.extraction_timestamp},
                    {"domain_info", {
                        {"page_number", profile.domain_info.page_number},
                        {"row_index", profile.domain_info.row_index},
                        {"row_number", profile.domain_info.row_number},
                        {"province", profile.domain_info.province},
                        {"city", profile.domain_info.city},
                        {"domain_url", profile.domain_info.domain_url}
                    }},
                    {"created_at", profile.created_at}
                }}
            };
            
            json(res, response);
        } else {
            notFound(res, result.message);
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error in getProfile: " + std::string(e.what()));
        serverError(res, "Internal server error");
    }
}

void WebsiteProfileController::getAllProfiles(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    try {
        // Parse query parameters for pagination
        std::string query = std::string(req->getQuery());
        int limit = 100;
        int skip = 0;
        
        // Simple query parsing
        size_t limit_pos = query.find("limit=");
        if (limit_pos != std::string::npos) {
            size_t end_pos = query.find("&", limit_pos);
            std::string limit_str = query.substr(limit_pos + 6, end_pos - limit_pos - 6);
            try {
                limit = std::stoi(limit_str);
            } catch (...) {}
        }
        
        size_t skip_pos = query.find("skip=");
        if (skip_pos != std::string::npos) {
            size_t end_pos = query.find("&", skip_pos);
            std::string skip_str = query.substr(skip_pos + 5, end_pos == std::string::npos ? std::string::npos : end_pos - skip_pos - 5);
            try {
                skip = std::stoi(skip_str);
            } catch (...) {}
        }
        
        auto result = getStorage()->getAllProfiles(limit, skip);
        
        if (result.success) {
            nlohmann::json profiles_json = nlohmann::json::array();
            
            for (const auto& profile : result.value) {
                nlohmann::json services_json = nlohmann::json::array();
                for (const auto& service : profile.business_services) {
                    services_json.push_back({
                        {"row_number", service.row_number},
                        {"service_title", service.service_title},
                        {"permit_issuer", service.permit_issuer},
                        {"permit_number", service.permit_number},
                        {"validity_start_date", service.validity_start_date},
                        {"validity_end_date", service.validity_end_date},
                        {"status", service.status}
                    });
                }
                
                profiles_json.push_back({
                    {"business_name", profile.business_name},
                    {"website_url", profile.website_url},
                    {"owner_name", profile.owner_name},
                    {"grant_date", {
                        {"persian", profile.grant_date.persian},
                        {"gregorian", profile.grant_date.gregorian}
                    }},
                    {"expiry_date", {
                        {"persian", profile.expiry_date.persian},
                        {"gregorian", profile.expiry_date.gregorian}
                    }},
                    {"address", profile.address},
                    {"phone", profile.phone},
                    {"email", profile.email},
                    {"location", {
                        {"latitude", profile.location.latitude},
                        {"longitude", profile.location.longitude}
                    }},
                    {"business_experience", profile.business_experience},
                    {"business_hours", profile.business_hours},
                    {"business_services", services_json},
                    {"extraction_timestamp", profile.extraction_timestamp},
                    {"domain_info", {
                        {"page_number", profile.domain_info.page_number},
                        {"row_index", profile.domain_info.row_index},
                        {"row_number", profile.domain_info.row_number},
                        {"province", profile.domain_info.province},
                        {"city", profile.domain_info.city},
                        {"domain_url", profile.domain_info.domain_url}
                    }},
                    {"created_at", profile.created_at}
                });
            }
            
            nlohmann::json response = {
                {"success", true},
                {"message", result.message},
                {"data", {
                    {"profiles", profiles_json},
                    {"count", profiles_json.size()},
                    {"limit", limit},
                    {"skip", skip}
                }}
            };
            
            json(res, response);
        } else {
            serverError(res, result.message);
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error in getAllProfiles: " + std::string(e.what()));
        serverError(res, "Internal server error");
    }
}

void WebsiteProfileController::updateProfile(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    std::string url = std::string(req->getParameter(0));
    std::string buffer;
    
    res->onData([this, res, url, buffer = std::move(buffer)](std::string_view data, bool last) mutable {
        buffer.append(data.data(), data.length());
        
        if (last) {
            try {
                if (url.empty()) {
                    badRequest(res, "Missing website URL parameter");
                    return;
                }
                
                // Parse JSON body
                auto jsonBody = nlohmann::json::parse(buffer);
                
                // Parse profile from JSON
                auto profile = parseProfileFromJson(jsonBody);
                
                // Update in database
                auto result = getStorage()->updateProfile(url, profile);
                
                if (result.success) {
                    nlohmann::json response = {
                        {"success", true},
                        {"message", result.message}
                    };
                    json(res, response);
                    LOG_INFO("Website profile updated: " + url);
                } else {
                    notFound(res, result.message);
                }
                
            } catch (const nlohmann::json::parse_error& e) {
                LOG_ERROR("JSON parse error in updateProfile: " + std::string(e.what()));
                badRequest(res, "Invalid JSON format");
            } catch (const std::exception& e) {
                LOG_ERROR("Error in updateProfile: " + std::string(e.what()));
                serverError(res, "Internal server error");
            }
        }
    });
    
    res->onAborted([]() {
        LOG_WARNING("Client disconnected during updateProfile request");
    });
}

void WebsiteProfileController::deleteProfile(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    try {
        std::string url = std::string(req->getParameter(0));
        
        if (url.empty()) {
            badRequest(res, "Missing website URL parameter");
            return;
        }
        
        auto result = getStorage()->deleteProfile(url);
        
        if (result.success) {
            nlohmann::json response = {
                {"success", true},
                {"message", result.message}
            };
            json(res, response);
            LOG_INFO("Website profile deleted: " + url);
        } else {
            notFound(res, result.message);
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error in deleteProfile: " + std::string(e.what()));
        serverError(res, "Internal server error");
    }
}

void WebsiteProfileController::checkProfile(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    try {
        std::string url = std::string(req->getParameter(0));
        
        if (url.empty()) {
            badRequest(res, "Missing website URL parameter");
            return;
        }
        
        auto result = getStorage()->profileExists(url);
        
        if (result.success) {
            nlohmann::json response = {
                {"success", true},
                {"message", result.message},
                {"data", {
                    {"website_url", url},
                    {"exists", result.value}
                }}
            };
            json(res, response);
        } else {
            serverError(res, result.message);
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error in checkProfile: " + std::string(e.what()));
        serverError(res, "Internal server error");
    }
}

// Callback function for libcurl to write response data
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

void WebsiteProfileController::triggerCrawlForWebsite(const std::string& websiteUrl, const std::string& email, const std::string& ownerName) {
    // Run async to not block the main response
    std::thread([websiteUrl, email, ownerName]() {
        try {
            LOG_INFO("Triggering crawl for website: " + websiteUrl);
            
            // Prepare the JSON payload for /api/crawl/add-site
            nlohmann::json payload = {
                {"url", "https://" + websiteUrl},  // Add https:// prefix
                {"maxPages", 5},
                {"maxDepth", 5},
            };
            
            // Add email if provided
            if (!email.empty()) {
                payload["email"] = email;
                payload["recipientName"] = ownerName;
                payload["language"] = "fa";  // Default to Persian for e-namad websites
            }
            
            std::string jsonPayload = payload.dump();
            std::string responseBuffer;
            
            // Initialize CURL
            CURL* curl = curl_easy_init();
            if (!curl) {
                LOG_ERROR("Failed to initialize CURL for crawl trigger");
                return;
            }
            
            // Get base URL from environment variable
            const char* baseUrlEnv = std::getenv("BASE_URL");
            std::string baseUrl = baseUrlEnv ? baseUrlEnv : "http://localhost:3000";
            
            // Set up the request using base URL from environment
            std::string url = baseUrl + "/api/crawl/add-site";
            LOG_DEBUG("Crawl API endpoint: " + url);
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonPayload.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(jsonPayload.size()));
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBuffer);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);  // 10 seconds timeout
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);  // 5 seconds connection timeout
            
            // Set headers
            struct curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, "Content-Type: application/json");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            
            // Perform the request
            CURLcode res = curl_easy_perform(curl);
            
            if (res != CURLE_OK) {
                LOG_ERROR("CURL error when triggering crawl: " + std::string(curl_easy_strerror(res)));
            } else {
                // Check HTTP response code
                long responseCode;
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
                
                if (responseCode >= 200 && responseCode < 300) {
                    LOG_INFO("Successfully triggered crawl for " + websiteUrl + " (HTTP " + std::to_string(responseCode) + ")");
                    LOG_DEBUG("Crawl API response: " + responseBuffer);
                } else {
                    LOG_WARNING("Crawl trigger returned HTTP " + std::to_string(responseCode) + " for " + websiteUrl);
                    LOG_DEBUG("Response body: " + responseBuffer);
                }
            }
            
            // Cleanup
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            
        } catch (const std::exception& e) {
            LOG_ERROR("Exception in triggerCrawlForWebsite: " + std::string(e.what()));
        }
    }).detach();  // Detach the thread to avoid blocking
}

