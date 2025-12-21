#pragma once

#include <string>
#include <chrono>
#include <optional>
#include <vector>

namespace search_engine {
namespace storage {

enum class ProfileType {
    PERSON,        // Individual person profile
    BUSINESS       // Business/company profile
};

struct Profile {
    // Unique identifier (MongoDB ObjectId will be auto-generated)
    std::optional<std::string> id;

    // Core profile data - required fields (support UTF-8 for Persian and English)
    std::string slug;         // URL-friendly identifier (supports Persian and English characters)
    std::string name;         // Display name (supports full Unicode)

    // Profile metadata
    ProfileType type;         // PERSON or BUSINESS
    std::optional<std::string> bio;  // Optional short description (supports multilingual content)
    bool isPublic = true;     // Public visibility (default true for MVP)

    // URL history for SEO redirects (301 redirects when slug changes)
    std::optional<std::vector<std::string>> previousSlugs;  // Previous slugs for redirect tracking
    std::optional<std::chrono::system_clock::time_point> slugChangedAt;  // When slug was last changed

    // Timestamps
    std::chrono::system_clock::time_point createdAt;
};

} // namespace storage
} // namespace search_engine
