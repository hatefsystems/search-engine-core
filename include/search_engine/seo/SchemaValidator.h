#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace search_engine {
namespace seo {

/**
 * Validation result for JSON-LD schema
 */
struct ValidationResult {
    bool success;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;

    ValidationResult() : success(true) {}

    void addError(const std::string& error) {
        errors.push_back(error);
        success = false;
    }

    void addWarning(const std::string& warning) {
        warnings.push_back(warning);
    }

    bool hasErrors() const {
        return !errors.empty();
    }

    bool hasWarnings() const {
        return !warnings.empty();
    }
};

/**
 * Schema Validator - Validates JSON-LD structured data
 * 
 * Validates structured data against schema.org requirements:
 * - Checks for required fields
 * - Validates field types
 * - Validates URL and date formats
 * - Checks nested object structures
 */
class SchemaValidator {
public:
    /**
     * Validate Person schema
     * @param schema JSON object with Person schema
     * @return Validation result with errors and warnings
     */
    static ValidationResult validatePersonSchema(const nlohmann::json& schema);

    /**
     * Validate Organization schema
     * @param schema JSON object with Organization schema
     * @return Validation result with errors and warnings
     */
    static ValidationResult validateOrganizationSchema(const nlohmann::json& schema);

    /**
     * Validate generic schema structure
     * @param schema JSON object with schema
     * @return Validation result with errors and warnings
     */
    static ValidationResult validateSchemaStructure(const nlohmann::json& schema);

private:
    /**
     * Check if field exists in schema
     * @param schema JSON object
     * @param fieldName Field name to check
     * @return True if field exists and is not null
     */
    static bool hasField(const nlohmann::json& schema, const std::string& fieldName);

    /**
     * Check if field is a string
     * @param schema JSON object
     * @param fieldName Field name to check
     * @return True if field is a string
     */
    static bool isString(const nlohmann::json& schema, const std::string& fieldName);

    /**
     * Check if field is an object
     * @param schema JSON object
     * @param fieldName Field name to check
     * @return True if field is an object
     */
    static bool isObject(const nlohmann::json& schema, const std::string& fieldName);

    /**
     * Check if field is an array
     * @param schema JSON object
     * @param fieldName Field name to check
     * @return True if field is an array
     */
    static bool isArray(const nlohmann::json& schema, const std::string& fieldName);

    /**
     * Validate URL format
     * @param url URL string to validate
     * @return True if URL format is valid
     */
    static bool isValidUrl(const std::string& url);

    /**
     * Validate date format (ISO 8601)
     * @param date Date string to validate
     * @return True if date format is valid
     */
    static bool isValidDate(const std::string& date);

    /**
     * Validate PostalAddress schema
     * @param address JSON object with PostalAddress
     * @param result Validation result to add errors/warnings
     */
    static void validatePostalAddress(const nlohmann::json& address, ValidationResult& result);

    /**
     * Validate ContactPoint schema
     * @param contactPoint JSON object with ContactPoint
     * @param result Validation result to add errors/warnings
     */
    static void validateContactPoint(const nlohmann::json& contactPoint, ValidationResult& result);

    /**
     * Validate Occupation schema
     * @param occupation JSON object with Occupation
     * @param result Validation result to add errors/warnings
     */
    static void validateOccupation(const nlohmann::json& occupation, ValidationResult& result);
};

} // namespace seo
} // namespace search_engine
