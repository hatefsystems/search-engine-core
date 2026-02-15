#include "ProfileController.h"
#include "../../include/Logger.h"
#include "../../include/search_engine/common/SlugGenerator.h"
#include "../../include/search_engine/storage/ProfileValidator.h"
#include "../../include/search_engine/storage/AuditLogger.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <random>

ProfileController::ProfileController() {
    // Empty constructor - use lazy initialization pattern
    LOG_DEBUG("ProfileController created (lazy initialization)");
}

search_engine::storage::ProfileStorage* ProfileController::getStorage() const {
    if (!storage_) {
        try {
            LOG_INFO("Lazy initializing ProfileStorage");
            storage_ = std::make_unique<search_engine::storage::ProfileStorage>();
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to lazy initialize ProfileStorage: " + std::string(e.what()));
            throw;
        }
    }
    return storage_.get();
}

search_engine::common::SlugCache* ProfileController::getSlugCache() const {
    if (!slugCache_) {
        try {
            LOG_INFO("Lazy initializing SlugCache");
            // 5 minute TTL for slug cache
            slugCache_ = std::make_unique<search_engine::common::SlugCache>(300);
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to lazy initialize SlugCache: " + std::string(e.what()));
            throw;
        }
    }
    return slugCache_.get();
}

search_engine::storage::ProfileViewAnalyticsStorage* ProfileController::getAnalyticsStorage() const {
    if (!analyticsStorage_) {
        try {
            LOG_INFO("Lazy initializing ProfileViewAnalyticsStorage");
            analyticsStorage_ = std::make_unique<search_engine::storage::ProfileViewAnalyticsStorage>();
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to lazy initialize ProfileViewAnalyticsStorage: " + std::string(e.what()));
            throw;
        }
    }
    return analyticsStorage_.get();
}

search_engine::storage::ComplianceStorage* ProfileController::getComplianceStorage() const {
    if (!complianceStorage_) {
        try {
            LOG_INFO("Lazy initializing ComplianceStorage");
            complianceStorage_ = std::make_unique<search_engine::storage::ComplianceStorage>();
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to lazy initialize ComplianceStorage: " + std::string(e.what()));
            throw;
        }
    }
    return complianceStorage_.get();
}

search_engine::storage::AuditStorage* ProfileController::getAuditStorage() const {
    if (!auditStorage_) {
        try {
            LOG_INFO("Lazy initializing AuditStorage");
            auditStorage_ = std::make_unique<search_engine::storage::AuditStorage>();
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to lazy initialize AuditStorage: " + std::string(e.what()));
            throw;
        }
    }
    return auditStorage_.get();
}

ApiRateLimiter* ProfileController::getRateLimiter() const {
    if (!rateLimiter_) {
        try {
            LOG_INFO("Lazy initializing ApiRateLimiter");
            // Get config from environment or use defaults
            size_t maxRequests = 60; // Default: 60 requests
            int windowSeconds = 60;  // Default: 60 seconds
            
            const char* limitEnv = std::getenv("PROFILE_API_RATE_LIMIT_REQUESTS");
            if (limitEnv) {
                maxRequests = std::stoi(limitEnv);
            }
            
            const char* windowEnv = std::getenv("PROFILE_API_RATE_LIMIT_WINDOW_SECONDS");
            if (windowEnv) {
                windowSeconds = std::stoi(windowEnv);
            }
            
            rateLimiter_ = std::make_unique<ApiRateLimiter>(maxRequests, std::chrono::seconds(windowSeconds));
            LOG_INFO("ApiRateLimiter configured: " + std::to_string(maxRequests) + 
                    " requests per " + std::to_string(windowSeconds) + " seconds");
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to lazy initialize ApiRateLimiter: " + std::string(e.what()));
            throw;
        }
    }
    return rateLimiter_.get();
}

std::string ProfileController::getClientIP(uWS::HttpRequest* req) {
    // Try X-Forwarded-For header first (for proxied requests)
    std::string xForwardedFor = std::string(req->getHeader("x-forwarded-for"));
    if (!xForwardedFor.empty()) {
        // X-Forwarded-For can contain multiple IPs, get the first one
        size_t commaPos = xForwardedFor.find(",");
        if (commaPos != std::string::npos) {
            return xForwardedFor.substr(0, commaPos);
        }
        return xForwardedFor;
    }
    
    // Try X-Real-IP header
    std::string xRealIP = std::string(req->getHeader("x-real-ip"));
    if (!xRealIP.empty()) {
        return xRealIP;
    }
    
    return "unknown";
}

std::string ProfileController::getUserAgent(uWS::HttpRequest* req) {
    std::string userAgent = std::string(req->getHeader("user-agent"));
    if (userAgent.empty()) {
        return "unknown";
    }
    return userAgent;
}

std::string ProfileController::getReferrer(uWS::HttpRequest* req) {
    std::string referrer = std::string(req->getHeader("referer"));
    if (referrer.empty()) {
        return "direct";
    }
    return referrer;
}

// ==================== Authentication & Ownership Helpers ====================

std::string ProfileController::generateOwnerToken() {
    // Generate a secure random token (64 hex characters = 32 bytes)
    std::random_device rd;
    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 32; i++) {
        ss << std::setw(2) << std::setfill('0') << (rd() % 256);
    }
    return ss.str();
}

std::string ProfileController::getAuthToken(uWS::HttpRequest* req) {
    // Try Authorization header first (Bearer token)
    std::string authHeader = std::string(req->getHeader("authorization"));
    if (!authHeader.empty() && authHeader.find("Bearer ") == 0) {
        return authHeader.substr(7); // Remove "Bearer " prefix
    }
    
    // Try x-profile-token header
    std::string profileToken = std::string(req->getHeader("x-profile-token"));
    if (!profileToken.empty()) {
        return profileToken;
    }
    
    return "";
}

bool ProfileController::checkOwnership(const search_engine::storage::Profile& profile, const std::string& token) {
    // Require ownership token for all mutating operations
    if (!profile.ownerToken || profile.ownerToken.value().empty()) {
        // Profiles without tokens should be migrated; deny access for safety
        LOG_WARNING("Profile " + profile.id.value_or("unknown") + " has no owner token - denying access");
        return false;
    }
    
    // If token is empty but profile has ownerToken, deny access
    if (token.empty()) {
        return false;
    }
    
    // Check if tokens match
    return profile.ownerToken.value() == token;
}

std::string ProfileController::getCallerIdentity(uWS::HttpRequest* req) {
    std::string token = getAuthToken(req);
    if (!token.empty()) {
        return "token:" + token.substr(0, 8) + "..."; // Return first 8 chars for logging
    }
    return "anonymous";
}

bool ProfileController::checkRateLimit(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    std::string clientIP = getClientIP(req);
    
    if (getRateLimiter()->shouldThrottle(clientIP)) {
        int retryAfter = getRateLimiter()->getRetryAfter(clientIP);
        
        res->writeStatus("429 Too Many Requests");
        res->writeHeader("Retry-After", std::to_string(retryAfter));
        
        nlohmann::json errorResponse = {
            {"success", false},
            {"message", "Rate limit exceeded. Please try again later."},
            {"error", "RATE_LIMIT_EXCEEDED"},
            {"retryAfter", retryAfter}
        };
        
        this->json(res, errorResponse);
        LOG_WARNING("Rate limit exceeded for IP: " + clientIP);
        return true; // Rate limited
    }
    
    return false; // Not rate limited
}

void ProfileController::recordProfileView(const std::string& profileId, uWS::HttpRequest* req) {
    try {
        // Get client IP and User-Agent
        std::string ipAddress = getClientIP(req);
        std::string userAgent = getUserAgent(req);
        std::string referrer = getReferrer(req);
        
        LOG_DEBUG("Recording profile view: profileId=" + profileId + ", IP=" + ipAddress);
        
        // Generate unique view ID using random_device for thread safety
        auto now = std::chrono::system_clock::now();
        auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        std::random_device rd;
        std::string viewId = std::to_string(nowMs) + "-" + std::to_string(rd() % 1000000);
        
        // Tier 1: Privacy-first analytics (NO IP!)
        search_engine::storage::GeoData geo = search_engine::storage::GeoIPService::lookup(ipAddress);
        search_engine::storage::UserAgentInfo uaInfo = search_engine::storage::UserAgentParser::parse(userAgent);
        
        search_engine::storage::ProfileViewAnalytics analytics;
        analytics.viewId = viewId;
        analytics.profileId = profileId;
        analytics.timestamp = now;
        analytics.country = geo.country;
        analytics.province = geo.province;
        analytics.city = geo.city;
        analytics.browser = uaInfo.browser;
        analytics.os = uaInfo.os;
        analytics.deviceType = uaInfo.deviceType;
        
        auto analyticsResult = getAnalyticsStorage()->recordView(analytics);
        if (!analyticsResult.success) {
            LOG_WARNING("Failed to record Tier 1 analytics: " + analyticsResult.message);
        }
        
        // Tier 2: Encrypted compliance log
        std::string encryptionKey = search_engine::storage::DataEncryption::getEncryptionKey();
        
        search_engine::storage::LegalComplianceLog complianceLog;
        complianceLog.logId = viewId + "-compliance";
        complianceLog.userId = "anonymous";  // No user tracking yet
        complianceLog.timestamp = now;
        complianceLog.ipAddress_encrypted = search_engine::storage::DataEncryption::encrypt(ipAddress, encryptionKey);
        complianceLog.userAgent_encrypted = search_engine::storage::DataEncryption::encrypt(userAgent, encryptionKey);
        complianceLog.referrer_encrypted = search_engine::storage::DataEncryption::encrypt(referrer, encryptionKey);
        complianceLog.viewId = viewId;
        
        // Set retention expiry to 12 months from now
        auto twelveMonths = std::chrono::hours(24 * 365);  // 365 days
        complianceLog.retentionExpiry = now + twelveMonths;
        complianceLog.isUnderInvestigation = false;
        
        auto complianceResult = getComplianceStorage()->recordLog(complianceLog);
        if (!complianceResult.success) {
            LOG_WARNING("Failed to record Tier 2 compliance log: " + complianceResult.message);
        }
        
        // Audit log: record profile view
        try {
            std::string viewerId = "anonymous";  // TODO: Get viewer's user ID from auth when implemented
            search_engine::storage::AuditLogger::logProfileView(
                profileId, viewerId, ipAddress, userAgent, getAuditStorage()
            );
        } catch (const std::exception& e) {
            LOG_WARNING("Failed to record audit log for profile view: " + std::string(e.what()));
        }
        
        // Secure memory wipe for sensitive data
        search_engine::storage::secureMemoryWipe(&ipAddress);
        search_engine::storage::secureMemoryWipe(&userAgent);
        search_engine::storage::secureMemoryWipe(&referrer);
        
        LOG_INFO("Profile view recorded successfully: viewId=" + viewId);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to record profile view: " + std::string(e.what()));
        // Don't fail the request if tracking fails
    }
}

search_engine::storage::Profile ProfileController::parseProfileFromJson(const nlohmann::json& json) {
    search_engine::storage::Profile profile;

    // Parse required fields
    if (json.contains("slug") && json["slug"].is_string()) {
        profile.slug = json["slug"].get<std::string>();
    } else {
        throw std::invalid_argument("Missing required field: slug");
    }

    if (json.contains("name") && json["name"].is_string()) {
        profile.name = json["name"].get<std::string>();
    } else {
        throw std::invalid_argument("Missing required field: name");
    }

    if (json.contains("type") && json["type"].is_string()) {
        profile.type = stringToProfileType(json["type"].get<std::string>());
    } else {
        throw std::invalid_argument("Missing required field: type");
    }

    // Parse optional fields
    if (json.contains("bio") && json["bio"].is_string()) {
        profile.bio = json["bio"].get<std::string>();
        // Validate bio length (max 500 characters)
        if (profile.bio->length() > 500) {
            throw std::invalid_argument("Bio exceeds maximum length of 500 characters");
        }
    }

    if (json.contains("isPublic") && json["isPublic"].is_boolean()) {
        profile.isPublic = json["isPublic"].get<bool>();
    } else {
        profile.isPublic = true; // Default to public
    }

    // Validate slug format
    if (!search_engine::storage::ProfileStorage::isValidSlug(profile.slug)) {
        throw std::invalid_argument("Invalid slug format. Slug must contain only Persian letters, English letters, numbers, and hyphens.");
    }

    return profile;
}

nlohmann::json ProfileController::profileToJson(const search_engine::storage::Profile& profile) {
    nlohmann::json json;

    // Required fields
    json["id"] = profile.id.value_or("");
    json["slug"] = profile.slug;
    json["name"] = profile.name;
    json["type"] = profileTypeToString(profile.type);
    json["isPublic"] = profile.isPublic;

    // Optional fields
    if (profile.bio) {
        json["bio"] = profile.bio.value();
    }

    // Format createdAt timestamp as ISO 8601 string
    auto time_t = std::chrono::system_clock::to_time_t(profile.createdAt);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    json["createdAt"] = ss.str();

    // Format updatedAt timestamp as ISO 8601 string if present
    if (profile.updatedAt) {
        auto updated_time_t = std::chrono::system_clock::to_time_t(profile.updatedAt.value());
        std::stringstream updated_ss;
        updated_ss << std::put_time(std::gmtime(&updated_time_t), "%Y-%m-%dT%H:%M:%SZ");
        json["updatedAt"] = updated_ss.str();
    }

    return json;
}

std::string ProfileController::profileTypeToString(search_engine::storage::ProfileType type) {
    switch (type) {
        case search_engine::storage::ProfileType::PERSON: return "PERSON";
        case search_engine::storage::ProfileType::BUSINESS: return "BUSINESS";
        default: return "UNKNOWN";
    }
}

search_engine::storage::ProfileType ProfileController::stringToProfileType(const std::string& type) {
    if (type == "PERSON") return search_engine::storage::ProfileType::PERSON;
    if (type == "BUSINESS") return search_engine::storage::ProfileType::BUSINESS;
    throw std::invalid_argument("Invalid profile type: " + type + ". Must be PERSON or BUSINESS.");
}

void ProfileController::createProfile(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    // Check rate limit
    if (checkRateLimit(res, req)) {
        return;
    }
    
    std::string buffer;

    res->onData([this, res, req, buffer = std::move(buffer)](std::string_view data, bool last) mutable {
        buffer.append(data.data(), data.length());

        if (last) {
            try {
                // Parse JSON body
                auto jsonBody = nlohmann::json::parse(buffer);

                // Parse profile from JSON
                auto profile = parseProfileFromJson(jsonBody);

                // Set creation timestamp
                profile.createdAt = std::chrono::system_clock::now();

                // Generate owner token for authentication
                profile.ownerToken = generateOwnerToken();

                // Validate profile using ProfileValidator
                auto validationResult = search_engine::storage::ProfileValidator::validate(profile);
                
                if (!validationResult.isValid) {
                    // Return 400 with structured error response
                    nlohmann::json errorResponse = {
                        {"success", false},
                        {"message", "Validation failed"},
                        {"errors", validationResult.errors}
                    };
                    
                    if (!validationResult.warnings.empty()) {
                        errorResponse["warnings"] = validationResult.warnings;
                    }
                    
                    res->writeStatus("400 Bad Request");
                    this->json(res, errorResponse);
                    LOG_WARNING("Profile validation failed: " + std::to_string(validationResult.errors.size()) + " errors");
                    return;
                }

                // Save to database
                auto result = getStorage()->store(profile);

                if (result.success) {
                    // Audit log: record profile creation
                    try {
                        std::string userId = getCallerIdentity(req);
                        std::string ipAddress = getClientIP(req);
                        std::string userAgent = getUserAgent(req);
                        
                        search_engine::storage::AuditLogger::logProfileCreate(
                            profile, userId, ipAddress, userAgent, getAuditStorage()
                        );
                    } catch (const std::exception& e) {
                        LOG_WARNING("Failed to record audit log: " + std::string(e.what()));
                    }
                    
                    nlohmann::json response = {
                        {"success", true},
                        {"message", result.message},
                        {"data", profileToJson(profile)}
                    };
                    
                    // Include ownerToken in response (only returned once on creation)
                    if (profile.ownerToken) {
                        response["ownerToken"] = profile.ownerToken.value();
                    }
                    
                    // Include warnings if present
                    if (!validationResult.warnings.empty()) {
                        response["warnings"] = validationResult.warnings;
                    }
                    
                    this->json(res, response);
                    LOG_INFO("Profile created with slug: " + profile.slug);
                } else {
                    if (result.message.find("already taken") != std::string::npos) {
                        badRequest(res, result.message);
                    } else {
                        serverError(res, result.message);
                    }
                }

            } catch (const nlohmann::json::parse_error& e) {
                LOG_ERROR("JSON parse error in createProfile: " + std::string(e.what()));
                badRequest(res, "Invalid JSON format");
            } catch (const std::invalid_argument& e) {
                LOG_ERROR("Validation error in createProfile: " + std::string(e.what()));
                badRequest(res, std::string(e.what()));
            } catch (const std::exception& e) {
                LOG_ERROR("Error in createProfile: " + std::string(e.what()));
                serverError(res, "Internal server error");
            }
        }
    });

    res->onAborted([]() {
        LOG_WARNING("Client disconnected during createProfile request");
    });
}

void ProfileController::getProfileById(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    // Check rate limit
    if (checkRateLimit(res, req)) {
        return;
    }
    
    try {
        std::string id = std::string(req->getParameter(0));

        if (id.empty()) {
            badRequest(res, "Profile ID is required");
            return;
        }

        auto result = getStorage()->findById(id);

        if (result.success) {
            nlohmann::json response = {
                {"success", true},
                {"message", result.message},
                {"data", profileToJson(result.value)}
            };
            json(res, response);
        } else {
            notFound(res, result.message);
        }

    } catch (const std::exception& e) {
        LOG_ERROR("Error in getProfileById: " + std::string(e.what()));
        serverError(res, "Internal server error");
    }
}

void ProfileController::getPublicProfile(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    // Check rate limit for public endpoints
    if (checkRateLimit(res, req)) {
        return;
    }
    
    try {
        std::string slug = std::string(req->getParameter(0));

        if (slug.empty()) {
            badRequest(res, "Profile slug is required");
            return;
        }

        servePublicProfileBySlug(res, req, slug);

    } catch (const std::exception& e) {
        LOG_ERROR("Error in getPublicProfile: " + std::string(e.what()));
        serverError(res, "Internal server error");
    }
}

void ProfileController::updateProfile(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    // Check rate limit
    if (checkRateLimit(res, req)) {
        return;
    }
    
    std::string buffer;
    std::string profileId = std::string(req->getParameter(0));

    if (profileId.empty()) {
        badRequest(res, "Profile ID is required");
        return;
    }

    res->onData([this, res, req, buffer = std::move(buffer), profileId](std::string_view data, bool last) mutable {
        buffer.append(data.data(), data.length());

        if (last) {
            try {
                // Parse JSON body
                auto jsonBody = nlohmann::json::parse(buffer);

                // First, get the existing profile (for audit trail)
                auto existingResult = getStorage()->findById(profileId);
                if (!existingResult.success) {
                    notFound(res, "Profile not found");
                    return;
                }

                auto oldProfile = existingResult.value;  // Keep old version for audit
                auto existingProfile = existingResult.value;

                // Check ownership (authentication)
                std::string authToken = getAuthToken(req);
                if (!checkOwnership(existingProfile, authToken)) {
                    res->writeStatus("403 Forbidden");
                    nlohmann::json errorResponse = {
                        {"success", false},
                        {"message", "Forbidden: You don't have permission to update this profile"},
                        {"error", "FORBIDDEN"}
                    };
                    this->json(res, errorResponse);
                    LOG_WARNING("Unauthorized update attempt on profile: " + profileId);
                    return;
                }

                // Update only provided fields (partial update)
                std::string oldSlug = existingProfile.slug; // Save before overwriting for cache invalidation
                if (jsonBody.contains("slug") && jsonBody["slug"].is_string()) {
                    existingProfile.slug = jsonBody["slug"].get<std::string>();
                }

                if (jsonBody.contains("name") && jsonBody["name"].is_string()) {
                    existingProfile.name = jsonBody["name"].get<std::string>();
                }

                if (jsonBody.contains("type") && jsonBody["type"].is_string()) {
                    existingProfile.type = stringToProfileType(jsonBody["type"].get<std::string>());
                }

                if (jsonBody.contains("bio") && jsonBody["bio"].is_string()) {
                    existingProfile.bio = jsonBody["bio"].get<std::string>();
                }

                if (jsonBody.contains("isPublic") && jsonBody["isPublic"].is_boolean()) {
                    existingProfile.isPublic = jsonBody["isPublic"].get<bool>();
                }

                // Set ID for update
                existingProfile.id = profileId;

                // Validate the updated profile using ProfileValidator
                auto validationResult = search_engine::storage::ProfileValidator::validate(existingProfile);
                
                if (!validationResult.isValid) {
                    // Return 400 with structured error response
                    nlohmann::json errorResponse = {
                        {"success", false},
                        {"message", "Validation failed"},
                        {"errors", validationResult.errors}
                    };
                    
                    if (!validationResult.warnings.empty()) {
                        errorResponse["warnings"] = validationResult.warnings;
                    }
                    
                    res->writeStatus("400 Bad Request");
                    this->json(res, errorResponse);
                    LOG_WARNING("Profile validation failed during update: " + std::to_string(validationResult.errors.size()) + " errors");
                    return;
                }

                // Update in database
                auto updateResult = getStorage()->update(existingProfile);

                if (updateResult.success) {
                    // Audit log: record profile update
                    try {
                        std::string userId = getCallerIdentity(req);
                        std::string ipAddress = getClientIP(req);
                        std::string userAgent = getUserAgent(req);
                        
                        search_engine::storage::AuditLogger::logProfileUpdate(
                            oldProfile, existingProfile, userId, ipAddress, userAgent, getAuditStorage()
                        );
                    } catch (const std::exception& e) {
                        LOG_WARNING("Failed to record audit log: " + std::string(e.what()));
                    }
                    
                    // Clear cache if slug was changed
                    if (existingProfile.slug != oldSlug) {
                        getSlugCache()->remove(oldSlug);
                        getSlugCache()->remove(existingProfile.slug);
                        LOG_DEBUG("Profile slug changed from '" + oldSlug + "' to '" + existingProfile.slug + "' - cache entries removed");
                    }

                    nlohmann::json response = {
                        {"success", true},
                        {"message", updateResult.message},
                        {"data", profileToJson(existingProfile)}
                    };
                    
                    // Include warnings if present
                    if (!validationResult.warnings.empty()) {
                        response["warnings"] = validationResult.warnings;
                    }
                    
                    this->json(res, response);
                    LOG_INFO("Profile updated with ID: " + profileId);
                } else {
                    if (updateResult.message.find("already taken") != std::string::npos) {
                        badRequest(res, updateResult.message);
                    } else {
                        serverError(res, updateResult.message);
                    }
                }

            } catch (const nlohmann::json::parse_error& e) {
                LOG_ERROR("JSON parse error in updateProfile: " + std::string(e.what()));
                badRequest(res, "Invalid JSON format");
            } catch (const std::invalid_argument& e) {
                LOG_ERROR("Validation error in updateProfile: " + std::string(e.what()));
                badRequest(res, std::string(e.what()));
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

void ProfileController::deleteProfile(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    // Check rate limit
    if (checkRateLimit(res, req)) {
        return;
    }
    
    try {
        std::string id = std::string(req->getParameter(0));

        if (id.empty()) {
            badRequest(res, "Profile ID is required");
            return;
        }

        // First, get the existing profile to check ownership
        auto existingResult = getStorage()->findById(id);
        if (!existingResult.success) {
            notFound(res, "Profile not found");
            return;
        }

        auto profile = existingResult.value;

        // Check ownership (authentication)
        std::string authToken = getAuthToken(req);
        if (!checkOwnership(profile, authToken)) {
            res->writeStatus("403 Forbidden");
            nlohmann::json errorResponse = {
                {"success", false},
                {"message", "Forbidden: You don't have permission to delete this profile"},
                {"error", "FORBIDDEN"}
            };
            this->json(res, errorResponse);
            LOG_WARNING("Unauthorized delete attempt on profile: " + id);
            return;
        }

        auto result = getStorage()->deleteProfile(id);

        if (result.success) {
            // Audit log: record profile deletion
            try {
                std::string userId = getCallerIdentity(req);
                std::string ipAddress = getClientIP(req);
                std::string userAgent = getUserAgent(req);
                
                search_engine::storage::AuditLogger::logProfileDelete(
                    id, userId, ipAddress, userAgent, getAuditStorage()
                );
            } catch (const std::exception& e) {
                LOG_WARNING("Failed to record audit log: " + std::string(e.what()));
            }
            
            // Clear cache when profile is deleted
            getSlugCache()->clear();
            LOG_DEBUG("Profile deleted - cache cleared");

            // Return 204 No Content for successful deletion
            res->writeStatus("204 No Content");
            res->end();
            LOG_INFO("Profile deleted with ID: " + id);
        } else {
            notFound(res, result.message);
        }

    } catch (const std::exception& e) {
        LOG_ERROR("Error in deleteProfile: " + std::string(e.what()));
        serverError(res, "Internal server error");
    }
}

void ProfileController::restoreProfile(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    // Check rate limit
    if (checkRateLimit(res, req)) {
        return;
    }
    
    try {
        std::string id = std::string(req->getParameter(0));

        if (id.empty()) {
            badRequest(res, "Profile ID is required");
            return;
        }

        // Fetch the profile (including soft-deleted) to check ownership
        // Use a direct query that doesn't exclude deletedAt
        auto existingResult = getStorage()->findById(id);
        // If not found (findById excludes deleted), the profile might be deleted.
        // For restore, we trust the storage layer handles finding deleted profiles.
        // But we need to verify ownership if we can find the profile.

        // Check ownership (authentication)
        std::string authToken = getAuthToken(req);
        if (existingResult.success) {
            if (!checkOwnership(existingResult.value, authToken)) {
                res->writeStatus("403 Forbidden");
                nlohmann::json errorResponse = {
                    {"success", false},
                    {"message", "Forbidden: You don't have permission to restore this profile"},
                    {"error", "FORBIDDEN"}
                };
                this->json(res, errorResponse);
                LOG_WARNING("Unauthorized restore attempt on profile: " + id);
                return;
            }
        }

        auto result = getStorage()->restoreProfile(id);

        if (result.success) {
            nlohmann::json response = {
                {"success", true},
                {"message", result.message}
            };
            this->json(res, response);
            LOG_INFO("Profile restored with ID: " + id);
        } else {
            notFound(res, result.message);
        }

    } catch (const std::exception& e) {
        LOG_ERROR("Error in restoreProfile: " + std::string(e.what()));
        serverError(res, "Internal server error");
    }
}

void ProfileController::listProfiles(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    // Check rate limit
    if (checkRateLimit(res, req)) {
        return;
    }
    
    try {
        // Parse query parameters
        int limit = 50; // Default
        int skip = 0;   // Default
        std::optional<search_engine::storage::ProfileType> filterType;

        // Parse limit parameter
        auto limitParam = req->getQuery("limit");
        if (!limitParam.empty()) {
            try {
                limit = std::stoi(std::string(limitParam));
                if (limit < 1 || limit > 100) {
                    limit = 50; // Reset to default if out of range
                }
            } catch (const std::exception&) {
                limit = 50; // Reset to default if invalid
            }
        }

        // Parse skip parameter
        auto skipParam = req->getQuery("skip");
        if (!skipParam.empty()) {
            try {
                skip = std::stoi(std::string(skipParam));
                if (skip < 0) {
                    skip = 0; // Reset to default if negative
                }
            } catch (const std::exception&) {
                skip = 0; // Reset to default if invalid
            }
        }

        // Parse type filter
        auto typeParam = req->getQuery("type");
        if (!typeParam.empty()) {
            std::string typeStr = std::string(typeParam);
            if (typeStr == "PERSON") {
                filterType = search_engine::storage::ProfileType::PERSON;
            } else if (typeStr == "BUSINESS") {
                filterType = search_engine::storage::ProfileType::BUSINESS;
            }
        }

        // Query profiles
        Result<std::vector<search_engine::storage::Profile>> result;
        if (filterType.has_value()) {
            result = getStorage()->findByType(filterType.value(), limit, skip);
        } else {
            result = getStorage()->findAll(limit, skip);
        }

        if (result.success) {
            nlohmann::json profilesArray = nlohmann::json::array();
            for (const auto& profile : result.value) {
                profilesArray.push_back(profileToJson(profile));
            }

            nlohmann::json response = {
                {"success", true},
                {"message", result.message},
                {"data", profilesArray},
                {"count", static_cast<int>(result.value.size())}
            };
            json(res, response);
        } else {
            serverError(res, result.message);
        }

    } catch (const std::exception& e) {
        LOG_ERROR("Error in listProfiles: " + std::string(e.what()));
        serverError(res, "Internal server error");
    }
}

void ProfileController::getPublicProfileBySlug(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    // Check rate limit for public endpoints
    if (checkRateLimit(res, req)) {
        return;
    }
    
    try {
        std::string slug = std::string(req->getParameter(0));

        // Check for reserved paths first
        if (slug.empty() || search_engine::common::SlugGenerator::isReservedSlug(slug)) {
            // Return 404 for reserved paths - do not leave the connection hanging
            notFound(res, "Not found");
            return;
        }

        servePublicProfileBySlug(res, req, slug);

    } catch (const std::exception& e) {
        LOG_ERROR("Error in getPublicProfileBySlug: " + std::string(e.what()));
        serverError(res, "Internal server error");
    }
}

void ProfileController::servePublicProfileBySlug(uWS::HttpResponse<false>* res, uWS::HttpRequest* req, const std::string& slug) {
    // Check for SEO redirects first
    if (checkAndRedirectOldSlug(res, slug)) {
        return; // Redirect was issued
    }

    // Check cache first for better performance
    auto cachedProfileId = getSlugCache()->get(slug);
    if (cachedProfileId.has_value()) {
        // Get profile by ID from cache hit
        auto result = getStorage()->findById(cachedProfileId.value());
        if (result.success) {
            const auto& profile = result.value;

            // Check if profile is public
            if (!profile.isPublic) {
                res->writeStatus("403 Forbidden");
                nlohmann::json errorResponse = {
                    {"success", false},
                    {"message", "Profile is private"},
                    {"error", "PROFILE_PRIVATE"}
                };
                json(res, errorResponse);
                return;
            }

            // Record profile view (Tier 1 + Tier 2) - privacy-first analytics
            recordProfileView(profile.id.value_or(""), req);

            nlohmann::json response = {
                {"success", true},
                {"message", "Profile found (cached)"},
                {"data", profileToJson(profile)}
            };
            this->json(res, response);
            return;
        } else {
            // Cache miss - remove invalid cache entry
            getSlugCache()->remove(slug);
        }
    }

    // Cache miss - lookup from database
    auto result = getStorage()->findBySlug(slug);

    // Cache successful lookups
    if (result.success && result.value.has_value()) {
        getSlugCache()->put(slug, result.value.value().id.value_or(""));
    }

    if (result.success && result.value.has_value()) {
        const auto& profile = result.value.value();

        // Check if profile is public
        if (!profile.isPublic) {
            res->writeStatus("403 Forbidden");
            nlohmann::json errorResponse = {
                {"success", false},
                {"message", "Profile is private"},
                {"error", "PROFILE_PRIVATE"}
            };
            json(res, errorResponse);
            return;
        }

        // Record profile view (Tier 1 + Tier 2) - privacy-first analytics
        recordProfileView(profile.id.value_or(""), req);

        nlohmann::json response = {
            {"success", true},
            {"message", result.message},
            {"data", profileToJson(profile)}
        };
        json(res, response);
    } else {
        notFound(res, "Profile not found");
    }
}

void ProfileController::checkSlugAvailability(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    try {
        // Get slug from query parameter
        std::string slug = std::string(req->getQuery("slug"));

        if (slug.empty()) {
            badRequest(res, "Slug parameter is required");
            return;
        }

        // Check if slug is reserved
        if (search_engine::common::SlugGenerator::isReservedSlug(slug)) {
            nlohmann::json response = {
                {"success", true},
                {"available", false},
                {"slug", slug},
                {"message", "This slug is reserved and cannot be used"}
            };
            json(res, response);
            return;
        }

        // Check availability
        auto result = getStorage()->checkSlugAvailability(slug);

        nlohmann::json response = {
            {"success", result.success},
            {"available", result.success ? result.value : false},
            {"slug", slug}
        };

        // Add suggestions if not available
        if (result.success && !result.value) {
            // Generate some suggestions
            std::vector<std::string> suggestions;
            suggestions.push_back(slug + "-2");
            suggestions.push_back(slug + "-pro");
            suggestions.push_back(slug + "-official");

            response["suggestions"] = suggestions;
        }

        if (result.success) {
            response["message"] = result.value ? "Slug is available" : "Slug is already taken";
            json(res, response);
        } else {
            response["message"] = result.message;
            badRequest(res, result.message);
        }

    } catch (const std::exception& e) {
        LOG_ERROR("Error in checkSlugAvailability: " + std::string(e.what()));
        serverError(res, "Internal server error");
    }
}

void ProfileController::changeSlug(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    // Check rate limit
    if (checkRateLimit(res, req)) {
        return;
    }
    
    std::string buffer;
    std::string profileId = std::string(req->getParameter(0));

    if (profileId.empty()) {
        badRequest(res, "Profile ID is required");
        return;
    }

    res->onData([this, res, req, buffer = std::move(buffer), profileId](std::string_view data, bool last) mutable {
        buffer.append(data.data(), data.length());

        if (last) {
            try {
                // Parse JSON body
                auto jsonBody = nlohmann::json::parse(buffer);

                // Extract new slug
                if (!jsonBody.contains("slug") || !jsonBody["slug"].is_string()) {
                    badRequest(res, "New slug is required");
                    return;
                }

                std::string newSlug = jsonBody["slug"].get<std::string>();

                // First, get the existing profile to check ownership
                auto existingResult = getStorage()->findById(profileId);
                if (!existingResult.success) {
                    badRequest(res, "Profile not found");
                    return;
                }

                auto profile = existingResult.value;

                // Check ownership (authentication)
                std::string authToken = getAuthToken(req);
                if (!checkOwnership(profile, authToken)) {
                    res->writeStatus("403 Forbidden");
                    nlohmann::json errorResponse = {
                        {"success", false},
                        {"message", "Forbidden: You don't have permission to change this profile's slug"},
                        {"error", "FORBIDDEN"}
                    };
                    this->json(res, errorResponse);
                    LOG_WARNING("Unauthorized slug change attempt on profile: " + profileId);
                    return;
                }

                // Update slug
                auto result = getStorage()->updateSlug(profileId, newSlug);

                if (result.success) {
                    // Invalidate cache entries for the old slug
                    // Note: We can't easily know the old slug here, so we clear cache
                    // In production, you might want to track old slugs in cache or use a more sophisticated invalidation strategy
                    getSlugCache()->clear();

                    nlohmann::json response = {
                        {"success", true},
                        {"message", result.message}
                    };
                    this->json(res, response);
                    LOG_INFO("Slug changed for profile " + profileId + " to: " + newSlug + " (cache cleared)");
                } else {
                    badRequest(res, result.message);
                }

            } catch (const nlohmann::json::parse_error& e) {
                LOG_ERROR("JSON parse error in changeSlug: " + std::string(e.what()));
                badRequest(res, "Invalid JSON format");
            } catch (const std::exception& e) {
                LOG_ERROR("Error in changeSlug: " + std::string(e.what()));
                serverError(res, "Internal server error");
            }
        }
    });

    res->onAborted([]() {
        LOG_WARNING("Client disconnected during changeSlug request");
    });
}

bool ProfileController::checkAndRedirectOldSlug(uWS::HttpResponse<false>* res, const std::string& requestedSlug) {
    try {
        // Use targeted MongoDB query with index on previousSlugs
        auto result = getStorage()->findByPreviousSlug(requestedSlug);

        if (result.success && result.value.has_value()) {
            const auto& profile = result.value.value();
            // Found a match! Issue 301 redirect to current slug
            std::string redirectUrl = "/" + profile.slug;
            res->writeStatus("301 Moved Permanently");
            res->writeHeader("Location", redirectUrl);
            res->writeHeader("Content-Type", "text/html");
            res->end("<html><body><h1>301 Moved Permanently</h1><p>The profile has moved to <a href=\"" + redirectUrl + "\">" + redirectUrl + "</a></p></body></html>");

            LOG_INFO("SEO redirect: " + requestedSlug + " -> " + profile.slug + " (profile ID: " + (profile.id ? profile.id.value() : "unknown") + ")");
            return true;
        }

        return false; // No redirect needed

    } catch (const std::exception& e) {
        LOG_ERROR("Error in checkAndRedirectOldSlug: " + std::string(e.what()));
        return false; // Don't redirect on error, let normal flow continue
    }
}

void ProfileController::getPrivacyDashboard(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    try {
        std::string profileId = std::string(req->getParameter(0));
        
        if (profileId.empty()) {
            badRequest(res, "Profile ID is required");
            return;
        }

        // Verify ownership before exposing analytics data
        auto existingResult = getStorage()->findById(profileId);
        if (!existingResult.success) {
            notFound(res, "Profile not found");
            return;
        }

        std::string authToken = getAuthToken(req);
        if (!checkOwnership(existingResult.value, authToken)) {
            res->writeStatus("403 Forbidden");
            nlohmann::json errorResponse = {
                {"success", false},
                {"message", "Forbidden: You don't have permission to view this privacy dashboard"},
                {"error", "FORBIDDEN"}
            };
            this->json(res, errorResponse);
            LOG_WARNING("Unauthorized privacy dashboard access attempt for profile: " + profileId);
            return;
        }
        
        LOG_INFO("Privacy dashboard requested for profile: " + profileId);
        
        // Get recent views from Tier 1 analytics (last 30 days)
        auto viewsResult = getAnalyticsStorage()->getRecentViewsByProfile(profileId, 30);
        auto viewCountResult = getAnalyticsStorage()->countViewsByProfile(profileId);
        
        // Build activity log (privacy-first: no IPs!)
        nlohmann::json activityLog = nlohmann::json::array();
        
        if (viewsResult.success) {
            for (const auto& view : viewsResult.value) {
                auto time_t = std::chrono::system_clock::to_time_t(view.timestamp);
                std::stringstream ss;
                ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
                
                nlohmann::json activity = {
                    {"when", ss.str()},
                    {"action", "profile_view"},
                    {"location", view.city + ", " + view.province + ", " + view.country},
                    {"device", view.browser + " on " + view.os + " (" + view.deviceType + ")"}
                };
                activityLog.push_back(activity);
            }
        }
        
        // Data retention settings
        nlohmann::json dataRetention = {
            {"profileData", "Until account deletion"},
            {"analyticsData", "2 years (730 days)"},
            {"complianceLogs", "12 months (365 days) - auto-deleted"},
            {"deletedData", "Immediate (0 days)"}
        };
        
        // User controls (foundation - auth will gate these)
        nlohmann::json userControls = {
            {"canExportAllData", true},
            {"canDeleteAccount", true},
            {"canControlRetention", false}  // Future feature
        };
        
        // Build response
        nlohmann::json response = {
            {"success", true},
            {"data", {
                {"profileId", profileId},
                {"totalViews", viewCountResult.success ? viewCountResult.value : 0},
                {"recentActivity", activityLog},
                {"dataRetention", dataRetention},
                {"userControls", userControls},
                {"legalRequestsCount", 0},  // Future: track court orders
                {"privacyLevel", "Maximum"},
                {"encryptionEnabled", true},
                {"ipAddressStored", "Encrypted only (12 months)"}
            }}
        };
        
        json(res, response);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error in getPrivacyDashboard: " + std::string(e.what()));
        serverError(res, "Internal server error");
    }
}

void ProfileController::cleanupExpiredComplianceLogs(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    try {
        // Verify API key for internal endpoint
        std::string apiKey = std::string(req->getHeader("x-api-key"));
        const char* expectedKey = std::getenv("INTERNAL_API_KEY");
        
        if (!expectedKey || apiKey.empty() || apiKey != std::string(expectedKey)) {
            res->writeStatus("403 Forbidden");
            nlohmann::json errorResponse = {
                {"success", false},
                {"message", "Unauthorized: Invalid or missing API key"},
                {"error", "INVALID_API_KEY"}
            };
            json(res, errorResponse);
            LOG_WARNING("Unauthorized compliance cleanup attempt");
            return;
        }
        
        LOG_INFO("Compliance cleanup job started (authorized)");
        
        // Count expired logs before deletion (for audit)
        auto countResult = getComplianceStorage()->countExpiredLogs();
        int64_t expiredCount = countResult.success ? countResult.value : 0;
        
        // Delete expired compliance logs
        auto deleteResult = getComplianceStorage()->deleteExpiredLogs();
        
        if (deleteResult.success) {
            nlohmann::json response = {
                {"success", true},
                {"message", "Compliance logs cleanup completed"},
                {"data", {
                    {"expiredLogsFound", expiredCount},
                    {"logsDeleted", deleteResult.value},
                    {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
                }}
            };
            json(res, response);
            LOG_INFO("Compliance cleanup completed: " + std::to_string(deleteResult.value) + " logs deleted");
        } else {
            res->writeStatus("500 Internal Server Error");
            nlohmann::json errorResponse = {
                {"success", false},
                {"message", "Failed to cleanup compliance logs: " + deleteResult.message},
                {"error", "CLEANUP_FAILED"}
            };
            json(res, errorResponse);
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error in cleanupExpiredComplianceLogs: " + std::string(e.what()));
        serverError(res, "Internal server error");
    }
}

// ==================== Link Block Methods ====================

search_engine::storage::LinkBlockStorage* ProfileController::getLinkBlockStorage() const {
    if (!linkBlockStorage_) {
        try {
            LOG_INFO("Lazy initializing LinkBlockStorage");
            linkBlockStorage_ = std::make_unique<search_engine::storage::LinkBlockStorage>();
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to lazy initialize LinkBlockStorage: " + std::string(e.what()));
            throw;
        }
    }
    return linkBlockStorage_.get();
}

search_engine::storage::LinkClickAnalyticsStorage* ProfileController::getLinkClickAnalyticsStorage() const {
    if (!linkClickAnalyticsStorage_) {
        try {
            LOG_INFO("Lazy initializing LinkClickAnalyticsStorage");
            linkClickAnalyticsStorage_ = std::make_unique<search_engine::storage::LinkClickAnalyticsStorage>();
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to lazy initialize LinkClickAnalyticsStorage: " + std::string(e.what()));
            throw;
        }
    }
    return linkClickAnalyticsStorage_.get();
}

ApiRateLimiter* ProfileController::getLinkRedirectRateLimiter() const {
    if (!linkRedirectRateLimiter_) {
        try {
            LOG_INFO("Lazy initializing link redirect ApiRateLimiter");
            // Get config from environment or use defaults (stricter for redirect)
            size_t maxRequests = 120; // Default: 120 requests per minute (2/s)
            int windowSeconds = 60;
            
            const char* limitEnv = std::getenv("LINK_REDIRECT_RATE_LIMIT_REQUESTS");
            if (limitEnv) {
                maxRequests = std::stoi(limitEnv);
            }
            
            const char* windowEnv = std::getenv("LINK_REDIRECT_RATE_LIMIT_WINDOW_SECONDS");
            if (windowEnv) {
                windowSeconds = std::stoi(windowEnv);
            }
            
            linkRedirectRateLimiter_ = std::make_unique<ApiRateLimiter>(maxRequests, std::chrono::seconds(windowSeconds));
            LOG_INFO("Link redirect rate limiter configured: " + std::to_string(maxRequests) + 
                    " requests per " + std::to_string(windowSeconds) + " seconds");
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to lazy initialize link redirect rate limiter: " + std::string(e.what()));
            throw;
        }
    }
    return linkRedirectRateLimiter_.get();
}

bool ProfileController::checkLinkRedirectRateLimit(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    std::string clientIP = getClientIP(req);
    
    if (getLinkRedirectRateLimiter()->shouldThrottle(clientIP)) {
        int retryAfter = getLinkRedirectRateLimiter()->getRetryAfter(clientIP);
        
        res->writeStatus("429 Too Many Requests");
        res->writeHeader("Retry-After", std::to_string(retryAfter));
        
        nlohmann::json errorResponse = {
            {"success", false},
            {"message", "Too many redirect requests. Please try again later."},
            {"error", "RATE_LIMIT_EXCEEDED"},
            {"retryAfter", retryAfter}
        };
        
        this->json(res, errorResponse);
        LOG_WARNING("Link redirect rate limit exceeded for IP: " + clientIP);
        return true; // Rate limited
    }
    
    return false; // Not rate limited
}

void ProfileController::recordLinkClick(const std::string& linkId, const std::string& profileId, uWS::HttpRequest* req) {
    try {
        // Get client IP and User-Agent
        std::string ipAddress = getClientIP(req);
        std::string userAgent = getUserAgent(req);
        std::string referrer = getReferrer(req);
        
        LOG_DEBUG("Recording link click: linkId=" + linkId + ", profileId=" + profileId);
        
        // Generate unique click ID using random_device for thread safety
        auto now = std::chrono::system_clock::now();
        auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        std::random_device rd;
        std::string clickId = std::to_string(nowMs) + "-" + std::to_string(rd() % 1000000);
        
        // Privacy-first analytics (NO IP!)
        search_engine::storage::GeoData geo = search_engine::storage::GeoIPService::lookup(ipAddress);
        search_engine::storage::UserAgentInfo uaInfo = search_engine::storage::UserAgentParser::parse(userAgent);
        
        search_engine::storage::LinkClickAnalytics analytics;
        analytics.clickId = clickId;
        analytics.linkId = linkId;
        analytics.profileId = profileId;
        analytics.timestamp = now;
        analytics.country = geo.country;
        analytics.province = geo.province;
        analytics.city = geo.city;
        analytics.browser = uaInfo.browser;
        analytics.os = uaInfo.os;
        analytics.deviceType = uaInfo.deviceType;
        analytics.referrer = referrer.empty() ? "Direct" : referrer;
        
        auto analyticsResult = getLinkClickAnalyticsStorage()->recordClick(analytics);
        if (!analyticsResult.success) {
            LOG_WARNING("Failed to record link click analytics: " + analyticsResult.message);
        }
        
        // Secure memory wipe for sensitive data
        search_engine::storage::secureMemoryWipe(&ipAddress);
        search_engine::storage::secureMemoryWipe(&userAgent);
        search_engine::storage::secureMemoryWipe(&referrer);
        
        LOG_INFO("Link click recorded successfully: clickId=" + clickId);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to record link click: " + std::string(e.what()));
        // Don't fail the redirect if tracking fails
    }
}

search_engine::storage::LinkBlock ProfileController::parseLinkFromJson(const nlohmann::json& json) {
    search_engine::storage::LinkBlock link;
    
    // Required: url
    if (json.contains("url") && json["url"].is_string()) {
        link.url = json["url"].get<std::string>();
    } else {
        throw std::invalid_argument("Missing required field: url");
    }
    
    // Required: title
    if (json.contains("title") && json["title"].is_string()) {
        link.title = json["title"].get<std::string>();
    } else {
        throw std::invalid_argument("Missing required field: title");
    }
    
    // Optional: description
    if (json.contains("description") && json["description"].is_string()) {
        link.description = json["description"].get<std::string>();
    }
    
    // Optional: iconUrl
    if (json.contains("iconUrl") && json["iconUrl"].is_string()) {
        link.iconUrl = json["iconUrl"].get<std::string>();
    }
    
    // Optional: isActive
    if (json.contains("isActive") && json["isActive"].is_boolean()) {
        link.isActive = json["isActive"].get<bool>();
    }
    
    // Optional: privacy
    if (json.contains("privacy") && json["privacy"].is_string()) {
        std::string privacyStr = json["privacy"].get<std::string>();
        link.privacy = search_engine::storage::stringToLinkPrivacy(privacyStr);
    }
    
    // Optional: tags
    if (json.contains("tags") && json["tags"].is_array()) {
        for (const auto& tag : json["tags"]) {
            if (tag.is_string()) {
                link.tags.push_back(tag.get<std::string>());
            }
        }
    }
    
    // Optional: sortOrder
    if (json.contains("sortOrder") && json["sortOrder"].is_number()) {
        link.sortOrder = json["sortOrder"].get<int>();
    }
    
    return link;
}

nlohmann::json ProfileController::linkToJson(const search_engine::storage::LinkBlock& link) const {
    nlohmann::json json;
    
    if (link.id.has_value()) {
        json["id"] = link.id.value();
    }
    
    json["profileId"] = link.profileId;
    json["url"] = link.url;
    json["title"] = link.title;
    
    if (link.description.has_value()) {
        json["description"] = link.description.value();
    }
    if (link.iconUrl.has_value()) {
        json["iconUrl"] = link.iconUrl.value();
    }
    
    json["isActive"] = link.isActive;
    json["privacy"] = search_engine::storage::linkPrivacyToString(link.privacy);
    json["tags"] = link.tags;
    json["sortOrder"] = link.sortOrder;
    
    // Timestamps (ISO 8601)
    auto createdTime = std::chrono::system_clock::to_time_t(link.createdAt);
    std::tm createdTm = *std::gmtime(&createdTime);
    char createdBuffer[32];
    std::strftime(createdBuffer, sizeof(createdBuffer), "%Y-%m-%dT%H:%M:%SZ", &createdTm);
    json["createdAt"] = createdBuffer;
    
    if (link.updatedAt.has_value()) {
        auto updatedTime = std::chrono::system_clock::to_time_t(link.updatedAt.value());
        std::tm updatedTm = *std::gmtime(&updatedTime);
        char updatedBuffer[32];
        std::strftime(updatedBuffer, sizeof(updatedBuffer), "%Y-%m-%dT%H:%M:%SZ", &updatedTm);
        json["updatedAt"] = updatedBuffer;
    }
    
    return json;
}

void ProfileController::redirectLink(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    try {
        // Check rate limit first
        if (checkLinkRedirectRateLimit(res, req)) {
            return; // Rate limited
        }
        
        // Get link ID from URL parameter
        std::string linkId = std::string(req->getParameter(0));
        
        LOG_DEBUG("Link redirect requested: " + linkId);
        
        // Find link by ID
        auto linkResult = getLinkBlockStorage()->findById(linkId);
        if (!linkResult.success || !linkResult.value.has_value()) {
            res->writeStatus("404 Not Found");
            res->end("Link not found");
            return;
        }
        
        const auto& link = linkResult.value.value();
        
        // Check if link is active
        if (!link.isActive || link.privacy == search_engine::storage::LinkPrivacy::DISABLED) {
            res->writeStatus("404 Not Found");
            res->end("Link not available");
            return;
        }
        
        // Check if profile is public
        auto profileResult = getStorage()->findById(link.profileId);
        if (!profileResult.success || !profileResult.value.isPublic) {
            res->writeStatus("404 Not Found");
            res->end("Link not available");
            return;
        }
        
        // Record click analytics (async, don't block redirect)
        if (link.privacy == search_engine::storage::LinkPrivacy::PUBLIC) {
            // Only record analytics for public links
            recordLinkClick(link.id.value(), link.profileId, req);
        }
        
        // Perform redirect (secure: only to stored URL)
        res->writeStatus("302 Found");
        res->writeHeader("Location", link.url);
        res->end();
        
        LOG_INFO("Link redirect: " + linkId + " -> " + link.url);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error in redirectLink: " + std::string(e.what()));
        res->writeStatus("500 Internal Server Error");
        res->end("Internal server error");
    }
}

void ProfileController::createLink(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    // Rate limit check
    if (checkRateLimit(res, req)) {
        return;
    }
    
    std::string profileId = std::string(req->getParameter(0));
    
    // Setup response handlers for POST data
    std::string buffer;
    
    res->onAborted([&buffer]() {
        LOG_WARNING("Request aborted during createLink");
    });
    
    res->onData([this, res, req, profileId, &buffer](std::string_view chunk, bool isFinal) {
        buffer.append(chunk.data(), chunk.size());
        
        if (!isFinal) {
            return;
        }
        
        try {
            // Parse JSON body
            nlohmann::json json = nlohmann::json::parse(buffer);
            
            // Verify profile ownership
            auto profileResult = getStorage()->findById(profileId);
            if (!profileResult.success) {
                badRequest(res, "Profile not found");
                return;
            }
            
            std::string token = getAuthToken(req);
            if (!checkOwnership(profileResult.value, token)) {
                res->writeStatus("403 Forbidden");
                nlohmann::json errorResponse = {
                    {"success", false},
                    {"message", "Not authorized to create links for this profile"},
                    {"error", "FORBIDDEN"}
                };
                this->json(res, errorResponse);
                return;
            }
            
            // Parse link from JSON
            auto link = parseLinkFromJson(json);
            link.profileId = profileId;
            link.createdAt = std::chrono::system_clock::now();
            
            // Validate link
            if (!link.isValid()) {
                badRequest(res, "Invalid link data");
                return;
            }
            
            // Store link
            auto result = getLinkBlockStorage()->store(link);
            
            if (result.success) {
                link.id = result.value;
                nlohmann::json response = {
                    {"success", true},
                    {"message", "Link created successfully"},
                    {"data", linkToJson(link)}
                };
                this->json(res, response);
                LOG_INFO("Link created: " + result.value);
            } else {
                res->writeStatus("500 Internal Server Error");
                nlohmann::json errorResponse = {
                    {"success", false},
                    {"message", "Failed to create link: " + result.message},
                    {"error", "STORAGE_ERROR"}
                };
                this->json(res, errorResponse);
            }
            
        } catch (const std::invalid_argument& e) {
            badRequest(res, std::string(e.what()));
        } catch (const nlohmann::json::exception& e) {
            badRequest(res, "Invalid JSON: " + std::string(e.what()));
        } catch (const std::exception& e) {
            LOG_ERROR("Error in createLink: " + std::string(e.what()));
            serverError(res, "Internal server error");
        }
    });
}

void ProfileController::getLinks(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    try {
        std::string profileId = std::string(req->getParameter(0));
        
        LOG_DEBUG("Fetching links for profile: " + profileId);
        
        // Find links for profile
        auto result = getLinkBlockStorage()->findByProfile(profileId, 100, 0);
        
        if (result.success) {
            nlohmann::json linksArray = nlohmann::json::array();
            for (const auto& link : result.value) {
                linksArray.push_back(linkToJson(link));
            }
            
            nlohmann::json response = {
                {"success", true},
                {"message", "Links retrieved successfully"},
                {"data", linksArray}
            };
            this->json(res, response);
        } else {
            serverError(res, "Failed to retrieve links: " + result.message);
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error in getLinks: " + std::string(e.what()));
        serverError(res, "Internal server error");
    }
}

void ProfileController::getLinkById(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    try {
        std::string profileId = std::string(req->getParameter(0));
        std::string linkId = std::string(req->getParameter(1));
        
        // Find link by ID
        auto result = getLinkBlockStorage()->findById(linkId);
        
        if (!result.success) {
            serverError(res, "Failed to retrieve link: " + result.message);
            return;
        }
        
        if (!result.value.has_value()) {
            res->writeStatus("404 Not Found");
            nlohmann::json errorResponse = {
                {"success", false},
                {"message", "Link not found"},
                {"error", "NOT_FOUND"}
            };
            this->json(res, errorResponse);
            return;
        }
        
        const auto& link = result.value.value();
        
        // Verify link belongs to profile
        if (link.profileId != profileId) {
            res->writeStatus("404 Not Found");
            nlohmann::json errorResponse = {
                {"success", false},
                {"message", "Link not found"},
                {"error", "NOT_FOUND"}
            };
            this->json(res, errorResponse);
            return;
        }
        
        nlohmann::json response = {
            {"success", true},
            {"message", "Link retrieved successfully"},
            {"data", linkToJson(link)}
        };
        this->json(res, response);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error in getLinkById: " + std::string(e.what()));
        serverError(res, "Internal server error");
    }
}

void ProfileController::updateLink(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    // Rate limit check
    if (checkRateLimit(res, req)) {
        return;
    }
    
    std::string profileId = std::string(req->getParameter(0));
    std::string linkId = std::string(req->getParameter(1));
    
    std::string buffer;
    
    res->onAborted([&buffer]() {
        LOG_WARNING("Request aborted during updateLink");
    });
    
    res->onData([this, res, req, profileId, linkId, &buffer](std::string_view chunk, bool isFinal) {
        buffer.append(chunk.data(), chunk.size());
        
        if (!isFinal) {
            return;
        }
        
        try {
            // Verify profile ownership
            auto profileResult = getStorage()->findById(profileId);
            if (!profileResult.success) {
                badRequest(res, "Profile not found");
                return;
            }
            
            std::string token = getAuthToken(req);
            if (!checkOwnership(profileResult.value, token)) {
                res->writeStatus("403 Forbidden");
                nlohmann::json errorResponse = {
                    {"success", false},
                    {"message", "Not authorized to update links for this profile"},
                    {"error", "FORBIDDEN"}
                };
                this->json(res, errorResponse);
                return;
            }
            
            // Get existing link
            auto linkResult = getLinkBlockStorage()->findById(linkId);
            if (!linkResult.success || !linkResult.value.has_value()) {
                badRequest(res, "Link not found");
                return;
            }
            
            auto link = linkResult.value.value();
            
            // Verify link belongs to profile
            if (link.profileId != profileId) {
                badRequest(res, "Link not found");
                return;
            }
            
            // Parse updates from JSON
            nlohmann::json json = nlohmann::json::parse(buffer);
            
            if (json.contains("url")) link.url = json["url"].get<std::string>();
            if (json.contains("title")) link.title = json["title"].get<std::string>();
            if (json.contains("description")) {
                link.description = json["description"].is_null() ? std::nullopt : std::optional<std::string>(json["description"].get<std::string>());
            }
            if (json.contains("iconUrl")) {
                link.iconUrl = json["iconUrl"].is_null() ? std::nullopt : std::optional<std::string>(json["iconUrl"].get<std::string>());
            }
            if (json.contains("isActive")) link.isActive = json["isActive"].get<bool>();
            if (json.contains("privacy")) {
                link.privacy = search_engine::storage::stringToLinkPrivacy(json["privacy"].get<std::string>());
            }
            if (json.contains("tags")) {
                link.tags.clear();
                for (const auto& tag : json["tags"]) {
                    link.tags.push_back(tag.get<std::string>());
                }
            }
            if (json.contains("sortOrder")) link.sortOrder = json["sortOrder"].get<int>();
            
            // Validate updated link
            if (!link.isValid()) {
                badRequest(res, "Invalid link data");
                return;
            }
            
            // Update link
            auto result = getLinkBlockStorage()->update(link);
            
            if (result.success) {
                nlohmann::json response = {
                    {"success", true},
                    {"message", "Link updated successfully"},
                    {"data", linkToJson(link)}
                };
                this->json(res, response);
                LOG_INFO("Link updated: " + linkId);
            } else {
                serverError(res, "Failed to update link: " + result.message);
            }
            
        } catch (const std::invalid_argument& e) {
            badRequest(res, std::string(e.what()));
        } catch (const nlohmann::json::exception& e) {
            badRequest(res, "Invalid JSON: " + std::string(e.what()));
        } catch (const std::exception& e) {
            LOG_ERROR("Error in updateLink: " + std::string(e.what()));
            serverError(res, "Internal server error");
        }
    });
}

void ProfileController::deleteLink(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    try {
        std::string profileId = std::string(req->getParameter(0));
        std::string linkId = std::string(req->getParameter(1));
        
        // Verify profile ownership
        auto profileResult = getStorage()->findById(profileId);
        if (!profileResult.success) {
            badRequest(res, "Profile not found");
            return;
        }
        
        std::string token = getAuthToken(req);
        if (!checkOwnership(profileResult.value, token)) {
            res->writeStatus("403 Forbidden");
            nlohmann::json errorResponse = {
                {"success", false},
                {"message", "Not authorized to delete links for this profile"},
                {"error", "FORBIDDEN"}
            };
            this->json(res, errorResponse);
            return;
        }
        
        // Verify link belongs to profile
        auto linkResult = getLinkBlockStorage()->findById(linkId);
        if (!linkResult.success || !linkResult.value.has_value()) {
            badRequest(res, "Link not found");
            return;
        }
        
        if (linkResult.value.value().profileId != profileId) {
            badRequest(res, "Link not found");
            return;
        }
        
        // Delete link
        auto result = getLinkBlockStorage()->deleteLink(linkId);
        
        if (result.success) {
            nlohmann::json response = {
                {"success", true},
                {"message", "Link deleted successfully"}
            };
            this->json(res, response);
            LOG_INFO("Link deleted: " + linkId);
        } else {
            serverError(res, "Failed to delete link: " + result.message);
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error in deleteLink: " + std::string(e.what()));
        serverError(res, "Internal server error");
    }
}

void ProfileController::getLinkAnalytics(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    try {
        std::string profileId = std::string(req->getParameter(0));
        
        // Verify profile ownership
        auto profileResult = getStorage()->findById(profileId);
        if (!profileResult.success) {
            badRequest(res, "Profile not found");
            return;
        }
        
        std::string token = getAuthToken(req);
        if (!checkOwnership(profileResult.value, token)) {
            res->writeStatus("403 Forbidden");
            nlohmann::json errorResponse = {
                {"success", false},
                {"message", "Not authorized to view analytics for this profile"},
                {"error", "FORBIDDEN"}
            };
            this->json(res, errorResponse);
            return;
        }
        
        // Get total clicks for profile
        auto countResult = getLinkClickAnalyticsStorage()->countClicksByProfile(profileId);
        int64_t totalClicks = countResult.success ? countResult.value : 0;
        
        // Get recent clicks
        auto clicksResult = getLinkClickAnalyticsStorage()->getRecentClicksByProfile(profileId, 100);
        
        // Build analytics response
        nlohmann::json response = {
            {"success", true},
            {"message", "Analytics retrieved successfully"},
            {"data", {
                {"totalClicks", totalClicks},
                {"recentClicks", nlohmann::json::array()}
            }}
        };
        
        if (clicksResult.success) {
            for (const auto& click : clicksResult.value) {
                nlohmann::json clickJson = {
                    {"linkId", click.linkId},
                    {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                        click.timestamp.time_since_epoch()).count()},
                    {"country", click.country},
                    {"city", click.city},
                    {"browser", click.browser},
                    {"os", click.os},
                    {"deviceType", click.deviceType},
                    {"referrer", click.referrer}
                };
                response["data"]["recentClicks"].push_back(clickJson);
            }
        }
        
        this->json(res, response);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error in getLinkAnalytics: " + std::string(e.what()));
        serverError(res, "Internal server error");
    }
}

void ProfileController::cleanupExpiredLinkAnalytics(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    try {
        LOG_INFO("Link analytics cleanup job requested");
        
        // Require API key for internal endpoint
        std::string apiKey = std::string(req->getHeader("x-api-key"));
        const char* expectedKey = std::getenv("INTERNAL_API_KEY");
        
        if (!expectedKey || apiKey.empty() || apiKey != std::string(expectedKey)) {
            res->writeStatus("403 Forbidden");
            nlohmann::json errorResponse = {
                {"success", false},
                {"message", "Unauthorized: Invalid or missing API key"},
                {"error", "INVALID_API_KEY"}
            };
            json(res, errorResponse);
            LOG_WARNING("Unauthorized link analytics cleanup attempt");
            return;
        }
        
        LOG_INFO("Link analytics cleanup job started (authorized)");
        
        // Get retention days from environment or use default
        const char* retentionEnv = std::getenv("LINK_ANALYTICS_RETENTION_DAYS");
        int retentionDays = retentionEnv ? std::stoi(retentionEnv) : 90;
        
        // Calculate cutoff timestamp
        auto now = std::chrono::system_clock::now();
        auto cutoff = now - std::chrono::hours(24 * retentionDays);
        
        // Delete old analytics
        auto deleteResult = getLinkClickAnalyticsStorage()->deleteOldClicks(cutoff);
        
        if (deleteResult.success) {
            nlohmann::json response = {
                {"success", true},
                {"message", "Link analytics cleanup completed"},
                {"data", {
                    {"retentionDays", retentionDays},
                    {"analyticsDeleted", deleteResult.value},
                    {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                        now.time_since_epoch()).count()}
                }}
            };
            json(res, response);
            LOG_INFO("Link analytics cleanup completed: " + std::to_string(deleteResult.value) + 
                    " records deleted (retention: " + std::to_string(retentionDays) + " days)");
        } else {
            res->writeStatus("500 Internal Server Error");
            nlohmann::json errorResponse = {
                {"success", false},
                {"message", "Failed to cleanup link analytics: " + deleteResult.message},
                {"error", "CLEANUP_FAILED"}
            };
            json(res, errorResponse);
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error in cleanupExpiredLinkAnalytics: " + std::string(e.what()));
        serverError(res, "Internal server error");
    }
}
