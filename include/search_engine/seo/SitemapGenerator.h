#pragma once

#include <string>
#include <vector>
#include "../storage/Profile.h"

namespace search_engine {
namespace seo {

/**
 * Sitemap Generator - Generates XML sitemaps for search engines
 * 
 * Generates sitemaps following the sitemap protocol:
 * https://www.sitemaps.org/protocol.html
 */
class SitemapGenerator {
public:
    /**
     * Generate sitemap for profiles
     * @param profiles Vector of profiles to include in sitemap
     * @param baseUrl Base URL for the website
     * @return XML sitemap string
     */
    static std::string generateProfilesSitemap(
        const std::vector<storage::Profile>& profiles,
        const std::string& baseUrl
    );

    /**
     * Generate sitemap index when there are multiple sitemaps
     * @param totalProfiles Total number of profiles
     * @param baseUrl Base URL for the website
     * @param profilesPerSitemap Number of profiles per sitemap (default 50000)
     * @return XML sitemap index string
     */
    static std::string generateSitemapIndex(
        int totalProfiles,
        const std::string& baseUrl,
        int profilesPerSitemap = 50000
    );

    /**
     * Generate static pages sitemap (home, about, etc.)
     * @param baseUrl Base URL for the website
     * @return XML sitemap string
     */
    static std::string generateStaticPagesSitemap(const std::string& baseUrl);

private:
    /**
     * Escape XML special characters
     * @param text Text to escape
     * @return Escaped text
     */
    static std::string escapeXml(const std::string& text);

    /**
     * Format date to W3C Datetime format (ISO 8601)
     * @param timePoint System clock time point
     * @return W3C formatted date string
     */
    static std::string formatW3CDate(const std::chrono::system_clock::time_point& timePoint);
};

} // namespace seo
} // namespace search_engine
