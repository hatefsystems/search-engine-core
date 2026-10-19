#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <cctype>

namespace search_engine {
namespace skills {

/**
 * @brief Utility class for normalizing and autocompleting skills
 */
class SkillNormalizer {
public:
    /**
     * @brief Normalize a skill name (lowercase, trim whitespace)
     * @param skill Skill name to normalize
     * @return Normalized skill name
     */
    static std::string normalize(const std::string& skill);
    
    /**
     * @brief Get the category for a given skill
     * @param skill Skill name
     * @return Category name (TECHNICAL, BUSINESS, CREATIVE, or OTHER)
     */
    static std::string getCategory(const std::string& skill);
    
    /**
     * @brief Autocomplete skill names based on query
     * @param query Search query
     * @param limit Maximum number of results (default 10)
     * @return Vector of matching skill names
     */
    static std::vector<std::string> autocomplete(const std::string& query, int limit = 10);
    
    /**
     * @brief Check if a skill exists in predefined list
     * @param skill Skill name
     * @return true if skill exists, false otherwise
     */
    static bool exists(const std::string& skill);

private:
    static std::string toLower(const std::string& str);
    static std::string trim(const std::string& str);
};

} // namespace skills
} // namespace search_engine
