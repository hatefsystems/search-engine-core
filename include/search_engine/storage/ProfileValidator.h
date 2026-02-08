#pragma once

#include "Profile.h"
#include <string>
#include <vector>
#include <regex>

namespace search_engine {
namespace storage {

/**
 * @brief Validation result containing success status, errors, and warnings
 */
struct ValidationResult {
    bool isValid;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    
    ValidationResult() : isValid(true) {}
    
    explicit ValidationResult(bool valid) : isValid(valid) {}
    
    ValidationResult(bool valid, const std::vector<std::string>& errs, const std::vector<std::string>& warns = {})
        : isValid(valid), errors(errs), warnings(warns) {}
};

/**
 * @brief ProfileValidator - Central validation engine for profiles
 * 
 * Provides comprehensive validation for Profile, PersonProfile, and BusinessProfile
 * with detailed error messages and warnings.
 */
class ProfileValidator {
public:
    /**
     * @brief Validate a base Profile
     * @param profile Profile to validate
     * @return ValidationResult with errors and warnings
     */
    static ValidationResult validate(const Profile& profile);
    
    /**
     * @brief Validate PersonProfile specific fields
     * @param profile PersonProfile to validate
     * @return ValidationResult with errors and warnings
     */
    static ValidationResult validatePersonFields(const PersonProfile& profile);
    
    /**
     * @brief Validate BusinessProfile specific fields
     * @param profile BusinessProfile to validate
     * @return ValidationResult with errors and warnings
     */
    static ValidationResult validateBusinessFields(const BusinessProfile& profile);
    
    // Field-specific validation helpers
    
    /**
     * @brief Validate slug format (Persian + English + numbers + hyphens)
     * @param slug Slug to validate
     * @return true if valid, false otherwise
     */
    static bool isValidSlug(const std::string& slug);
    
    /**
     * @brief Validate email format
     * @param email Email to validate
     * @return true if valid, false otherwise
     */
    static bool isValidEmail(const std::string& email);
    
    /**
     * @brief Validate phone number (international format: +CountryCodeXXXXXXXXXX)
     * @param phone Phone number to validate
     * @return true if valid, false otherwise
     */
    static bool isValidPhone(const std::string& phone);
    
    /**
     * @brief Validate URL format
     * @param url URL to validate
     * @return true if valid, false otherwise
     */
    static bool isValidUrl(const std::string& url);
    
    /**
     * @brief Get current year for date validation
     * @return Current year
     */
    static int getCurrentYear();
    
private:
    // Industry categories allowed for business profiles
    static const std::vector<std::string> VALID_INDUSTRIES;
    
    // Experience levels allowed for person profiles
    static const std::vector<std::string> VALID_EXPERIENCE_LEVELS;
    
    // Company sizes allowed for business profiles
    static const std::vector<std::string> VALID_COMPANY_SIZES;
};

} // namespace storage
} // namespace search_engine
