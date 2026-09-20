#pragma once

#include "../storage/Profile.h"
#include <nlohmann/json.hpp>
#include <utility>

namespace search_engine::profile {

inline bool isPublicImageUrl(const std::string& url) {
    if (url.empty() || url.find_first_of("\\\r\n\t") != std::string::npos) return false;
    return (url.front() == '/' && (url.size() == 1 || url[1] != '/')) ||
        url.rfind("https://", 0) == 0 || url.rfind("http://", 0) == 0;
}

// Work on a copy: owner data and persisted privacy preferences are never changed.
inline storage::PersonProfile publicPersonProfile(storage::PersonProfile person) {
    person.ownerToken.reset();
    person.ownerTokenHash.reset();
    person.ownerId.reset();
    if (!person.privacy.showEmail) person.email.reset();
    if (!person.privacy.showPhone) person.phone.reset();
    if (!person.privacy.showLocation) person.location.reset();
    if (!person.privacy.showAvailability) person.availabilityStatus.reset();
    if (person.avatarUrl && !isPublicImageUrl(*person.avatarUrl)) person.avatarUrl.reset();
    if (person.coverImageUrl && !isPublicImageUrl(*person.coverImageUrl)) person.coverImageUrl.reset();
    return person;
}

inline std::string escapeHtml(const std::string& text) {
    std::string escaped;
    for (char c : text) {
        switch (c) {
        case '&': escaped += "&amp;"; break;
        case '<': escaped += "&lt;"; break;
        case '>': escaped += "&gt;"; break;
        case '"': escaped += "&quot;"; break;
        case '\'': escaped += "&#39;"; break;
        default: escaped += c;
        }
    }
    return escaped;
}

inline nlohmann::json escapeTemplateStrings(nlohmann::json value) {
    if (value.is_string()) return escapeHtml(value.get<std::string>());
    if (value.is_structured()) {
        for (auto& child : value) child = escapeTemplateStrings(std::move(child));
    }
    return value;
}

// JSON escapes remain valid JSON and cannot terminate an HTML script element.
inline std::string scriptSafeJson(const nlohmann::json& value) {
    std::string result;
    for (char c : value.dump(2)) {
        switch (c) {
        case '<': result += "\\u003c"; break;
        case '>': result += "\\u003e"; break;
        case '&': result += "\\u0026"; break;
        default: result += c;
        }
    }
    return result;
}

inline nlohmann::json profileTemplateData(nlohmann::json data) {
    // Inja does not autoescape. Escape all displayed strings, but serialize
    // JSON-LD separately: HTML entities would corrupt the structured data.
    const auto jsonld = data.at("jsonld");
    data.erase("jsonld");
    data = escapeTemplateStrings(std::move(data));
    data["jsonld"] = scriptSafeJson(jsonld);
    return data;
}

inline void addPersonHeaderData(nlohmann::json& data) {
    auto& person = data["profile"];
    const auto hasText = [&person](const char* key) {
        return person.contains(key) && person[key].is_string() &&
            !person[key].get_ref<const std::string&>().empty();
    };
    data["headerName"] = hasText("displayName") ? person["displayName"] : person["name"];
    data["headerSkills"] = nlohmann::json::array();
    const bool leveled = person.contains("skillsWithLevel") && !person["skillsWithLevel"].empty();
    if (leveled) {
        for (const auto& skill : person["skillsWithLevel"]) {
            if (skill.value("name", "").empty()) continue;
            const auto level = skill.value("level", "");
            data["headerSkills"].push_back({{"name", skill["name"]}, {"levelLabel",
                level == "EXPERT" ? "پیشرفته" : level == "INTERMEDIATE" ? "متوسط" :
                level == "BEGINNER" ? "مقدماتی" : ""}});
        }
    } else if (person.contains("skills")) {
        for (const auto& skill : person["skills"]) {
            if (!skill.get<std::string>().empty())
                data["headerSkills"].push_back({{"name", skill}, {"levelLabel", ""}});
        }
    }
    const auto availability = person.value("availabilityStatus", "");
    data["availabilityLabel"] = availability == "AVAILABLE" ? "آماده همکاری" :
        availability == "BUSY" ? "مشغول به کار" : availability == "NOT_AVAILABLE" ? "فعلاً در دسترس نیست" : "";
}

} // namespace search_engine::profile
