#include "../../include/search_engine/storage/AuditLogger.h"
#include "../../include/Logger.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>

namespace search_engine {
namespace storage {

std::string AuditLogger::generateAuditId() {
    // Generate a unique ID using timestamp + random number
    auto now = std::chrono::system_clock::now();
    auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    
    std::stringstream ss;
    ss << "audit_" << nowMs << "_" << dis(gen);
    return ss.str();
}

std::string AuditLogger::profileToJsonString(const Profile& profile) {
    try {
        nlohmann::json j = {
            {"id", profile.id.value_or("")},
            {"slug", profile.slug},
            {"name", profile.name},
            {"type", profileTypeToString(profile.type)},
            {"isPublic", profile.isPublic}
        };
        
        if (profile.bio.has_value()) {
            j["bio"] = profile.bio.value();
        }
        
        return j.dump();
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to convert profile to JSON: " + std::string(e.what()));
        return "{}";
    }
}

void AuditLogger::logProfileCreate(
    const Profile& profile,
    const std::string& userId,
    const std::string& ipAddress,
    const std::string& userAgent,
    AuditStorage* storage) {
    
    if (!storage) {
        LOG_WARNING("AuditLogger::logProfileCreate called with null storage");
        return;
    }
    
    try {
        ProfileAuditLog log;
        log.id = generateAuditId();
        log.timestamp = std::chrono::system_clock::now();
        log.action = AuditAction::CREATE;
        log.resourceType = "profile";
        log.resourceId = profile.id.value_or("");
        log.userId = userId.empty() ? "anonymous" : userId;
        log.ipAddress = ipAddress;
        log.userAgent = userAgent;
        log.oldValue = "";  // No old value for create
        log.newValue = profileToJsonString(profile);
        log.reason = "Profile creation via API";
        log.sessionId = "";  // TODO: Add session tracking when auth is implemented
        log.apiVersion = "v1";
        log.isAutomated = false;
        
        auto result = storage->recordAudit(log);
        if (!result.success) {
            LOG_WARNING("Failed to record audit log for profile creation: " + result.message);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in logProfileCreate: " + std::string(e.what()));
    }
}

void AuditLogger::logProfileUpdate(
    const Profile& oldProfile,
    const Profile& newProfile,
    const std::string& userId,
    const std::string& ipAddress,
    const std::string& userAgent,
    AuditStorage* storage) {
    
    if (!storage) {
        LOG_WARNING("AuditLogger::logProfileUpdate called with null storage");
        return;
    }
    
    try {
        ProfileAuditLog log;
        log.id = generateAuditId();
        log.timestamp = std::chrono::system_clock::now();
        log.action = AuditAction::UPDATE;
        log.resourceType = "profile";
        log.resourceId = newProfile.id.value_or("");
        log.userId = userId.empty() ? "anonymous" : userId;
        log.ipAddress = ipAddress;
        log.userAgent = userAgent;
        log.oldValue = profileToJsonString(oldProfile);
        log.newValue = profileToJsonString(newProfile);
        log.reason = "Profile update via API";
        log.sessionId = "";  // TODO: Add session tracking when auth is implemented
        log.apiVersion = "v1";
        log.isAutomated = false;
        
        auto result = storage->recordAudit(log);
        if (!result.success) {
            LOG_WARNING("Failed to record audit log for profile update: " + result.message);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in logProfileUpdate: " + std::string(e.what()));
    }
}

void AuditLogger::logProfileDelete(
    const std::string& profileId,
    const std::string& userId,
    const std::string& ipAddress,
    const std::string& userAgent,
    AuditStorage* storage) {
    
    if (!storage) {
        LOG_WARNING("AuditLogger::logProfileDelete called with null storage");
        return;
    }
    
    try {
        ProfileAuditLog log;
        log.id = generateAuditId();
        log.timestamp = std::chrono::system_clock::now();
        log.action = AuditAction::DELETE;
        log.resourceType = "profile";
        log.resourceId = profileId;
        log.userId = userId.empty() ? "anonymous" : userId;
        log.ipAddress = ipAddress;
        log.userAgent = userAgent;
        log.oldValue = "";  // Profile already deleted, no old value available
        log.newValue = "";  // No new value for delete
        log.reason = "Profile deletion via API";
        log.sessionId = "";  // TODO: Add session tracking when auth is implemented
        log.apiVersion = "v1";
        log.isAutomated = false;
        
        auto result = storage->recordAudit(log);
        if (!result.success) {
            LOG_WARNING("Failed to record audit log for profile deletion: " + result.message);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in logProfileDelete: " + std::string(e.what()));
    }
}

void AuditLogger::logProfileView(
    const std::string& profileId,
    const std::string& viewerId,
    const std::string& ipAddress,
    const std::string& userAgent,
    AuditStorage* storage) {
    
    if (!storage) {
        LOG_WARNING("AuditLogger::logProfileView called with null storage");
        return;
    }
    
    try {
        ProfileAuditLog log;
        log.id = generateAuditId();
        log.timestamp = std::chrono::system_clock::now();
        log.action = AuditAction::VIEW;
        log.resourceType = "profile";
        log.resourceId = profileId;
        log.userId = viewerId.empty() ? "anonymous" : viewerId;
        log.ipAddress = ipAddress;
        log.userAgent = userAgent;
        log.oldValue = "";  // No old/new values for view
        log.newValue = "";
        log.reason = "Profile view via public API";
        log.sessionId = "";  // TODO: Add session tracking when auth is implemented
        log.apiVersion = "v1";
        log.isAutomated = false;
        
        auto result = storage->recordAudit(log);
        if (!result.success) {
            LOG_WARNING("Failed to record audit log for profile view: " + result.message);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in logProfileView: " + std::string(e.what()));
    }
}

} // namespace storage
} // namespace search_engine
