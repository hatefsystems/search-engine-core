#pragma once

#include <string>
#include <chrono>
#include <optional>
#include <vector>
#include <stdexcept>

namespace search_engine {
namespace storage {

enum class ProfileType {
    PERSON,        // Individual person profile
    BUSINESS       // Business/company profile
};

// Type conversion helpers for API/database
std::string profileTypeToString(ProfileType type);
ProfileType stringToProfileType(const std::string& str);

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

    // Validation method
    bool isValid() const;
};

// PersonProfile extends Profile with person-specific fields
struct PersonProfile : public Profile {
    // Professional Info
    std::optional<std::string> title;              // Job title (e.g., "Software Engineer")
    std::optional<std::string> company;            // Current company
    std::vector<std::string> skills;               // Skills list
    std::optional<std::string> experienceLevel;    // "Entry", "Mid", "Senior", "Executive"

    // Education
    std::optional<std::string> education;          // Degree/field of study
    std::optional<std::string> school;             // University/school name

    // Social Links
    std::optional<std::string> linkedinUrl;        // LinkedIn profile
    std::optional<std::string> githubUrl;          // GitHub profile
    std::optional<std::string> portfolioUrl;       // Personal website/portfolio

    // Contact (will be encrypted in later tasks)
    std::optional<std::string> email;              // Contact email
    std::optional<std::string> phone;              // Phone number

    // Validation method
    bool isValid() const;
};

// BusinessProfile extends Profile with business-specific fields
struct BusinessProfile : public Profile {
    // Company Info
    std::optional<std::string> companyName;        // Official company name
    std::optional<std::string> industry;           // Industry category
    std::optional<std::string> companySize;        // "1-10", "11-50", "51-200", "201-1000", "1000+"
    std::optional<int> foundedYear;                // Year founded

    // Location
    std::optional<std::string> address;            // Business address
    std::optional<std::string> city;               // City
    std::optional<std::string> country;            // Country

    // Business Details
    std::optional<std::string> website;            // Company website
    std::optional<std::string> description;        // Company description
    std::vector<std::string> services;             // Services offered

    // Contact (will be encrypted in later tasks)
    std::optional<std::string> businessEmail;      // Business email
    std::optional<std::string> businessPhone;      // Business phone

    // Validation method
    bool isValid() const;
};

} // namespace storage
} // namespace search_engine
