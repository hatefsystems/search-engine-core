#include "../../include/search_engine/storage/UserAgentParser.h"
#include "../../include/Logger.h"
#include <algorithm>
#include <cctype>

namespace search_engine {
namespace storage {

bool UserAgentParser::contains(const std::string& str, const std::string& substr) {
    std::string strLower = str;
    std::string substrLower = substr;
    
    std::transform(strLower.begin(), strLower.end(), strLower.begin(), ::tolower);
    std::transform(substrLower.begin(), substrLower.end(), substrLower.begin(), ::tolower);
    
    return strLower.find(substrLower) != std::string::npos;
}

std::string UserAgentParser::detectBrowser(const std::string& ua) {
    // Check for common browsers (order matters for correct detection)
    // Edge must be checked before Chrome (Edge contains Chrome in UA)
    if (contains(ua, "Edg/") || contains(ua, "Edge/")) {
        return "Edge";
    }
    if (contains(ua, "Chrome/") || contains(ua, "CriOS/")) {
        return "Chrome";
    }
    if (contains(ua, "Firefox/") || contains(ua, "FxiOS/")) {
        return "Firefox";
    }
    if (contains(ua, "Safari/") && !contains(ua, "Chrome")) {
        return "Safari";
    }
    if (contains(ua, "OPR/") || contains(ua, "Opera/")) {
        return "Opera";
    }
    
    return "Unknown";
}

std::string UserAgentParser::detectOS(const std::string& ua) {
    // Check for mobile OS first
    if (contains(ua, "Android")) {
        return "Android";
    }
    if (contains(ua, "iPhone") || contains(ua, "iPad") || contains(ua, "iPod")) {
        return "iOS";
    }
    
    // Desktop OS
    if (contains(ua, "Windows")) {
        return "Windows";
    }
    if (contains(ua, "Macintosh") || contains(ua, "Mac OS X")) {
        return "macOS";
    }
    if (contains(ua, "Linux") && !contains(ua, "Android")) {
        return "Linux";
    }
    
    return "Unknown";
}

std::string UserAgentParser::detectDeviceType(const std::string& ua) {
    // Detect mobile devices
    if (contains(ua, "Mobile") || contains(ua, "iPhone") || contains(ua, "iPod") || 
        (contains(ua, "Android") && !contains(ua, "Tablet"))) {
        return "Mobile";
    }
    
    // Detect tablets
    if (contains(ua, "Tablet") || contains(ua, "iPad")) {
        return "Tablet";
    }
    
    // Default to desktop
    return "Desktop";
}

UserAgentInfo UserAgentParser::parse(const std::string& userAgent) {
    if (userAgent.empty()) {
        LOG_DEBUG("Empty User-Agent string");
        return UserAgentInfo{};
    }
    
    UserAgentInfo info;
    info.browser = detectBrowser(userAgent);
    info.os = detectOS(userAgent);
    info.deviceType = detectDeviceType(userAgent);
    
    LOG_DEBUG("Parsed UA: browser=" + info.browser + ", os=" + info.os + 
              ", deviceType=" + info.deviceType);
    
    return info;
}

} // namespace storage
} // namespace search_engine
