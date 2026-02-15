#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "../storage/Profile.h"

namespace search_engine {
namespace seo {

/**
 * SEO Generator - Generates structured data and meta tags for profiles
 * 
 * This utility class generates:
 * - JSON-LD structured data (Person and Organization schemas)
 * - Open Graph meta tags for social sharing
 * - Twitter Card tags
 * - SEO-optimized meta descriptions and titles
 */
class SEOGenerator {
public:
    /**
     * Generate JSON-LD Person schema for individual profiles
     * @param profile PersonProfile struct with person data
     * @param baseUrl Base URL for the website (e.g., "https://hatef.ir")
     * @param linkBlocks Vector of link blocks (social media, portfolio links)
     * @return JSON object with Person schema
     */
    static nlohmann::json generatePersonSchema(
        const storage::PersonProfile& profile,
        const std::string& baseUrl,
        const std::vector<nlohmann::json>& linkBlocks = {}
    );

    /**
     * Generate JSON-LD Organization schema for business profiles
     * @param profile BusinessProfile struct with business data
     * @param baseUrl Base URL for the website
     * @param linkBlocks Vector of link blocks (social media, website links)
     * @return JSON object with Organization schema
     */
    static nlohmann::json generateOrganizationSchema(
        const storage::BusinessProfile& profile,
        const std::string& baseUrl,
        const std::vector<nlohmann::json>& linkBlocks = {}
    );

    /**
     * Generate Open Graph meta tags for social sharing
     * @param profile Base Profile struct
     * @param baseUrl Base URL for the website
     * @param profileType Type string ("person" or "organization")
     * @return Array of Open Graph tag objects with property and content
     */
    static nlohmann::json generateOpenGraphTags(
        const storage::Profile& profile,
        const std::string& baseUrl,
        const std::string& profileType
    );

    /**
     * Generate Twitter Card meta tags
     * @param profile Base Profile struct
     * @param baseUrl Base URL for the website
     * @param profileType Type string ("person" or "organization")
     * @return Array of Twitter Card tag objects with name and content
     */
    static nlohmann::json generateTwitterCardTags(
        const storage::Profile& profile,
        const std::string& baseUrl,
        const std::string& profileType
    );

    /**
     * Generate SEO-optimized meta description
     * @param profile Base Profile struct
     * @param maxLength Maximum length for description (default 160 chars)
     * @return Meta description string
     */
    static std::string generateMetaDescription(
        const storage::Profile& profile,
        size_t maxLength = 160
    );

    /**
     * Generate SEO-optimized page title
     * @param profile Base Profile struct
     * @return Page title string
     */
    static std::string generatePageTitle(const storage::Profile& profile);

private:
    /**
     * Truncate text to specified length with ellipsis
     * @param text Text to truncate
     * @param maxLength Maximum length
     * @return Truncated text
     */
    static std::string truncateText(const std::string& text, size_t maxLength);

    /**
     * Extract social media links from link blocks
     * @param linkBlocks Vector of link block JSON objects
     * @return Array of social media URLs
     */
    static nlohmann::json extractSocialLinks(const std::vector<nlohmann::json>& linkBlocks);

    /**
     * Format ISO 8601 date from time_point
     * @param timePoint System clock time point
     * @return ISO 8601 formatted date string
     */
    static std::string formatISODate(const std::chrono::system_clock::time_point& timePoint);
};

} // namespace seo
} // namespace search_engine
