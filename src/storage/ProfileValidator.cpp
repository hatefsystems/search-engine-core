#include "../../include/search_engine/storage/ProfileValidator.h"
#include "../../include/search_engine/storage/ProfileStorage.h"
#include <regex>
#include <chrono>
#include <algorithm>

namespace search_engine {
namespace storage {

// Static member initialization
const std::vector<std::string> ProfileValidator::VALID_INDUSTRIES = {
    "Technology", "Healthcare", "Finance", "Education",
    "Manufacturing", "Retail", "Food", "Construction",
    "Real Estate", "Transportation", "Hospitality", "Media",
    "Entertainment", "Consulting", "Legal", "Marketing",
    "Agriculture", "Energy", "Telecommunications", "Other"
};

const std::vector<std::string> ProfileValidator::VALID_EXPERIENCE_LEVELS = {
    "Entry", "Mid", "Senior", "Executive"
};

const std::vector<std::string> ProfileValidator::VALID_COMPANY_SIZES = {
    "1-10", "11-50", "51-200", "201-1000", "1000+"
};

ValidationResult ProfileValidator::validate(const Profile& profile) {
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    
    // Required field: slug
    if (profile.slug.empty()) {
        errors.push_back("Slug is required");
    } else {
        // Validate slug format
        if (!isValidSlug(profile.slug)) {
            errors.push_back("Invalid slug format. Slug must contain only Persian letters, English letters, numbers, and hyphens.");
        }
        
        // Check slug length
        if (profile.slug.length() > 50) {
            errors.push_back("Slug exceeds maximum length of 50 characters");
        }
        
        // Check for double hyphens (warning, not error)
        if (profile.slug.find("--") != std::string::npos) {
            warnings.push_back("Slug contains consecutive hyphens (--) which may affect readability");
        }
    }
    
    // Required field: name
    if (profile.name.empty()) {
        errors.push_back("Name is required");
    } else if (profile.name.length() > 200) {
        errors.push_back("Name exceeds maximum length of 200 characters");
    }
    
    // Optional field: bio
    if (profile.bio.has_value()) {
        if (profile.bio.value().empty()) {
            warnings.push_back("Bio is empty; consider removing or adding content");
        } else if (profile.bio.value().length() > 500) {
            errors.push_back("Bio exceeds maximum length of 500 characters");
        }
    }
    
    return ValidationResult(!errors.empty() ? false : true, errors, warnings);
}

ValidationResult ProfileValidator::validatePersonFields(const PersonProfile& profile) {
    // First validate base profile
    ValidationResult baseResult = validate(profile);
    
    std::vector<std::string> errors = baseResult.errors;
    std::vector<std::string> warnings = baseResult.warnings;
    
    // Check profile type
    if (profile.type != ProfileType::PERSON) {
        errors.push_back("Profile type must be PERSON for PersonProfile");
    }
    
    // Validate experienceLevel if present
    if (profile.experienceLevel.has_value()) {
        const std::string& level = profile.experienceLevel.value();
        if (std::find(VALID_EXPERIENCE_LEVELS.begin(), VALID_EXPERIENCE_LEVELS.end(), level) 
            == VALID_EXPERIENCE_LEVELS.end()) {
            errors.push_back("Invalid experience level. Must be one of: Entry, Mid, Senior, Executive");
        }
    }
    
    // Validate email if present
    if (profile.email.has_value() && !profile.email.value().empty()) {
        if (!isValidEmail(profile.email.value())) {
            errors.push_back("Invalid email format");
        }
    }
    
    // Validate phone if present
    if (profile.phone.has_value() && !profile.phone.value().empty()) {
        if (!isValidPhone(profile.phone.value())) {
            errors.push_back("Invalid phone format. Use international format: +CountryCodeNumber (e.g., +989123456789)");
        }
    }
    
    // Validate URLs if present
    if (profile.linkedinUrl.has_value() && !profile.linkedinUrl.value().empty()) {
        if (!isValidUrl(profile.linkedinUrl.value())) {
            errors.push_back("Invalid LinkedIn URL format");
        }
    }
    
    if (profile.githubUrl.has_value() && !profile.githubUrl.value().empty()) {
        if (!isValidUrl(profile.githubUrl.value())) {
            errors.push_back("Invalid GitHub URL format");
        }
    }
    
    if (profile.portfolioUrl.has_value() && !profile.portfolioUrl.value().empty()) {
        if (!isValidUrl(profile.portfolioUrl.value())) {
            errors.push_back("Invalid portfolio URL format");
        }
    }
    
    // Validate skills
    if (profile.skills.empty()) {
        warnings.push_back("No skills listed; consider adding skills to improve profile visibility");
    } else if (profile.skills.size() > 50) {
        warnings.push_back("Large number of skills (" + std::to_string(profile.skills.size()) + 
                          "); consider focusing on key skills");
    }
    
    return ValidationResult(errors.empty(), errors, warnings);
}

ValidationResult ProfileValidator::validateBusinessFields(const BusinessProfile& profile) {
    // First validate base profile
    ValidationResult baseResult = validate(profile);
    
    std::vector<std::string> errors = baseResult.errors;
    std::vector<std::string> warnings = baseResult.warnings;
    
    // Check profile type
    if (profile.type != ProfileType::BUSINESS) {
        errors.push_back("Profile type must be BUSINESS for BusinessProfile");
    }
    
    // Required field for business: companyName
    if (!profile.companyName.has_value() || profile.companyName.value().empty()) {
        errors.push_back("Company name is required for business profiles");
    } else if (profile.companyName.value().length() > 200) {
        errors.push_back("Company name exceeds maximum length of 200 characters");
    }
    
    // Validate industry
    if (profile.industry.has_value() && !profile.industry.value().empty()) {
        const std::string& industry = profile.industry.value();
        if (std::find(VALID_INDUSTRIES.begin(), VALID_INDUSTRIES.end(), industry) 
            == VALID_INDUSTRIES.end()) {
            errors.push_back("Invalid industry category. Must be one of: Technology, Healthcare, Finance, Education, Manufacturing, Retail, Food, Construction, Real Estate, Transportation, Hospitality, Media, Entertainment, Consulting, Legal, Marketing, Agriculture, Energy, Telecommunications, Other");
        }
    } else {
        warnings.push_back("Industry not specified; consider adding for better categorization");
    }
    
    // Validate company size
    if (profile.companySize.has_value() && !profile.companySize.value().empty()) {
        const std::string& size = profile.companySize.value();
        if (std::find(VALID_COMPANY_SIZES.begin(), VALID_COMPANY_SIZES.end(), size) 
            == VALID_COMPANY_SIZES.end()) {
            errors.push_back("Invalid company size. Must be one of: 1-10, 11-50, 51-200, 201-1000, 1000+");
        }
    }
    
    // Validate founded year
    if (profile.foundedYear.has_value()) {
        int year = profile.foundedYear.value();
        int currentYear = getCurrentYear();
        
        if (year < 1800) {
            errors.push_back("Founded year must be 1800 or later");
        } else if (year > currentYear + 1) {
            errors.push_back("Founded year cannot be more than 1 year in the future");
        }
    }
    
    // Validate business email if present
    if (profile.businessEmail.has_value() && !profile.businessEmail.value().empty()) {
        if (!isValidEmail(profile.businessEmail.value())) {
            errors.push_back("Invalid business email format");
        }
    }
    
    // Validate business phone if present
    if (profile.businessPhone.has_value() && !profile.businessPhone.value().empty()) {
        if (!isValidPhone(profile.businessPhone.value())) {
            errors.push_back("Invalid business phone format. Use international format: +CountryCodeNumber");
        }
    }
    
    // Validate website URL if present
    if (profile.website.has_value() && !profile.website.value().empty()) {
        if (!isValidUrl(profile.website.value())) {
            errors.push_back("Invalid website URL format");
        }
    }
    
    // Validate services
    if (profile.services.empty()) {
        warnings.push_back("No services listed; consider adding services to describe your business");
    } else if (profile.services.size() > 50) {
        warnings.push_back("Large number of services (" + std::to_string(profile.services.size()) + 
                          "); consider focusing on key services");
    }
    
    return ValidationResult(errors.empty(), errors, warnings);
}

bool ProfileValidator::isValidSlug(const std::string& slug) {
    // Delegate to ProfileStorage for consistency
    return ProfileStorage::isValidSlug(slug);
}

bool ProfileValidator::isValidEmail(const std::string& email) {
    if (email.empty() || email.length() > 254) {
        return false;
    }
    
    // RFC 5322 simplified regex for email validation
    static const std::regex emailRegex(
        R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})"
    );
    
    return std::regex_match(email, emailRegex);
}

bool ProfileValidator::isValidPhone(const std::string& phone) {
    if (phone.empty()) {
        return true; // Empty is valid (optional field)
    }
    
    // International format: +CountryCode(1-4 digits) + Number(6-14 digits)
    // Examples: +989123456789, +14155551234, +442071234567
    static const std::regex phoneRegex(R"(^\+\d{1,4}\d{6,14}$)");
    
    return std::regex_match(phone, phoneRegex);
}

bool ProfileValidator::isValidUrl(const std::string& url) {
    if (url.empty() || url.length() > 2048) {
        return false;
    }
    
    // Basic URL validation (http:// or https://)
    static const std::regex urlRegex(
        R"(^https?://[a-zA-Z0-9\-._~:/?#\[\]@!$&'()*+,;=%]+$)"
    );
    
    return std::regex_match(url, urlRegex);
}

int ProfileValidator::getCurrentYear() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::localtime(&time);
    return tm.tm_year + 1900;
}

} // namespace storage
} // namespace search_engine
