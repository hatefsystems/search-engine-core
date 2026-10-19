#include "../../include/search_engine/common/SkillNormalizer.h"
#include "../../include/search_engine/skills/SkillsData.h"

namespace search_engine {
namespace skills {

std::string SkillNormalizer::normalize(const std::string& skill) {
    return trim(skill);
}

std::string SkillNormalizer::getCategory(const std::string& skill) {
    std::string normalizedSkill = normalize(skill);
    
    // Check in technical skills
    const auto& technicalSkills = SkillsData::TECHNICAL_SKILLS;
    if (std::find(technicalSkills.begin(), technicalSkills.end(), normalizedSkill) != technicalSkills.end()) {
        return "TECHNICAL";
    }
    
    // Check in business skills
    const auto& businessSkills = SkillsData::BUSINESS_SKILLS;
    if (std::find(businessSkills.begin(), businessSkills.end(), normalizedSkill) != businessSkills.end()) {
        return "BUSINESS";
    }
    
    // Check in creative skills
    const auto& creativeSkills = SkillsData::CREATIVE_SKILLS;
    if (std::find(creativeSkills.begin(), creativeSkills.end(), normalizedSkill) != creativeSkills.end()) {
        return "CREATIVE";
    }
    
    return "OTHER";
}

std::vector<std::string> SkillNormalizer::autocomplete(const std::string& query, int limit) {
    std::vector<std::string> results;
    std::string lowerQuery = toLower(query);
    
    if (lowerQuery.empty()) {
        return results;
    }
    
    auto allSkills = SkillsData::getAllSkills();
    
    // Find skills that contain the query (case-insensitive)
    for (const auto& skill : allSkills) {
        std::string lowerSkill = toLower(skill);
        
        // Prioritize skills that start with query
        if (lowerSkill.find(lowerQuery) == 0) {
            results.push_back(skill);
        }
    }
    
    // Add skills that contain query but don't start with it
    for (const auto& skill : allSkills) {
        std::string lowerSkill = toLower(skill);
        
        if (lowerSkill.find(lowerQuery) != std::string::npos && 
            lowerSkill.find(lowerQuery) != 0) {
            results.push_back(skill);
        }
    }
    
    // Limit results
    if (results.size() > static_cast<size_t>(limit)) {
        results.resize(limit);
    }
    
    return results;
}

bool SkillNormalizer::exists(const std::string& skill) {
    std::string normalizedSkill = normalize(skill);
    auto allSkills = SkillsData::getAllSkills();
    
    return std::find(allSkills.begin(), allSkills.end(), normalizedSkill) != allSkills.end();
}

std::string SkillNormalizer::toLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

std::string SkillNormalizer::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) {
        return "";
    }
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}

} // namespace skills
} // namespace search_engine
