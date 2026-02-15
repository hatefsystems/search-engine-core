#include "../include/search_engine/seo/SEOGenerator.h"
#include "../include/Logger.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace search_engine {
namespace seo {

nlohmann::json SEOGenerator::generatePersonSchema(
    const storage::PersonProfile& profile,
    const std::string& baseUrl,
    const std::vector<nlohmann::json>& linkBlocks
) {
    LOG_DEBUG("Generating Person schema for profile: " + profile.slug);

    nlohmann::json schema = {
        {"@context", "https://schema.org"},
        {"@type", "Person"},
        {"name", profile.name},
        {"url", baseUrl + "/" + profile.slug}
    };

    // Add bio as description
    if (profile.bio.has_value() && !profile.bio.value().empty()) {
        schema["description"] = profile.bio.value();
    }

    // Add job title and occupation
    if (profile.title.has_value() && !profile.title.value().empty()) {
        nlohmann::json occupation = {
            {"@type", "Occupation"},
            {"name", profile.title.value()}
        };

        // Add company if available
        if (profile.company.has_value() && !profile.company.value().empty()) {
            occupation["occupationLocation"] = {
                {"@type", "Organization"},
                {"name", profile.company.value()}
            };
        }

        schema["hasOccupation"] = occupation;
        schema["jobTitle"] = profile.title.value();
    }

    // Add skills as knowsAbout
    if (!profile.skills.empty()) {
        schema["knowsAbout"] = profile.skills;
    }

    // Add education
    if (profile.education.has_value() && !profile.education.value().empty()) {
        nlohmann::json educationalCredential = {
            {"@type", "EducationalOccupationalCredential"},
            {"credentialCategory", profile.education.value()}
        };

        if (profile.school.has_value() && !profile.school.value().empty()) {
            educationalCredential["educationalLevel"] = profile.school.value();
        }

        schema["hasCredential"] = educationalCredential;
    }

    // Add social links from link blocks
    nlohmann::json socialLinks = extractSocialLinks(linkBlocks);
    if (!socialLinks.empty()) {
        schema["sameAs"] = socialLinks;
    }

    // Add contact information (only if public)
    if (profile.email.has_value() && !profile.email.value().empty()) {
        schema["email"] = profile.email.value();
    }

    if (profile.phone.has_value() && !profile.phone.value().empty()) {
        schema["telephone"] = profile.phone.value();
    }

    // Add portfolio URL if available
    if (profile.portfolioUrl.has_value() && !profile.portfolioUrl.value().empty()) {
        schema["url"] = profile.portfolioUrl.value();
        schema["mainEntityOfPage"] = profile.portfolioUrl.value();
    }

    LOG_DEBUG("Person schema generated successfully");
    return schema;
}

nlohmann::json SEOGenerator::generateOrganizationSchema(
    const storage::BusinessProfile& profile,
    const std::string& baseUrl,
    const std::vector<nlohmann::json>& linkBlocks
) {
    LOG_DEBUG("Generating Organization schema for profile: " + profile.slug);

    // Use LocalBusiness if address is available, otherwise Organization
    std::string schemaType = "Organization";
    if (profile.address.has_value() && !profile.address.value().empty()) {
        schemaType = "LocalBusiness";
    }

    nlohmann::json schema = {
        {"@context", "https://schema.org"},
        {"@type", schemaType},
        {"name", profile.companyName.has_value() ? profile.companyName.value() : profile.name},
        {"url", baseUrl + "/" + profile.slug}
    };

    // Add alternate name if different from company name
    if (profile.companyName.has_value() && profile.companyName.value() != profile.name) {
        schema["alternateName"] = profile.name;
    }

    // Add description
    if (profile.description.has_value() && !profile.description.value().empty()) {
        schema["description"] = profile.description.value();
    } else if (profile.bio.has_value() && !profile.bio.value().empty()) {
        schema["description"] = profile.bio.value();
    }

    // Add founding date
    if (profile.foundedYear.has_value()) {
        schema["foundingDate"] = std::to_string(profile.foundedYear.value());
    }

    // Add industry
    if (profile.industry.has_value() && !profile.industry.value().empty()) {
        schema["industry"] = profile.industry.value();
    }

    // Add address
    if (profile.address.has_value() && !profile.address.value().empty()) {
        nlohmann::json address = {
            {"@type", "PostalAddress"},
            {"streetAddress", profile.address.value()}
        };

        if (profile.city.has_value() && !profile.city.value().empty()) {
            address["addressLocality"] = profile.city.value();
        }

        if (profile.country.has_value() && !profile.country.value().empty()) {
            address["addressCountry"] = profile.country.value();
        }

        schema["address"] = address;
    }

    // Add contact point
    if (profile.businessEmail.has_value() || profile.businessPhone.has_value()) {
        nlohmann::json contactPoint = {
            {"@type", "ContactPoint"},
            {"contactType", "customer service"}
        };

        if (profile.businessEmail.has_value() && !profile.businessEmail.value().empty()) {
            contactPoint["email"] = profile.businessEmail.value();
        }

        if (profile.businessPhone.has_value() && !profile.businessPhone.value().empty()) {
            contactPoint["telephone"] = profile.businessPhone.value();
        }

        schema["contactPoint"] = contactPoint;
    }

    // Add website
    if (profile.website.has_value() && !profile.website.value().empty()) {
        schema["url"] = profile.website.value();
    }

    // Add services/products
    if (!profile.services.empty()) {
        nlohmann::json servicesArray = nlohmann::json::array();
        for (const auto& service : profile.services) {
            servicesArray.push_back({
                {"@type", "Service"},
                {"name", service}
            });
        }
        schema["makesOffer"] = servicesArray;
    }

    // Add social links from link blocks
    nlohmann::json socialLinks = extractSocialLinks(linkBlocks);
    if (!socialLinks.empty()) {
        schema["sameAs"] = socialLinks;
    }

    // Add company size as numberOfEmployees
    if (profile.companySize.has_value() && !profile.companySize.value().empty()) {
        schema["numberOfEmployees"] = profile.companySize.value();
    }

    LOG_DEBUG("Organization schema generated successfully");
    return schema;
}

nlohmann::json SEOGenerator::generateOpenGraphTags(
    const storage::Profile& profile,
    const std::string& baseUrl,
    const std::string& profileType
) {
    LOG_DEBUG("Generating Open Graph tags for profile: " + profile.slug);

    nlohmann::json tags = nlohmann::json::array();

    // Basic Open Graph tags
    tags.push_back({
        {"property", "og:type"},
        {"content", "profile"}
    });

    tags.push_back({
        {"property", "og:title"},
        {"content", generatePageTitle(profile)}
    });

    tags.push_back({
        {"property", "og:url"},
        {"content", baseUrl + "/" + profile.slug}
    });

    // Add description
    std::string description = generateMetaDescription(profile);
    tags.push_back({
        {"property", "og:description"},
        {"content", description}
    });

    // Add site name
    tags.push_back({
        {"property", "og:site_name"},
        {"content", "Hatef.ir"}
    });

    // Add locale
    tags.push_back({
        {"property", "og:locale"},
        {"content", "en_US"}
    });

    // Profile-specific Open Graph tags
    tags.push_back({
        {"property", "profile:username"},
        {"content", profile.slug}
    });

    LOG_DEBUG("Open Graph tags generated successfully");
    return tags;
}

nlohmann::json SEOGenerator::generateTwitterCardTags(
    const storage::Profile& profile,
    const std::string& baseUrl,
    const std::string& profileType
) {
    LOG_DEBUG("Generating Twitter Card tags for profile: " + profile.slug);

    nlohmann::json tags = nlohmann::json::array();

    // Twitter Card type
    tags.push_back({
        {"name", "twitter:card"},
        {"content", "summary"}
    });

    // Title
    tags.push_back({
        {"name", "twitter:title"},
        {"content", generatePageTitle(profile)}
    });

    // Description
    std::string description = generateMetaDescription(profile);
    tags.push_back({
        {"name", "twitter:description"},
        {"content", description}
    });

    // URL
    tags.push_back({
        {"name", "twitter:url"},
        {"content", baseUrl + "/" + profile.slug}
    });

    LOG_DEBUG("Twitter Card tags generated successfully");
    return tags;
}

std::string SEOGenerator::generateMetaDescription(
    const storage::Profile& profile,
    size_t maxLength
) {
    std::string description;

    // Try to use bio first
    if (profile.bio.has_value() && !profile.bio.value().empty()) {
        description = profile.bio.value();
    } else {
        // Fallback to generic description
        description = profile.name + " - Profile on Hatef.ir";
    }

    return truncateText(description, maxLength);
}

std::string SEOGenerator::generatePageTitle(const storage::Profile& profile) {
    std::string title = profile.name;

    // Add additional context for Person profiles
    if (profile.type == storage::ProfileType::PERSON) {
        // Try to cast to PersonProfile to access title
        // Note: In actual usage, caller should pass correct profile type
        title += " - Profile";
    } else if (profile.type == storage::ProfileType::BUSINESS) {
        title += " - Business Profile";
    }

    // Add site name
    title += " | Hatef.ir";

    return title;
}

std::string SEOGenerator::truncateText(const std::string& text, size_t maxLength) {
    if (text.length() <= maxLength) {
        return text;
    }

    // Find last space before maxLength to avoid cutting words
    size_t truncateAt = text.rfind(' ', maxLength - 3);
    if (truncateAt == std::string::npos || truncateAt < maxLength / 2) {
        truncateAt = maxLength - 3;
    }

    return text.substr(0, truncateAt) + "...";
}

nlohmann::json SEOGenerator::extractSocialLinks(const std::vector<nlohmann::json>& linkBlocks) {
    nlohmann::json socialLinks = nlohmann::json::array();

    for (const auto& linkBlock : linkBlocks) {
        if (linkBlock.contains("url") && linkBlock["url"].is_string()) {
            std::string url = linkBlock["url"].get<std::string>();
            
            // Filter for social media and professional networks
            std::vector<std::string> socialDomains = {
                "linkedin.com", "github.com", "twitter.com", "x.com",
                "facebook.com", "instagram.com", "youtube.com",
                "medium.com", "stackoverflow.com", "gitlab.com"
            };

            for (const auto& domain : socialDomains) {
                if (url.find(domain) != std::string::npos) {
                    socialLinks.push_back(url);
                    break;
                }
            }
        }
    }

    return socialLinks;
}

std::string SEOGenerator::formatISODate(const std::chrono::system_clock::time_point& timePoint) {
    auto time_t = std::chrono::system_clock::to_time_t(timePoint);
    std::tm tm = *std::gmtime(&time_t);
    
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d");
    return oss.str();
}

} // namespace seo
} // namespace search_engine
