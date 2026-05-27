#pragma once

#include <string>
#include <chrono>
#include <optional>
#include <vector>

namespace search_engine {
namespace storage {

// Privacy level for link blocks
enum class LinkPrivacy {
    PUBLIC,    // Link is visible to everyone and appears on profile
    HIDDEN,    // Link is hidden but redirect still works
    DISABLED   // Link is disabled (redirect returns 404)
};

// Type conversion helpers for API/database
std::string linkPrivacyToString(LinkPrivacy privacy);
LinkPrivacy stringToLinkPrivacy(const std::string& str);

/**
 * @brief Link block for profile pages
 * 
 * Represents a clickable link on a user's profile (e.g., social media, website, portfolio).
 * Each link has:
 * - URL destination (validated)
 * - Display metadata (title, description, icon)
 * - Privacy controls
 * - Analytics capability (privacy-respecting)
 */
struct LinkBlock {
    // Unique identifier (MongoDB ObjectId auto-generated)
    std::optional<std::string> id;
    
    // Core data - required fields
    std::string profileId;        // Profile this link belongs to
    std::string url;              // Destination URL (http/https only, validated)
    
    // Display metadata
    std::string title;                          // Link title (max 200 chars)
    std::optional<std::string> description;     // Optional description (max 500 chars)
    std::optional<std::string> iconUrl;         // Optional icon/favicon URL
    
    // Behavior and privacy
    bool isActive = true;                       // Active links can be clicked
    LinkPrivacy privacy = LinkPrivacy::PUBLIC;  // Visibility and analytics
    
    // Organization
    std::vector<std::string> tags;              // Optional tags for grouping
    int sortOrder = 0;                          // Display order (lower = higher)
    
    // Timestamps
    std::chrono::system_clock::time_point createdAt;
    std::optional<std::chrono::system_clock::time_point> updatedAt;
    
    // Validation method
    bool isValid() const;
};

} // namespace storage
} // namespace search_engine
