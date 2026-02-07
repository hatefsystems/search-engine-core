#pragma once

#include <string>

namespace search_engine {
namespace storage {

/**
 * @brief Geographic data extracted from IP address
 * 
 * City-level granularity only (no street addresses or precise locations).
 * Tier 1 analytics uses this for privacy-friendly location tracking.
 */
struct GeoData {
    std::string country = "Unknown";
    std::string province = "Unknown";
    std::string city = "Unknown";
    
    bool isUnknown() const {
        return country == "Unknown" && province == "Unknown" && city == "Unknown";
    }
};

/**
 * @brief GeoIP lookup service for IP address geolocation
 * 
 * Current implementation is a stub that returns "Unknown" for all lookups.
 * 
 * Future enhancements:
 * - Integrate MaxMind GeoLite2 database
 * - Add caching for repeated lookups
 * - Support IPv6
 * - Add accuracy confidence scores
 */
class GeoIPService {
public:
    /**
     * @brief Lookup geographic location from IP address
     * @param ipAddress IP address to lookup (IPv4 or IPv6)
     * @return GeoData with country, province, and city
     * 
     * Note: Current stub implementation returns "Unknown" for all fields.
     *       Real implementation will use MaxMind GeoLite2 or similar.
     */
    static GeoData lookup(const std::string& ipAddress);
    
private:
    // Future: Add GeoIP database instance and cache
};

} // namespace storage
} // namespace search_engine
