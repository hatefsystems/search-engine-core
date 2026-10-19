#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

namespace search_engine {
namespace skills {

/**
 * @brief Predefined skills organized by category
 */
class SkillsData {
public:
    // Technical Skills
    static const std::vector<std::string> TECHNICAL_SKILLS;
    
    // Business Skills
    static const std::vector<std::string> BUSINESS_SKILLS;
    
    // Creative Skills
    static const std::vector<std::string> CREATIVE_SKILLS;
    
    /**
     * @brief Get all skills across all categories
     * @return Vector of all skill names
     */
    static std::vector<std::string> getAllSkills();
    
    /**
     * @brief Get skills by category
     * @param category Category name (TECHNICAL, BUSINESS, CREATIVE)
     * @return Vector of skill names in that category
     */
    static std::vector<std::string> getSkillsByCategory(const std::string& category);
};

} // namespace skills
} // namespace search_engine
