#include "../include/search_engine/seo/SitemapGenerator.h"
#include "../include/Logger.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace search_engine {
namespace seo {

std::string SitemapGenerator::generateProfilesSitemap(
    const std::vector<storage::Profile>& profiles,
    const std::string& baseUrl
) {
    LOG_DEBUG("Generating profiles sitemap for " + std::to_string(profiles.size()) + " profiles");

    std::ostringstream xml;
    
    // XML declaration and urlset opening
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xml << "<urlset xmlns=\"http://www.sitemaps.org/schemas/sitemap/0.9\">\n";

    // Add each profile
    for (const auto& profile : profiles) {
        // Only include public profiles
        if (!profile.isPublic) {
            continue;
        }

        xml << "  <url>\n";
        xml << "    <loc>" << escapeXml(baseUrl + "/" + profile.slug) << "</loc>\n";
        
        // Add last modified date
        if (profile.updatedAt.has_value()) {
            xml << "    <lastmod>" << formatW3CDate(profile.updatedAt.value()) << "</lastmod>\n";
        } else {
            xml << "    <lastmod>" << formatW3CDate(profile.createdAt) << "</lastmod>\n";
        }
        
        // Change frequency and priority
        xml << "    <changefreq>weekly</changefreq>\n";
        xml << "    <priority>0.8</priority>\n";
        
        xml << "  </url>\n";
    }

    xml << "</urlset>\n";

    LOG_DEBUG("Profiles sitemap generated successfully");
    return xml.str();
}

std::string SitemapGenerator::generateSitemapIndex(
    int totalProfiles,
    const std::string& baseUrl,
    int profilesPerSitemap
) {
    LOG_DEBUG("Generating sitemap index for " + std::to_string(totalProfiles) + " profiles");

    std::ostringstream xml;
    
    // XML declaration and sitemapindex opening
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xml << "<sitemapindex xmlns=\"http://www.sitemaps.org/schemas/sitemap/0.9\">\n";

    // Static pages sitemap
    xml << "  <sitemap>\n";
    xml << "    <loc>" << escapeXml(baseUrl + "/sitemap-static.xml") << "</loc>\n";
    xml << "  </sitemap>\n";

    // Calculate number of profile sitemaps needed
    int numSitemaps = (totalProfiles + profilesPerSitemap - 1) / profilesPerSitemap;

    // Add each profile sitemap
    for (int i = 0; i < numSitemaps; i++) {
        xml << "  <sitemap>\n";
        xml << "    <loc>" << escapeXml(baseUrl + "/sitemap-profiles-" + std::to_string(i + 1) + ".xml") << "</loc>\n";
        xml << "  </sitemap>\n";
    }

    xml << "</sitemapindex>\n";

    LOG_DEBUG("Sitemap index generated successfully");
    return xml.str();
}

std::string SitemapGenerator::generateStaticPagesSitemap(const std::string& baseUrl) {
    LOG_DEBUG("Generating static pages sitemap");

    std::ostringstream xml;
    
    // XML declaration and urlset opening
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xml << "<urlset xmlns=\"http://www.sitemaps.org/schemas/sitemap/0.9\">\n";

    // Home page
    xml << "  <url>\n";
    xml << "    <loc>" << escapeXml(baseUrl + "/") << "</loc>\n";
    xml << "    <changefreq>daily</changefreq>\n";
    xml << "    <priority>1.0</priority>\n";
    xml << "  </url>\n";

    // Search page
    xml << "  <url>\n";
    xml << "    <loc>" << escapeXml(baseUrl + "/search") << "</loc>\n";
    xml << "    <changefreq>daily</changefreq>\n";
    xml << "    <priority>0.9</priority>\n";
    xml << "  </url>\n";

    // Sponsor page
    xml << "  <url>\n";
    xml << "    <loc>" << escapeXml(baseUrl + "/sponsor") << "</loc>\n";
    xml << "    <changefreq>monthly</changefreq>\n";
    xml << "    <priority>0.6</priority>\n";
    xml << "  </url>\n";

    xml << "</urlset>\n";

    LOG_DEBUG("Static pages sitemap generated successfully");
    return xml.str();
}

std::string SitemapGenerator::escapeXml(const std::string& text) {
    std::string escaped;
    escaped.reserve(text.size());

    for (char c : text) {
        switch (c) {
            case '&':  escaped += "&amp;"; break;
            case '<':  escaped += "&lt;"; break;
            case '>':  escaped += "&gt;"; break;
            case '"':  escaped += "&quot;"; break;
            case '\'': escaped += "&apos;"; break;
            default:   escaped += c; break;
        }
    }

    return escaped;
}

std::string SitemapGenerator::formatW3CDate(const std::chrono::system_clock::time_point& timePoint) {
    auto time_t = std::chrono::system_clock::to_time_t(timePoint);
    std::tm tm = *std::gmtime(&time_t);
    
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S+00:00");
    return oss.str();
}

} // namespace seo
} // namespace search_engine
