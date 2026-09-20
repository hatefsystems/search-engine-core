#pragma once

#include "../storage/Profile.h"
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <ctime>

namespace search_engine::profile {

// API serialization; public callers must first apply publicPersonProfile().
inline nlohmann::json profileToJson(const search_engine::storage::Profile& profile) {
    nlohmann::json json;

    // Required fields
    json["version"] = profile.version;
    json["id"] = profile.id.value_or("");
    json["slug"] = profile.slug;
    json["name"] = profile.name;
    json["type"] = (profile.type == storage::ProfileType::PERSON ? "PERSON" : profile.type == storage::ProfileType::BUSINESS ? "BUSINESS" : "UNKNOWN");
    json["isPublic"] = profile.isPublic;

    // Optional fields
    if (profile.bio) {
        json["bio"] = profile.bio.value();
    }

    // Format createdAt timestamp as ISO 8601 string
    auto time_t = std::chrono::system_clock::to_time_t(profile.createdAt);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    json["createdAt"] = ss.str();

    // Format updatedAt timestamp as ISO 8601 string if present
    if (profile.updatedAt) {
        auto updated_time_t = std::chrono::system_clock::to_time_t(profile.updatedAt.value());
        std::stringstream updated_ss;
        updated_ss << std::put_time(std::gmtime(&updated_time_t), "%Y-%m-%dT%H:%M:%SZ");
        json["updatedAt"] = updated_ss.str();
    }

    return json;
}

inline nlohmann::json personProfileToJson(const search_engine::storage::PersonProfile& profile) {
    // Start with base profile fields
    nlohmann::json json = profileToJson(static_cast<const search_engine::storage::Profile&>(profile));

    // Add PersonProfile-specific fields
    if (profile.displayName) {
        json["displayName"] = profile.displayName.value();
    }
    if (profile.englishName) {
        json["englishName"] = profile.englishName.value();
    }
    if (profile.tagline) {
        json["tagline"] = profile.tagline.value();
    }
    if (profile.professionalSummary) {
        json["professionalSummary"] = profile.professionalSummary.value();
    }
    if (profile.location) {
        json["location"] = profile.location.value();
    }
    if (!profile.languages.empty()) {
        json["languages"] = profile.languages;
    }
    if (profile.avatarUrl) {
        json["avatarUrl"] = profile.avatarUrl.value();
    }
    if (profile.coverImageUrl) {
        json["coverImageUrl"] = profile.coverImageUrl.value();
    }
    if (profile.availabilityStatus) {
        json["availabilityStatus"] = profile.availabilityStatus.value();
    }
    if (profile.title) {
        json["title"] = profile.title.value();
    }
    if (profile.company) {
        json["company"] = profile.company.value();
    }
    if (!profile.skills.empty()) {
        json["skills"] = profile.skills;
    }
    if (!profile.skillsWithLevel.empty()) {
        nlohmann::json skillsArray = nlohmann::json::array();
        for (const auto& skill : profile.skillsWithLevel) {
            nlohmann::json skillJson = {
                {"name", skill.name},
                {"level", skill.level}
            };
            if (!skill.category.empty()) {
                skillJson["category"] = skill.category;
            }
            skillsArray.push_back(skillJson);
        }
        json["skillsWithLevel"] = skillsArray;
    }
    if (profile.experienceLevel) {
        json["experienceLevel"] = profile.experienceLevel.value();
    }
    if (profile.education) {
        json["education"] = profile.education.value();
    }
    if (profile.school) {
        json["school"] = profile.school.value();
    }
    if (profile.linkedinUrl) {
        json["linkedinUrl"] = profile.linkedinUrl.value();
    }
    if (profile.githubUrl) {
        json["githubUrl"] = profile.githubUrl.value();
    }
    if (profile.portfolioUrl) {
        json["portfolioUrl"] = profile.portfolioUrl.value();
    }
    if (profile.email) {
        json["email"] = profile.email.value();
    }
    if (profile.phone) {
        json["phone"] = profile.privacy.showPhone ? profile.phone.value() : "";
    }

    // Privacy settings
    json["privacy"] = nlohmann::json{
        {"showEmail", profile.privacy.showEmail},
        {"showPhone", profile.privacy.showPhone},
        {"showLocation", profile.privacy.showLocation},
        {"showAvailability", profile.privacy.showAvailability}
    };

    return json;
}


} // namespace search_engine::profile
