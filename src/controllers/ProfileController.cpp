#include "ProfileController.h"
#include "../../include/Logger.h"
#include "../../include/search_engine/common/SlugGenerator.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <sstream>
#include <iomanip>

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

void ProfileController::recordProfileView(const std::string& profileId, uWS::HttpRequest* req) {
    try {
        // Get client IP and User-Agent
        std::string ipAddress = getClientIP(req);
        std::string userAgent = getUserAgent(req);
        std::string referrer = getReferrer(req);
        
        LOG_DEBUG("Recording profile view: profileId=" + profileId + ", IP=" + ipAddress);
        
        // Generate unique view ID (UUID simulation with timestamp + random)
        auto now = std::chrono::system_clock::now();
        auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        std::string viewId = std::to_string(nowMs) + "-" + std::to_string(rand() % 10000);
        
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
    std::string buffer;

    res->onData([this, res, buffer = std::move(buffer)](std::string_view data, bool last) mutable {
        buffer.append(data.data(), data.length());

        if (last) {
            try {
                // Parse JSON body
                auto jsonBody = nlohmann::json::parse(buffer);

                // Parse profile from JSON
                auto profile = parseProfileFromJson(jsonBody);

                // Set creation timestamp
                profile.createdAt = std::chrono::system_clock::now();

                // Save to database
                auto result = getStorage()->store(profile);

                if (result.success) {
                    nlohmann::json response = {
                        {"success", true},
                        {"message", result.message},
                        {"data", profileToJson(profile)}
                    };
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
    try {
        std::string slug = std::string(req->getParameter(0));

        if (slug.empty()) {
            badRequest(res, "Profile slug is required");
            return;
        }

        // Check for SEO redirects first
        auto redirectResult = checkAndRedirectOldSlug(res, slug);
        if (redirectResult) {
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
                // Return 403 Forbidden for private profiles
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

    } catch (const std::exception& e) {
        LOG_ERROR("Error in getPublicProfile: " + std::string(e.what()));
        serverError(res, "Internal server error");
    }
}

void ProfileController::updateProfile(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    std::string buffer;
    std::string profileId = std::string(req->getParameter(0));

    if (profileId.empty()) {
        badRequest(res, "Profile ID is required");
        return;
    }

    res->onData([this, res, buffer = std::move(buffer), profileId](std::string_view data, bool last) mutable {
        buffer.append(data.data(), data.length());

        if (last) {
            try {
                // Parse JSON body
                auto jsonBody = nlohmann::json::parse(buffer);

                // First, get the existing profile
                auto existingResult = getStorage()->findById(profileId);
                if (!existingResult.success) {
                    notFound(res, "Profile not found");
                    return;
                }

                auto existingProfile = existingResult.value;

                // Update only provided fields (partial update)
                if (jsonBody.contains("slug") && jsonBody["slug"].is_string()) {
                    existingProfile.slug = jsonBody["slug"].get<std::string>();
                    // Validate slug format
                    if (!search_engine::storage::ProfileStorage::isValidSlug(existingProfile.slug)) {
                        badRequest(res, "Invalid slug format. Slug must contain only Persian letters, English letters, numbers, and hyphens.");
                        return;
                    }
                }

                if (jsonBody.contains("name") && jsonBody["name"].is_string()) {
                    existingProfile.name = jsonBody["name"].get<std::string>();
                }

                if (jsonBody.contains("type") && jsonBody["type"].is_string()) {
                    existingProfile.type = stringToProfileType(jsonBody["type"].get<std::string>());
                }

                if (jsonBody.contains("bio") && jsonBody["bio"].is_string()) {
                    existingProfile.bio = jsonBody["bio"].get<std::string>();
                    // Validate bio length
                    if (existingProfile.bio->length() > 500) {
                        badRequest(res, "Bio exceeds maximum length of 500 characters");
                        return;
                    }
                }

                if (jsonBody.contains("isPublic") && jsonBody["isPublic"].is_boolean()) {
                    existingProfile.isPublic = jsonBody["isPublic"].get<bool>();
                }

                // Set ID for update
                existingProfile.id = profileId;

                // Update in database
                auto updateResult = getStorage()->update(existingProfile);

                if (updateResult.success) {
                    // Clear cache if slug was changed
                    if (existingProfile.slug != jsonBody["slug"].get<std::string>()) {
                        getSlugCache()->clear();
                        LOG_DEBUG("Profile slug changed - cache cleared");
                    }

                    nlohmann::json response = {
                        {"success", true},
                        {"message", updateResult.message},
                        {"data", profileToJson(existingProfile)}
                    };
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
    try {
        std::string id = std::string(req->getParameter(0));

        if (id.empty()) {
            badRequest(res, "Profile ID is required");
            return;
        }

        auto result = getStorage()->deleteProfile(id);

        if (result.success) {
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

void ProfileController::listProfiles(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
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
    try {
        std::string slug = std::string(req->getParameter(0));

        // Check for reserved paths first
        if (slug.empty() || search_engine::common::SlugGenerator::isReservedSlug(slug)) {
            // Let other controllers handle this (404 will be returned by uWS if no route matches)
            return;
        }

        // Check for SEO redirects first - look for profiles that have this slug in previousSlugs
        auto redirectResult = checkAndRedirectOldSlug(res, slug);
        if (redirectResult) {
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
                // Return 403 Forbidden for private profiles
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
            // Profile not found - this will result in 404
            notFound(res, "Profile not found");
        }

    } catch (const std::exception& e) {
        LOG_ERROR("Error in getPublicProfileBySlug: " + std::string(e.what()));
        serverError(res, "Internal server error");
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
    std::string buffer;
    std::string profileId = std::string(req->getParameter(0));

    if (profileId.empty()) {
        badRequest(res, "Profile ID is required");
        return;
    }

    res->onData([this, res, buffer = std::move(buffer), profileId](std::string_view data, bool last) mutable {
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
        // Query all profiles to find one that has this slug in previousSlugs
        // This is not optimal for production - should use a dedicated index
        auto allProfiles = getStorage()->findAll(1000, 0); // Get first 1000 profiles

        if (allProfiles.success) {
            for (const auto& profile : allProfiles.value) {
                if (profile.previousSlugs && !profile.previousSlugs->empty()) {
                    for (const auto& oldSlug : profile.previousSlugs.value()) {
                        if (oldSlug == requestedSlug) {
                            // Found a match! Issue 301 redirect to current slug
                            std::string redirectUrl = "/" + profile.slug;
                            res->writeStatus("301 Moved Permanently");
                            res->writeHeader("Location", redirectUrl);
                            res->writeHeader("Content-Type", "text/html");
                            res->end("<html><body><h1>301 Moved Permanently</h1><p>The profile has moved to <a href=\"" + redirectUrl + "\">" + redirectUrl + "</a></p></body></html>");

                            LOG_INFO("SEO redirect: " + requestedSlug + " -> " + profile.slug + " (profile ID: " + (profile.id ? profile.id.value() : "unknown") + ")");
                            return true;
                        }
                    }
                }
            }
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
            LOG_WARNING("Unauthorized compliance cleanup attempt with key: " + apiKey);
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
