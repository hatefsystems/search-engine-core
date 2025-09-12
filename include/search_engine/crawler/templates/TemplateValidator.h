#pragma once
#include "TemplateTypes.h"
#include <nlohmann/json.hpp>
#include <string>

namespace search_engine {
namespace crawler {
namespace templates {

struct ValidationResult {
    bool valid;
    std::string message;
};

inline std::string normalizeTemplateName(const std::string& name) {
    std::string normalized = name;
    // Trim whitespace
    normalized.erase(0, normalized.find_first_not_of(" \t\n\r"));
    normalized.erase(normalized.find_last_not_of(" \t\n\r") + 1);
    // Convert to lowercase for consistency
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
    return normalized;
}

inline bool isValidTemplateName(const std::string& name) {
    if (name.empty() || name.length() > 50) return false;
    // Allow alphanumeric, hyphens, underscores
    return std::all_of(name.begin(), name.end(), [](char c) {
        return std::isalnum(c) || c == '-' || c == '_';
    });
}

inline ValidationResult validateTemplateJson(const nlohmann::json& body) {
    if (!body.is_object()) return {false, "Body must be a JSON object"};
    
    // Validate name
    if (!body.contains("name") || !body["name"].is_string()) {
        return {false, "name is required and must be a string"};
    }
    std::string name = body["name"].get<std::string>();
    if (name.empty()) {
        return {false, "name cannot be empty"};
    }
    if (!isValidTemplateName(name)) {
        return {false, "name must be 1-50 characters, alphanumeric with hyphens/underscores only"};
    }

    // Validate config values with reasonable caps
    if (body.contains("config") && body["config"].is_object()) {
        const auto& cfg = body["config"];
        if (cfg.contains("maxPages")) {
            if (!cfg["maxPages"].is_number_integer()) {
                return {false, "config.maxPages must be an integer"};
            }
            int maxPages = cfg["maxPages"].get<int>();
            if (maxPages <= 0 || maxPages > 10000) {
                return {false, "config.maxPages must be between 1 and 10000"};
            }
        }
        if (cfg.contains("maxDepth")) {
            if (!cfg["maxDepth"].is_number_integer()) {
                return {false, "config.maxDepth must be an integer"};
            }
            int maxDepth = cfg["maxDepth"].get<int>();
            if (maxDepth <= 0 || maxDepth > 10) {
                return {false, "config.maxDepth must be between 1 and 10"};
            }
        }
        if (cfg.contains("politenessDelay")) {
            if (!cfg["politenessDelay"].is_number_integer()) {
                return {false, "config.politenessDelay must be an integer"};
            }
            int delay = cfg["politenessDelay"].get<int>();
            if (delay < 0 || delay > 60000) {
                return {false, "config.politenessDelay must be between 0 and 60000 ms"};
            }
        }
        // booleans are fine if present
    }

    if (body.contains("patterns") && body["patterns"].is_object()) {
        const auto& pat = body["patterns"];
        auto is_string_array = [](const nlohmann::json& j) {
            if (!j.is_array()) return false;
            for (const auto& v : j) if (!v.is_string()) return false;
            return true;
        };
        if (pat.contains("articleSelectors") && !is_string_array(pat["articleSelectors"]))
            return {false, "patterns.articleSelectors must be an array of strings"};
        if (pat.contains("titleSelectors") && !is_string_array(pat["titleSelectors"]))
            return {false, "patterns.titleSelectors must be an array of strings"};
        if (pat.contains("contentSelectors") && !is_string_array(pat["contentSelectors"]))
            return {false, "patterns.contentSelectors must be an array of strings"};
    }

    return {true, "ok"};
}

} // namespace templates
} // namespace crawler
} // namespace search_engine


