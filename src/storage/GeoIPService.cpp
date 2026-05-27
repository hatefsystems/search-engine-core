#include "../../include/search_engine/storage/GeoIPService.h"
#include "../../include/Logger.h"

namespace search_engine {
namespace storage {

GeoData GeoIPService::lookup(const std::string& ipAddress) {
    // Stub implementation - returns "Unknown" for all lookups
    // Future: Integrate MaxMind GeoLite2 database for real geolocation
    
    LOG_DEBUG("GeoIP lookup for IP: " + ipAddress + " (stub - returning Unknown)");
    
    GeoData result;
    result.country = "Unknown";
    result.province = "Unknown";
    result.city = "Unknown";
    
    return result;
}

} // namespace storage
} // namespace search_engine
