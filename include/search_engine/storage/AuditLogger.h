#pragma once

#include "ProfileAuditLog.h"
#include "Profile.h"
#include <string>
#include <memory>

namespace search_engine {
namespace storage {

/**
 * @brief Static helper for logging profile audit events
 * 
 * Provides convenient methods to log profile CRUD and view operations.
 * Handles building audit entries and calling AuditStorage.
 */
class AuditLogger {
public:
    /**
     * @brief Log profile creation
     * @param profile Profile that was created
     * @param userId User who created it (or "anonymous")
     * @param ipAddress IP address of requester
     * @param userAgent User agent string
     * @param storage AuditStorage instance to use
     */
    static void logProfileCreate(
        const Profile& profile,
        const std::string& userId,
        const std::string& ipAddress,
        const std::string& userAgent,
        AuditStorage* storage
    );
    
    /**
     * @brief Log profile update
     * @param oldProfile Profile before update
     * @param newProfile Profile after update
     * @param userId User who updated it (or "anonymous")
     * @param ipAddress IP address of requester
     * @param userAgent User agent string
     * @param storage AuditStorage instance to use
     */
    static void logProfileUpdate(
        const Profile& oldProfile,
        const Profile& newProfile,
        const std::string& userId,
        const std::string& ipAddress,
        const std::string& userAgent,
        AuditStorage* storage
    );
    
    /**
     * @brief Log profile deletion
     * @param profileId Profile ID that was deleted
     * @param userId User who deleted it (or "anonymous")
     * @param ipAddress IP address of requester
     * @param userAgent User agent string
     * @param storage AuditStorage instance to use
     */
    static void logProfileDelete(
        const std::string& profileId,
        const std::string& userId,
        const std::string& ipAddress,
        const std::string& userAgent,
        AuditStorage* storage
    );
    
    /**
     * @brief Log profile view
     * @param profileId Profile ID that was viewed
     * @param viewerId User who viewed it (or "anonymous")
     * @param ipAddress IP address of requester
     * @param userAgent User agent string
     * @param storage AuditStorage instance to use
     */
    static void logProfileView(
        const std::string& profileId,
        const std::string& viewerId,
        const std::string& ipAddress,
        const std::string& userAgent,
        AuditStorage* storage
    );

private:
    /**
     * @brief Convert profile to JSON string for audit log
     * @param profile Profile to convert
     * @return JSON string representation
     */
    static std::string profileToJsonString(const Profile& profile);
    
    /**
     * @brief Generate a unique audit log ID
     * @return Unique ID string
     */
    static std::string generateAuditId();
};

} // namespace storage
} // namespace search_engine
