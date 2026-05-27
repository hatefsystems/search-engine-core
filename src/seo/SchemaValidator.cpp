#include "../include/search_engine/seo/SchemaValidator.h"
#include "../include/Logger.h"
#include <regex>

namespace search_engine {
namespace seo {

ValidationResult SchemaValidator::validatePersonSchema(const nlohmann::json& schema) {
    LOG_DEBUG("Validating Person schema");
    
    ValidationResult result;

    // Check basic schema structure
    auto structureResult = validateSchemaStructure(schema);
    result.errors.insert(result.errors.end(), structureResult.errors.begin(), structureResult.errors.end());
    result.warnings.insert(result.warnings.end(), structureResult.warnings.begin(), structureResult.warnings.end());

    if (structureResult.hasErrors()) {
        result.success = false;
        return result;
    }

    // Check @type is Person
    if (hasField(schema, "@type")) {
        std::string type = schema["@type"].get<std::string>();
        if (type != "Person") {
            result.addError("@type must be 'Person' for Person schema");
        }
    }

    // Check required fields for Person
    if (!hasField(schema, "name")) {
        result.addError("Person schema must have 'name' field");
    } else if (!isString(schema, "name")) {
        result.addError("'name' must be a string");
    }

    // Validate optional URL field
    if (hasField(schema, "url")) {
        if (isString(schema, "url")) {
            std::string url = schema["url"].get<std::string>();
            if (!isValidUrl(url)) {
                result.addWarning("'url' field has invalid URL format");
            }
        } else {
            result.addError("'url' must be a string");
        }
    }

    // Validate sameAs array
    if (hasField(schema, "sameAs")) {
        if (isArray(schema, "sameAs")) {
            for (const auto& url : schema["sameAs"]) {
                if (url.is_string()) {
                    std::string urlStr = url.get<std::string>();
                    if (!isValidUrl(urlStr)) {
                        result.addWarning("'sameAs' array contains invalid URL: " + urlStr);
                    }
                }
            }
        } else if (isString(schema, "sameAs")) {
            // Single URL as string is also valid
            std::string url = schema["sameAs"].get<std::string>();
            if (!isValidUrl(url)) {
                result.addWarning("'sameAs' contains invalid URL");
            }
        }
    }

    // Validate hasOccupation if present
    if (hasField(schema, "hasOccupation")) {
        if (isObject(schema, "hasOccupation")) {
            validateOccupation(schema["hasOccupation"], result);
        } else {
            result.addError("'hasOccupation' must be an object");
        }
    }

    // Validate knowsAbout if present
    if (hasField(schema, "knowsAbout")) {
        if (!isArray(schema, "knowsAbout")) {
            result.addWarning("'knowsAbout' should be an array");
        }
    }

    LOG_DEBUG("Person schema validation completed with " + 
              std::to_string(result.errors.size()) + " errors and " +
              std::to_string(result.warnings.size()) + " warnings");

    return result;
}

ValidationResult SchemaValidator::validateOrganizationSchema(const nlohmann::json& schema) {
    LOG_DEBUG("Validating Organization schema");
    
    ValidationResult result;

    // Check basic schema structure
    auto structureResult = validateSchemaStructure(schema);
    result.errors.insert(result.errors.end(), structureResult.errors.begin(), structureResult.errors.end());
    result.warnings.insert(result.warnings.end(), structureResult.warnings.begin(), structureResult.warnings.end());

    if (structureResult.hasErrors()) {
        result.success = false;
        return result;
    }

    // Check @type is Organization or LocalBusiness
    if (hasField(schema, "@type")) {
        std::string type = schema["@type"].get<std::string>();
        if (type != "Organization" && type != "LocalBusiness") {
            result.addError("@type must be 'Organization' or 'LocalBusiness' for Organization schema");
        }
    }

    // Check required fields for Organization
    if (!hasField(schema, "name")) {
        result.addError("Organization schema must have 'name' field");
    } else if (!isString(schema, "name")) {
        result.addError("'name' must be a string");
    }

    // Validate optional URL field
    if (hasField(schema, "url")) {
        if (isString(schema, "url")) {
            std::string url = schema["url"].get<std::string>();
            if (!isValidUrl(url)) {
                result.addWarning("'url' field has invalid URL format");
            }
        } else {
            result.addError("'url' must be a string");
        }
    }

    // Validate address if present
    if (hasField(schema, "address")) {
        if (isObject(schema, "address")) {
            validatePostalAddress(schema["address"], result);
        } else if (!isString(schema, "address")) {
            result.addError("'address' must be an object (PostalAddress) or string");
        }
    }

    // Validate contactPoint if present
    if (hasField(schema, "contactPoint")) {
        if (isObject(schema, "contactPoint")) {
            validateContactPoint(schema["contactPoint"], result);
        } else {
            result.addError("'contactPoint' must be an object");
        }
    }

    // Validate foundingDate if present
    if (hasField(schema, "foundingDate")) {
        if (isString(schema, "foundingDate")) {
            std::string date = schema["foundingDate"].get<std::string>();
            if (!isValidDate(date)) {
                result.addWarning("'foundingDate' has invalid date format (should be ISO 8601)");
            }
        } else {
            result.addError("'foundingDate' must be a string");
        }
    }

    LOG_DEBUG("Organization schema validation completed with " + 
              std::to_string(result.errors.size()) + " errors and " +
              std::to_string(result.warnings.size()) + " warnings");

    return result;
}

ValidationResult SchemaValidator::validateSchemaStructure(const nlohmann::json& schema) {
    ValidationResult result;

    // Check if schema is an object
    if (!schema.is_object()) {
        result.addError("Schema must be a JSON object");
        return result;
    }

    // Check for required @context
    if (!hasField(schema, "@context")) {
        result.addError("Schema must have '@context' field");
    } else if (!isString(schema, "@context")) {
        result.addError("'@context' must be a string");
    } else {
        std::string context = schema["@context"].get<std::string>();
        if (context.find("schema.org") == std::string::npos) {
            result.addWarning("'@context' should reference schema.org");
        }
    }

    // Check for required @type
    if (!hasField(schema, "@type")) {
        result.addError("Schema must have '@type' field");
    } else if (!isString(schema, "@type")) {
        result.addError("'@type' must be a string");
    }

    return result;
}

bool SchemaValidator::hasField(const nlohmann::json& schema, const std::string& fieldName) {
    return schema.contains(fieldName) && !schema[fieldName].is_null();
}

bool SchemaValidator::isString(const nlohmann::json& schema, const std::string& fieldName) {
    return hasField(schema, fieldName) && schema[fieldName].is_string();
}

bool SchemaValidator::isObject(const nlohmann::json& schema, const std::string& fieldName) {
    return hasField(schema, fieldName) && schema[fieldName].is_object();
}

bool SchemaValidator::isArray(const nlohmann::json& schema, const std::string& fieldName) {
    return hasField(schema, fieldName) && schema[fieldName].is_array();
}

bool SchemaValidator::isValidUrl(const std::string& url) {
    // Basic URL validation
    std::regex urlRegex(
        R"(^(https?:\/\/)([a-zA-Z0-9-]+\.)+[a-zA-Z]{2,}(:[0-9]+)?(\/.*)?$)",
        std::regex::icase
    );
    return std::regex_match(url, urlRegex);
}

bool SchemaValidator::isValidDate(const std::string& date) {
    // Basic ISO 8601 date validation (YYYY, YYYY-MM, or YYYY-MM-DD)
    std::regex dateRegex(R"(^\d{4}(-\d{2}(-\d{2})?)?$)");
    return std::regex_match(date, dateRegex);
}

void SchemaValidator::validatePostalAddress(const nlohmann::json& address, ValidationResult& result) {
    // Check @type
    if (hasField(address, "@type")) {
        std::string type = address["@type"].get<std::string>();
        if (type != "PostalAddress") {
            result.addWarning("PostalAddress @type should be 'PostalAddress'");
        }
    }

    // Optional but recommended fields
    if (!hasField(address, "addressLocality") && !hasField(address, "addressCountry")) {
        result.addWarning("PostalAddress should have at least 'addressLocality' or 'addressCountry'");
    }
}

void SchemaValidator::validateContactPoint(const nlohmann::json& contactPoint, ValidationResult& result) {
    // Check @type
    if (hasField(contactPoint, "@type")) {
        std::string type = contactPoint["@type"].get<std::string>();
        if (type != "ContactPoint") {
            result.addWarning("ContactPoint @type should be 'ContactPoint'");
        }
    }

    // Check for at least one contact method
    if (!hasField(contactPoint, "telephone") && !hasField(contactPoint, "email")) {
        result.addWarning("ContactPoint should have at least 'telephone' or 'email'");
    }
}

void SchemaValidator::validateOccupation(const nlohmann::json& occupation, ValidationResult& result) {
    // Check @type
    if (hasField(occupation, "@type")) {
        std::string type = occupation["@type"].get<std::string>();
        if (type != "Occupation") {
            result.addWarning("Occupation @type should be 'Occupation'");
        }
    }

    // Check for required name field
    if (!hasField(occupation, "name")) {
        result.addError("Occupation must have 'name' field");
    }
}

} // namespace seo
} // namespace search_engine
