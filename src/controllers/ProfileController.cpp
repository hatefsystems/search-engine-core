#include "ProfileController.h"
#include "../../include/search_engine/profile/ProfileEditor.h"
#include "../../include/Logger.h"
#include "../../include/search_engine/common/SlugGenerator.h"
#include "../../include/search_engine/common/ProfileSlug.h"
#include "../../include/search_engine/profile/PublicProfile.h"
#include "../../include/search_engine/profile/ProfileJson.h"
#include "../../include/search_engine/storage/ProfileValidator.h"
#include "../../include/search_engine/storage/AuditLogger.h"
#include "../../include/search_engine/seo/SEOGenerator.h"
#include "../../include/search_engine/common/Base64.h"
#include "../../include/search_engine/common/ImageValidator.h"
#include "../../include/search_engine/common/SkillNormalizer.h"
#include "../../include/search_engine/skills/SkillsData.h"
#include <nlohmann/json.hpp>
#include <inja/inja.hpp>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <random>
#include <fstream>
#include <filesystem>

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

// ==================== Template Rendering Helpers ====================

std::string ProfileController::renderTemplate(const std::string& templateName, const nlohmann::json& data) {
    try {
        LOG_DEBUG("Rendering template: " + templateName);
        
        // Initialize Inja environment
        inja::Environment env("templates/");
        
        // Render the template with data
        std::string result = env.render_file(templateName, search_engine::profile::profileTemplateData(data));
        LOG_DEBUG("Successfully rendered template: " + templateName);
        return result;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error rendering template " + templateName + ": " + std::string(e.what()));
        throw;
    }
}

void ProfileController::renderProfilePage(uWS::HttpResponse<false>* res, const search_engine::storage::Profile& profile,
    const search_engine::storage::PersonProfile* person) {
    try {
        LOG_DEBUG("Rendering profile page for: " + profile.slug);
        
        // Get base URL from environment or use default
        const char* baseUrlEnv = std::getenv("BASE_URL");
        std::string baseUrl = baseUrlEnv ? std::string(baseUrlEnv) : "http://localhost:3000";
        
        // Fetch link blocks for this profile
        nlohmann::json linksArray = nlohmann::json::array();
        try {
            auto linksResult = getLinkBlockStorage()->findByProfile(profile.id.value_or(""));
            if (linksResult.success) {
                for (const auto& link : linksResult.value) {
                    // Only include public and active links
                    if (link.visibility == "PUBLIC" && link.privacy == search_engine::storage::LinkPrivacy::PUBLIC && link.isActive) {
                        linksArray.push_back(linkToJson(link));
                    }
                }
            }
        } catch (const std::exception& e) {
            LOG_WARNING("Failed to fetch link blocks: " + std::string(e.what()));
            // Continue without links
        }
        
        // Determine profile type and generate appropriate schema
        nlohmann::json jsonld;
        std::string templateName;
        std::string profileType;
        
        if (profile.type == search_engine::storage::ProfileType::PERSON) {
            if (!person) throw std::runtime_error("Full personal profile is required");
            auto seoPerson = *person;
            if (seoPerson.displayName && !seoPerson.displayName->empty())
                seoPerson.name = *seoPerson.displayName;
            if (!seoPerson.skillsWithLevel.empty()) {
                seoPerson.skills.clear();
                for (const auto& skill : seoPerson.skillsWithLevel)
                    seoPerson.skills.push_back(skill.name);
            }
            jsonld = search_engine::seo::SEOGenerator::generatePersonSchema(
                seoPerson, baseUrl, linksArray);
            templateName = "profile_person.inja";
            profileType = "person";
        } else {
            // Cast to BusinessProfile
            search_engine::storage::BusinessProfile businessProfile;
            // Copy base profile fields
            businessProfile.id = profile.id;
            businessProfile.slug = profile.slug;
            businessProfile.name = profile.name;
            businessProfile.type = profile.type;
            businessProfile.bio = profile.bio;
            businessProfile.isPublic = profile.isPublic;
            businessProfile.previousSlugs = profile.previousSlugs;
            businessProfile.slugChangedAt = profile.slugChangedAt;
            businessProfile.createdAt = profile.createdAt;
            businessProfile.updatedAt = profile.updatedAt;
            businessProfile.deletedAt = profile.deletedAt;
            businessProfile.ownerToken = profile.ownerToken;
            businessProfile.ownerId = profile.ownerId;
            
            // For now, use empty business-specific fields
            businessProfile.services = {};
            
            jsonld = search_engine::seo::SEOGenerator::generateOrganizationSchema(
                businessProfile, 
                baseUrl, 
                linksArray
            );
            templateName = "profile_organization.inja";
            profileType = "organization";
        }
        
        // Generate Open Graph and Twitter Card tags
        auto openGraph = search_engine::seo::SEOGenerator::generateOpenGraphTags(
            profile, 
            baseUrl, 
            profileType
        );
        auto twitterCard = search_engine::seo::SEOGenerator::generateTwitterCardTags(
            profile, 
            baseUrl, 
            profileType
        );
        
        // Prepare template data
        nlohmann::json templateData = {
            {"profile", person ? personProfileToJson(*person) : profileToJson(profile)},
            {"links", linksArray},
            {"jsonld", jsonld},
            {"openGraph", openGraph},
            {"twitterCard", twitterCard},
            {"baseUrl", baseUrl},
            {"seo", {
                {"title", search_engine::seo::SEOGenerator::generatePageTitle(profile)},
                {"description", search_engine::seo::SEOGenerator::generateMetaDescription(profile)}
            }}
        };
        
        if (person) {
            search_engine::profile::addPersonHeaderData(templateData);
            templateData["advancedSections"] = search_engine::profile::publicSectionCards(person->content, person->id.value_or(""));
            // Only already-public data is embedded. Keep initial payload bounded;
            // later cards use the existing unauthenticated public pagination API.
            auto presentation = templateData["profile"];
            for (const auto* key : {"completion", "contentLayout", "privacy", "version", "createdAt", "updatedAt"}) presentation.erase(key);
            presentation["links"] = linksArray;
            presentation["totals"] = nlohmann::json::object();
            presentation["featuredItems"] = nlohmann::json::array();
            for (auto& [section, items] : presentation["sections"].items()) {
                presentation["totals"][section] = items.size();
                for (const auto& ref : presentation["featured"]) {
                    if (ref.value("section", "") != section) continue;
                    for (const auto& item : items) if (item["id"] == ref["id"]) presentation["featuredItems"].push_back(item);
                }
                const size_t initialLimit = section == "skills" ? 7 : 3;
                if (items.size() > initialLimit) items.erase(items.begin() + initialLimit, items.end());
            }
            templateData["publicPresentation"] = presentation.dump();

            for (auto& tag : templateData["openGraph"]) {
                if (tag["property"] == "og:locale") tag["content"] = "fa_IR";
            }
        }

        // Render template
        std::string html = renderTemplate(templateName, templateData);
        
        // Send HTML response
        res->writeHeader("Content-Type", "text/html; charset=utf-8");
        res->writeHeader("Cache-Control", "no-cache, must-revalidate");
        res->writeHeader("Vary", "Accept");
        res->writeHeader("Server", "HatefEngine 1.0")->end(html);
        
        LOG_DEBUG("Successfully rendered profile page for: " + profile.slug);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error rendering profile page: " + std::string(e.what()));
        serverError(res, "Failed to render profile page");
    }
}

// ==================== Authentication & Ownership Helpers ====================

std::string ProfileController::generateOwnerToken() { return search_engine::profile::newOwnerKey(); }

bool ProfileController::secureCookies() const {
    const char* base = std::getenv("BASE_URL");
    return base && std::string(base).rfind("https://", 0) == 0;
}

bool ProfileController::sameOrigin(uWS::HttpRequest* req) {
    const auto origin = std::string(req->getHeader("origin"));
    const auto host = std::string(req->getHeader("host"));
    return !origin.empty() && !host.empty() && origin == (secureCookies() ? "https://" : "http://") + host;
}

std::string ProfileController::getAuthToken(uWS::HttpRequest* req) {
    const std::string auth(req->getHeader("authorization"));
    if (auth.rfind("Bearer ", 0) == 0) return auth.substr(7);
    const std::string header(req->getHeader("x-profile-token"));
    if (!header.empty()) return header;
    const auto method = req->getMethod();
    if (method != "get" && method != "head" && !sameOrigin(req)) return "";
    return search_engine::profile::readCookie(req->getHeader("cookie"), std::string(req->getParameter(0)));
}

bool ProfileController::checkOwnership(const search_engine::storage::Profile& profile, const std::string& token) {
    return search_engine::profile::ownsProfile(profile, token);
}

std::string ProfileController::getCallerIdentity(uWS::HttpRequest* req) {
    return getAuthToken(req).empty() ? "anonymous" : "profile-owner";
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
        throw std::invalid_argument(search_engine::common::profileSlugValidationError(profile.slug));
    }

    return profile;
}

nlohmann::json ProfileController::profileToJson(const search_engine::storage::Profile& profile) {
    return search_engine::profile::profileToJson(profile);
}

nlohmann::json ProfileController::personProfileToJson(const search_engine::storage::PersonProfile& profile) {
    return search_engine::profile::personProfileToJson(profile);
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

        // First, get the basic profile to check the type
        auto result = getStorage()->findById(id);

        if (result.success) {
            nlohmann::json response;

            const bool isOwner = checkOwnership(result.value, getAuthToken(req));
            if (!result.value.isPublic && !isOwner) {
                this->json(res, {{"success", false}, {"message", "Profile is private"},
                    {"error", "PROFILE_PRIVATE"}}, "403 Forbidden");
                return;
            }
            
            // If it's a PERSON profile, get the full PersonProfile data
            if (result.value.type == search_engine::storage::ProfileType::PERSON) {
                auto personResult = getStorage()->findPersonById(id);
                if (personResult.success && personResult.value.has_value()) {
                    if (!personResult.value->isPublic && !isOwner) {
                        this->json(res, {{"success", false}, {"message", "Profile is private"},
                            {"error", "PROFILE_PRIVATE"}}, "403 Forbidden");
                        return;
                    }
                    const auto person = isOwner ? *personResult.value :
                        search_engine::profile::publicPersonProfile(*personResult.value);
                    response = {
                        {"success", true},
                        {"message", personResult.message},
                        {"data", personProfileToJson(person)}
                    };
                } else {
                    response = {
                        {"success", true},
                        {"message", result.message},
                        {"data", profileToJson(result.value)}
                    };
                }
            } else {
                // For non-person profiles, use the basic profileToJson
                response = {
                    {"success", true},
                    {"message", result.message},
                    {"data", profileToJson(result.value)}
                };
            }
            
            response["canEdit"] = isOwner;
            res->writeStatus("200 OK");
            res->writeHeader("Cache-Control", "private, no-store");
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
    const std::string capturedToken = getAuthToken(req);
    const std::string callerIp = getClientIP(req);
    const std::string callerAgent = getUserAgent(req);
    if (capturedToken.empty() && checkRateLimit(res, req)) return;
    
    std::string buffer;
    std::string profileId = std::string(req->getParameter(0));

    if (profileId.empty()) {
        badRequest(res, "Profile ID is required");
        return;
    }

    res->onData([this, res, capturedToken, callerIp, callerAgent, buffer = std::move(buffer), profileId](std::string_view data, bool last) mutable {
        if (buffer.size() + data.size() > 65536) {
            res->writeStatus("413 Payload Too Large"); res->writeHeader("Server", "HatefEngine 1.0")->end(); return;
        }
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
                std::string authToken = capturedToken;
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

                if (existingProfile.type == search_engine::storage::ProfileType::PERSON) {
                    auto full = getStorage()->findPersonById(profileId);
                    if (!full.success || !full.value) { notFound(res, "Profile not found"); return; }
                    savePersonPatch(res, *full.value, jsonBody);
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
                        std::string userId = std::string("profile-owner");
                        std::string ipAddress = callerIp;
                        std::string userAgent = callerAgent;
                        
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
            res->writeHeader("Server", "HatefEngine 1.0")->end();
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
                if (!profile.isPublic) continue;
                if (profile.type == search_engine::storage::ProfileType::PERSON) {
                    auto person=getStorage()->findPersonById(profile.id.value_or(""));
                    if(person.success && person.value && person.value->isPublic) profilesArray.push_back(personProfileToJson(search_engine::profile::publicPersonProfile(*person.value)));
                } else profilesArray.push_back(profileToJson(profile));
            }

            nlohmann::json response = {
                {"success", true},
                {"message", result.message},
                {"data", profilesArray},
                {"count", static_cast<int>(profilesArray.size())}
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

        servePublicProfileBySlug(res, req, slug);

    } catch (const std::exception& e) {
        LOG_ERROR("Error in getPublicProfileBySlug: " + std::string(e.what()));
        serverError(res, "Internal server error");
    }
}

void ProfileController::servePublicProfileBySlug(uWS::HttpResponse<false>* res, uWS::HttpRequest* req, const std::string& rawSlug) {
    // Both public route aliases decode exactly once, before any cache or DB access.
    const auto decoded = search_engine::common::decodeProfileSlug(rawSlug, true);
    if (!decoded) {
        badRequest(res, "Invalid profile slug or URL encoding");
        return;
    }
    const auto& slug = *decoded;
    if (search_engine::common::SlugGenerator::isReservedSlug(slug)) {
        notFound(res, "Not found");
        return;
    }
    const auto canonical = search_engine::common::canonicalProfileSlug(slug).value();
    if (canonical != slug) {
        auto target = getStorage()->findBySlug(canonical);
        if (!target.success) { serverError(res, "Failed to resolve canonical profile"); return; }
        if (target.value) {
            res->writeStatus("301 Moved Permanently")
                ->writeHeader("Location", "/" + search_engine::common::encodeProfileSlug(canonical))
                ->writeHeader("Cache-Control", "no-cache, must-revalidate")
                ->writeHeader("Server", "HatefEngine 1.0")->end();
            return;
        }
    }
    if (checkAndRedirectOldSlug(res, slug)) return;

    const bool wantsJson = req->getHeader("accept").find("application/json") != std::string_view::npos;
    auto missing = [&]() {
        if (wantsJson) { notFound(res, "Profile not found"); return; }
        auto available = getStorage()->checkSlugAvailability(canonical);
        if (!available.success) { serverError(res, "امکان بررسی آدرس وجود ندارد."); return; }
        renderProfileEntry(res, canonical, available.value ? "missing" : "unavailable");
    };
    std::optional<search_engine::storage::Profile> resolved;
    bool cached = false;
    const auto cachedId = getSlugCache()->get(slug);
    if (cachedId) {
        const auto result = getStorage()->findById(*cachedId);
        if (result.success && result.value.slug == slug && !result.value.deletedAt) {
            resolved = result.value;
            cached = true;
        } else {
            getSlugCache()->remove(slug);
        }
    }
    if (!resolved) {
        const auto result = getStorage()->findBySlug(slug);
        if (!result.success) {
            serverError(res, "Failed to load profile");
            return;
        }
        if (!result.value) { missing(); return; }
        resolved = *result.value;
        getSlugCache()->put(slug, resolved->id.value_or(""));
    }

    std::optional<search_engine::storage::PersonProfile> person;
    if (resolved->type == search_engine::storage::ProfileType::PERSON) {
        const auto result = getStorage()->findPersonById(resolved->id.value_or(""));
        if (!result.success) {
            serverError(res, "Failed to load personal profile");
            return;
        }
        if (!result.value || result.value->deletedAt || result.value->slug != slug) {
            getSlugCache()->remove(slug);
            missing();
            return;
        }
        person = search_engine::profile::publicPersonProfile(*result.value);
    }
    const search_engine::storage::Profile& profile = person ?
        static_cast<const search_engine::storage::Profile&>(*person) : *resolved;
    if (!profile.isPublic) {
        res->writeStatus("403 Forbidden");
        res->writeHeader("Cache-Control", "no-cache, must-revalidate");
        res->writeHeader("Vary", "Accept");
        if (wantsJson) {
            this->json(res, {{"success", false}, {"message", "Profile is private"}, {"error", "PROFILE_PRIVATE"}}, "403 Forbidden");
        } else {
            renderProfileEntry(res, slug, "private");
        }
        return;
    }
    recordProfileView(profile.id.value_or(""), req);
    if (wantsJson) {
        res->writeStatus("200 OK");
        res->writeHeader("Cache-Control", "no-cache, must-revalidate");
        res->writeHeader("Vary", "Accept");
        this->json(res, {
            {"success", true},
            {"message", cached ? "Profile found (cached)" : "Profile found"},
            {"data", person ? personProfileToJson(*person) : profileToJson(profile)}
        });
    } else {
        renderProfilePage(res, profile, person ? &*person : nullptr);
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

        const auto normalized = search_engine::common::canonicalProfileSlug(slug);
        if (!normalized) { badRequest(res, search_engine::common::profileSlugValidationError(slug)); return; }
        slug = *normalized;

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
            suggestions.push_back(search_engine::common::truncateProfileSlug(slug, 98) + ".2");
            suggestions.push_back(search_engine::common::truncateProfileSlug(slug, 96) + ".pro");
            suggestions.push_back(search_engine::common::truncateProfileSlug(slug, 91) + ".official");

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

    const std::string capturedToken = getAuthToken(req);
    const std::string capturedIp = getClientIP(req);
    const std::string capturedAgent = getUserAgent(req);
    res->onData([this, res, capturedToken, capturedIp, capturedAgent, buffer = std::move(buffer), profileId](std::string_view data, bool last) mutable {
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
                std::string authToken = capturedToken;
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
            std::string redirectUrl = "/" + search_engine::common::encodeProfileSlug(profile.slug);
            res->writeStatus("301 Moved Permanently");
            res->writeHeader("Location", redirectUrl);
            res->writeHeader("Content-Type", "text/html");
            res->writeHeader("Server", "HatefEngine 1.0")->end("<html><body><h1>301 Moved Permanently</h1><p>The profile has moved to <a href=\"" + redirectUrl + "\">" + redirectUrl + "</a></p></body></html>");

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
    json["version"] = link.version;
    json["visibility"] = link.visibility;
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
            res->writeHeader("Server", "HatefEngine 1.0")->end("Link not found");
            return;
        }
        
        const auto& link = linkResult.value.value();
        
        // Check if link is active
        if (link.visibility == "HIDDEN" || !link.isActive || link.privacy == search_engine::storage::LinkPrivacy::DISABLED) {
            res->writeStatus("404 Not Found");
            res->writeHeader("Server", "HatefEngine 1.0")->end("Link not available");
            return;
        }
        
        // Check if profile is public
        auto profileResult = getStorage()->findById(link.profileId);
        if (!profileResult.success || !profileResult.value.isPublic) {
            res->writeStatus("404 Not Found");
            res->writeHeader("Server", "HatefEngine 1.0")->end("Link not available");
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
        res->writeHeader("Server", "HatefEngine 1.0")->end();
        
        LOG_INFO("Link redirect: " + linkId);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error in redirectLink: " + std::string(e.what()));
        res->writeStatus("500 Internal Server Error");
        res->writeHeader("Server", "HatefEngine 1.0")->end("Internal server error");
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

// ==================== Image Upload Endpoints ====================

void ProfileController::uploadAvatar(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    if (checkRateLimit(res, req)) {
        return;
    }
    
    std::string profileId = std::string(req->getParameter(0));
    std::string buffer;

    if (profileId.empty()) {
        badRequest(res, "Profile ID is required");
        return;
    }

    res->onAborted([res]() {
        LOG_DEBUG("Avatar upload request aborted");
    });

    const std::string capturedToken = getAuthToken(req);
    const std::string capturedIp = getClientIP(req);
    const std::string capturedAgent = getUserAgent(req);
    res->onData([this, res, capturedToken, capturedIp, capturedAgent, buffer = std::move(buffer), profileId](std::string_view data, bool last) mutable {
        if (buffer.size() + data.size() > 15 * 1024 * 1024) {
            res->writeStatus("413 Payload Too Large")->writeHeader("Server", "HatefEngine 1.0")->end(); return;
        }
        buffer.append(data.data(), data.length());

        if (last) {
            try {
                // Parse JSON body
                auto jsonBody = nlohmann::json::parse(buffer);

                if (!jsonBody.contains("image") || !jsonBody["image"].is_string()) {
                    badRequest(res, "Image data is required (base64 encoded)");
                    return;
                }

                // Get existing profile
                auto profileResult = getStorage()->findPersonById(profileId);
                if (!profileResult.success || !profileResult.value.has_value()) {
                    notFound(res, "Profile not found");
                    return;
                }

                auto personProfile = profileResult.value.value();

                // Check ownership
                std::string authToken = capturedToken;
                if (!checkOwnership(personProfile, authToken)) {
                    res->writeStatus("403 Forbidden");
                    nlohmann::json errorResponse = {
                        {"success", false},
                        {"message", "Forbidden: You don't have permission to upload avatar"},
                        {"error", "FORBIDDEN"}
                    };
                    json(res, errorResponse);
                    return;
                }

                if (!jsonBody.contains("version") || !jsonBody["version"].is_number_integer()) {
                    badRequest(res, "نسخهٔ اطلاعات لازم است."); return;
                }
                if (jsonBody["version"].get<int64_t>() != personProfile.version) {
                    json(res, {{"message", "اطلاعات در جای دیگری تغییر کرده است."}}, "409 Conflict"); return;
                }
                if (checkOwnerMutationRateLimit(res, personProfile.id.value())) return;
                // Decode base64 image
                std::string base64Data = jsonBody["image"].get<std::string>();
                
                // Remove data URI prefix if present (e.g., "data:image/png;base64,")
                size_t commaPos = base64Data.find(',');
                if (commaPos != std::string::npos) {
                    base64Data = base64Data.substr(commaPos + 1);
                }

                std::vector<unsigned char> imageData = search_engine::common::Base64::decode(base64Data);

                // Validate image
                auto imageInfo = search_engine::common::ImageValidator::validate(
                    imageData, 
                    search_engine::common::ImageValidator::MAX_AVATAR_SIZE
                );

                if (!imageInfo.isValid) {
                    badRequest(res, "Invalid image: unsupported format or exceeds size limit (5 MB)");
                    return;
                }

                // Save image to file
                std::string extension = search_engine::common::ImageValidator::getExtension(imageInfo.type);
                std::string filePath = saveImageToFile(imageData, profileId, "avatar", extension);

                // Update profile with avatar URL
                personProfile.avatarUrl = filePath;
                auto updateResult = getStorage()->updatePersonFields(personProfile, {"avatarUrl"}, personProfile.version);

                if (updateResult.success) {
                    nlohmann::json response = {
                        {"success", true},
                        {"message", "Avatar uploaded successfully"},
                        {"data", {
                            {"avatarUrl", filePath},
                            {"version", personProfile.version + 1},
                            {"size", (int)imageInfo.size},
                            {"mimeType", imageInfo.mimeType}
                        }}
                    };
                    json(res, response);
                    LOG_INFO("Avatar uploaded for profile: " + profileId);
                } else {
                    json(res, {{"message", "ذخیرهٔ تصویر انجام نشد."}}, updateResult.message == "VERSION_CONFLICT" ? "409 Conflict" : "500 Internal Server Error");
                }

            } catch (const nlohmann::json::exception& e) {
                LOG_ERROR("JSON parse error in uploadAvatar: " + std::string(e.what()));
                badRequest(res, "Invalid JSON");
            } catch (const std::exception& e) {
                LOG_ERROR("Error in uploadAvatar: " + std::string(e.what()));
                serverError(res, "Failed to upload avatar");
            }
        }
    });
}

void ProfileController::uploadCover(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    if (checkRateLimit(res, req)) {
        return;
    }
    
    std::string profileId = std::string(req->getParameter(0));
    std::string buffer;

    if (profileId.empty()) {
        badRequest(res, "Profile ID is required");
        return;
    }

    res->onAborted([res]() {
        LOG_DEBUG("Cover upload request aborted");
    });

    const std::string capturedToken = getAuthToken(req);
    const std::string capturedIp = getClientIP(req);
    const std::string capturedAgent = getUserAgent(req);
    res->onData([this, res, capturedToken, capturedIp, capturedAgent, buffer = std::move(buffer), profileId](std::string_view data, bool last) mutable {
        if (buffer.size() + data.size() > 15 * 1024 * 1024) {
            res->writeStatus("413 Payload Too Large")->writeHeader("Server", "HatefEngine 1.0")->end(); return;
        }
        buffer.append(data.data(), data.length());

        if (last) {
            try {
                // Parse JSON body
                auto jsonBody = nlohmann::json::parse(buffer);

                if (!jsonBody.contains("image") || !jsonBody["image"].is_string()) {
                    badRequest(res, "Image data is required (base64 encoded)");
                    return;
                }

                // Get existing profile
                auto profileResult = getStorage()->findPersonById(profileId);
                if (!profileResult.success || !profileResult.value.has_value()) {
                    notFound(res, "Profile not found");
                    return;
                }

                auto personProfile = profileResult.value.value();

                // Check ownership
                std::string authToken = capturedToken;
                if (!checkOwnership(personProfile, authToken)) {
                    res->writeStatus("403 Forbidden");
                    nlohmann::json errorResponse = {
                        {"success", false},
                        {"message", "Forbidden: You don't have permission to upload cover image"},
                        {"error", "FORBIDDEN"}
                    };
                    json(res, errorResponse);
                    return;
                }

                if (!jsonBody.contains("version") || !jsonBody["version"].is_number_integer()) {
                    badRequest(res, "نسخهٔ اطلاعات لازم است."); return;
                }
                if (jsonBody["version"].get<int64_t>() != personProfile.version) {
                    json(res, {{"message", "اطلاعات در جای دیگری تغییر کرده است."}}, "409 Conflict"); return;
                }
                if (checkOwnerMutationRateLimit(res, personProfile.id.value())) return;
                // Decode base64 image
                std::string base64Data = jsonBody["image"].get<std::string>();
                
                // Remove data URI prefix if present
                size_t commaPos = base64Data.find(',');
                if (commaPos != std::string::npos) {
                    base64Data = base64Data.substr(commaPos + 1);
                }

                std::vector<unsigned char> imageData = search_engine::common::Base64::decode(base64Data);

                // Validate image
                auto imageInfo = search_engine::common::ImageValidator::validate(
                    imageData, 
                    search_engine::common::ImageValidator::MAX_COVER_SIZE
                );

                if (!imageInfo.isValid) {
                    badRequest(res, "Invalid image: unsupported format or exceeds size limit (10 MB)");
                    return;
                }

                // Save image to file
                std::string extension = search_engine::common::ImageValidator::getExtension(imageInfo.type);
                std::string filePath = saveImageToFile(imageData, profileId, "cover", extension);

                // Update profile with cover URL
                personProfile.coverImageUrl = filePath;
                auto updateResult = getStorage()->updatePersonFields(personProfile, {"coverImageUrl"}, personProfile.version);

                if (updateResult.success) {
                    nlohmann::json response = {
                        {"success", true},
                        {"message", "Cover image uploaded successfully"},
                        {"data", {
                            {"coverImageUrl", filePath},
                            {"version", personProfile.version + 1},
                            {"size", (int)imageInfo.size},
                            {"mimeType", imageInfo.mimeType}
                        }}
                    };
                    json(res, response);
                    LOG_INFO("Cover image uploaded for profile: " + profileId);
                } else {
                    json(res, {{"message", "ذخیرهٔ تصویر انجام نشد."}}, updateResult.message == "VERSION_CONFLICT" ? "409 Conflict" : "500 Internal Server Error");
                }

            } catch (const nlohmann::json::exception& e) {
                LOG_ERROR("JSON parse error in uploadCover: " + std::string(e.what()));
                badRequest(res, "Invalid JSON");
            } catch (const std::exception& e) {
                LOG_ERROR("Error in uploadCover: " + std::string(e.what()));
                serverError(res, "Failed to upload cover image");
            }
        }
    });
}

// ==================== Image Upload Helpers ====================

std::string ProfileController::saveImageToFile(
    const std::vector<unsigned char>& imageData,
    const std::string& profileId,
    const std::string& imageType,
    const std::string& extension
) {
    // Create uploads directory if it doesn't exist
    std::string uploadsDir = "uploads/" + imageType + "s";
    std::filesystem::create_directories(uploadsDir);

    // Generate secure filename
    std::string filename = generateSecureFilename(profileId, extension);
    std::string filePath = uploadsDir + "/" + filename;

    // Write file
    std::ofstream outFile(filePath, std::ios::binary);
    if (!outFile) {
        throw std::runtime_error("Failed to create file: " + filePath);
    }

    outFile.write(reinterpret_cast<const char*>(imageData.data()), imageData.size());
    outFile.close();

    LOG_DEBUG("Saved " + imageType + " image to: " + filePath);

    // Return relative URL path
    return "/" + filePath;
}

std::string ProfileController::generateSecureFilename(
    const std::string& profileId,
    const std::string& extension
) {
    // Generate timestamp
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    // Generate random hash
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);

    std::stringstream ss;
    for (int i = 0; i < 8; i++) {
        ss << std::hex << dis(gen);
    }

    // Format: {profileId}_{timestamp}_{hash}.{ext}
    return profileId + "_" + std::to_string(timestamp) + "_" + ss.str() + "." + extension;
}

// ==================== Skills Management Endpoints ====================

void ProfileController::addSkills(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    if (checkRateLimit(res, req)) {
        return;
    }
    
    std::string profileId = std::string(req->getParameter(0));
    std::string buffer;

    if (profileId.empty()) {
        badRequest(res, "Profile ID is required");
        return;
    }

    res->onAborted([res]() {
        LOG_DEBUG("Add skills request aborted");
    });

    const std::string capturedToken = getAuthToken(req);
    const std::string capturedIp = getClientIP(req);
    const std::string capturedAgent = getUserAgent(req);
    res->onData([this, res, capturedToken, capturedIp, capturedAgent, buffer = std::move(buffer), profileId](std::string_view data, bool last) mutable {
        buffer.append(data.data(), data.length());

        if (last) {
            try {
                // Parse JSON body
                auto jsonBody = nlohmann::json::parse(buffer);

                if (!jsonBody.contains("skills") || !jsonBody["skills"].is_array()) {
                    badRequest(res, "Skills array is required");
                    return;
                }

                // Get existing profile
                auto profileResult = getStorage()->findPersonById(profileId);
                if (!profileResult.success || !profileResult.value.has_value()) {
                    notFound(res, "Profile not found");
                    return;
                }

                auto personProfile = profileResult.value.value();

                // Check ownership
                std::string authToken = capturedToken;
                if (!checkOwnership(personProfile, authToken)) {
                    res->writeStatus("403 Forbidden");
                    nlohmann::json errorResponse = {
                        {"success", false},
                        {"message", "Forbidden: You don't have permission to modify skills"},
                        {"error", "FORBIDDEN"}
                    };
                    json(res, errorResponse);
                    return;
                }

                if (!jsonBody.contains("version") || !jsonBody["version"].is_number_integer()) { badRequest(res, "نسخهٔ اطلاعات لازم است."); return; }
                if (jsonBody["version"].get<int64_t>() != personProfile.version) { json(res, {{"message", "اطلاعات در جای دیگری تغییر کرده است."}}, "409 Conflict"); return; }
                // Parse skills from request
                for (const auto& skillJson : jsonBody["skills"]) {
                    if (!skillJson.contains("name") || !skillJson["name"].is_string()) {
                        continue;
                    }

                    search_engine::storage::SkillWithLevel skill;
                    skill.name = search_engine::skills::SkillNormalizer::normalize(
                        skillJson["name"].get<std::string>()
                    );
                    
                    skill.level = skillJson.contains("level") && skillJson["level"].is_string()
                        ? skillJson["level"].get<std::string>()
                        : "INTERMEDIATE";
                    
                    skill.category = skillJson.contains("category") && skillJson["category"].is_string()
                        ? skillJson["category"].get<std::string>()
                        : search_engine::skills::SkillNormalizer::getCategory(skill.name);

                    // Check for duplicates
                    bool isDuplicate = false;
                    for (const auto& existingSkill : personProfile.skillsWithLevel) {
                        if (existingSkill.name == skill.name) {
                            isDuplicate = true;
                            break;
                        }
                    }

                    if (!isDuplicate) {
                        personProfile.skillsWithLevel.push_back(skill);
                    }
                }

                // Update profile
                nlohmann::json skills = nlohmann::json::array();
                for (const auto& skill : personProfile.skillsWithLevel) skills.push_back({{"name", skill.name}, {"level", skill.level}});
                auto checked = personProfile;
                search_engine::profile::applyEditorPatch(checked, {{"skillsWithLevel", skills}});
                search_engine::profile::bridgeLegacySkillsAddition(checked);
                if (checkOwnerMutationRateLimit(res, personProfile.id.value())) return;
                auto updateResult = getStorage()->updatePersonFields(checked, {"skills", "skillsWithLevel", "content"}, personProfile.version);

                if (updateResult.success) {
                    nlohmann::json response = {
                        {"success", true},
                        {"message", "Skills added successfully"},
                        {"data", {
                            {"version", personProfile.version + 1},
                            {"skillsCount", (int)personProfile.skillsWithLevel.size()}
                        }}
                    };
                    json(res, response);
                    LOG_INFO("Skills added to profile: " + profileId);
                } else {
                    serverError(res, "Failed to update profile with skills");
                }

            } catch (const nlohmann::json::exception& e) {
                LOG_ERROR("JSON parse error in addSkills: " + std::string(e.what()));
                badRequest(res, "Invalid JSON");
            } catch (const std::exception& e) {
                LOG_ERROR("Error in addSkills: " + std::string(e.what()));
                serverError(res, "Failed to add skills");
            }
        }
    });
}

void ProfileController::removeSkill(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    if (checkRateLimit(res, req)) {
        return;
    }
    
    std::string profileId = std::string(req->getParameter(0));
    std::string skillName = std::string(req->getParameter(1));

    if (profileId.empty() || skillName.empty()) {
        badRequest(res, "Profile ID and skill name are required");
        return;
    }

    try {
        // Get existing profile
        auto profileResult = getStorage()->findPersonById(profileId);
        if (!profileResult.success || !profileResult.value.has_value()) {
            notFound(res, "Profile not found");
            return;
        }

        auto personProfile = profileResult.value.value();

        // Check ownership
        std::string authToken = getAuthToken(req);
        if (!checkOwnership(personProfile, authToken)) {
            res->writeStatus("403 Forbidden");
            nlohmann::json errorResponse = {
                {"success", false},
                {"message", "Forbidden: You don't have permission to modify skills"},
                {"error", "FORBIDDEN"}
            };
            json(res, errorResponse);
            return;
        }

        const auto expected = std::string(req->getHeader("if-match"));
        if (expected.empty()) { badRequest(res, "نسخهٔ اطلاعات در If-Match لازم است."); return; }
        if (expected != std::to_string(personProfile.version)) { json(res, {{"message", "اطلاعات در جای دیگری تغییر کرده است."}}, "409 Conflict"); return; }
        if (search_engine::profile::removePersonSkill(personProfile, skillName)) {
            // Update profile
            if (checkOwnerMutationRateLimit(res, personProfile.id.value())) return;
                auto updateResult = getStorage()->updatePersonFields(personProfile, {"skills", "skillsWithLevel", "content"}, personProfile.version);

            if (updateResult.success) {
                nlohmann::json response = {
                    {"success", true},
                    {"message", "Skill removed successfully"},
                    {"data", {
                        {"version", personProfile.version + 1},
                        {"skillsCount", (int)personProfile.skillsWithLevel.size()}
                    }}
                };
                json(res, response);
                LOG_INFO("Skill removed from profile: " + profileId);
            } else {
                json(res, {{"message", "ذخیره انجام نشد."}}, updateResult.message == "VERSION_CONFLICT" ? "409 Conflict" : "500 Internal Server Error");
            }
        } else {
            notFound(res, "Skill not found in profile");
        }

    } catch (const std::exception& e) {
        LOG_ERROR("Error in removeSkill: " + std::string(e.what()));
        serverError(res, "Failed to remove skill");
    }
}

void ProfileController::getSkillsAutocomplete(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    try {
        // Get query parameter
        std::string query = std::string(req->getQuery("q"));

        if (query.empty()) {
            badRequest(res, "Query parameter 'q' is required");
            return;
        }

        // Get limit parameter (default: 10)
        std::string limitStr = std::string(req->getQuery("limit"));
        int limit = limitStr.empty() ? 10 : std::stoi(limitStr);
        limit = std::min(limit, 50); // Max 50 results

        // Autocomplete skills
        auto results = search_engine::skills::SkillNormalizer::autocomplete(query, limit);

        nlohmann::json response = {
            {"success", true},
            {"message", "Skills autocomplete results"},
            {"data", {
                {"query", query},
                {"results", results},
                {"count", results.size()}
            }}
        };

        json(res, response);

    } catch (const std::exception& e) {
        LOG_ERROR("Error in getSkillsAutocomplete: " + std::string(e.what()));
        serverError(res, "Failed to get autocomplete results");
    }
}
