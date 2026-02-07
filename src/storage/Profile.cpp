#include "../../include/search_engine/storage/Profile.h"
#include "../../include/search_engine/storage/ProfileStorage.h"

namespace search_engine {
namespace storage {

// Type conversion helper - converts ProfileType to string
std::string profileTypeToString(ProfileType type) {
    switch (type) {
        case ProfileType::PERSON: return "PERSON";
        case ProfileType::BUSINESS: return "BUSINESS";
        default: return "UNKNOWN";
    }
}

// Type conversion helper - converts string to ProfileType (throws on invalid)
ProfileType stringToProfileType(const std::string& str) {
    if (str == "PERSON") return ProfileType::PERSON;
    if (str == "BUSINESS") return ProfileType::BUSINESS;
    throw std::invalid_argument("Invalid profile type: " + str);
}

// Profile validation
bool Profile::isValid() const {
    // slug and name are required and must be non-empty
    if (slug.empty() || name.empty()) {
        return false;
    }

    // Validate slug format using ProfileStorage helper
    if (!ProfileStorage::isValidSlug(slug)) {
        return false;
    }

    // If bio is present, check length constraint (max 500 characters)
    if (bio.has_value() && bio.value().length() > 500) {
        return false;
    }

    return true;
}

// PersonProfile validation
bool PersonProfile::isValid() const {
    // Check base Profile validation
    if (!Profile::isValid()) {
        return false;
    }

    // Type must be PERSON
    if (type != ProfileType::PERSON) {
        return false;
    }

    // Validate experienceLevel if present (optional field)
    if (experienceLevel.has_value()) {
        const std::string& level = experienceLevel.value();
        if (level != "Entry" && level != "Mid" && level != "Senior" && level != "Executive") {
            return false;
        }
    }

    // All extended fields are optional, so no further validation needed
    return true;
}

// BusinessProfile validation
bool BusinessProfile::isValid() const {
    // Check base Profile validation
    if (!Profile::isValid()) {
        return false;
    }

    // Type must be BUSINESS
    if (type != ProfileType::BUSINESS) {
        return false;
    }

    // Validate companySize if present (optional field)
    if (companySize.has_value()) {
        const std::string& size = companySize.value();
        if (size != "1-10" && size != "11-50" && size != "51-200" && 
            size != "201-1000" && size != "1000+") {
            return false;
        }
    }

    // Validate foundedYear if present (should be reasonable)
    if (foundedYear.has_value()) {
        int year = foundedYear.value();
        if (year < 1800 || year > 2100) {
            return false;
        }
    }

    // All extended fields are optional, so no further validation needed
    return true;
}

} // namespace storage
} // namespace search_engine
