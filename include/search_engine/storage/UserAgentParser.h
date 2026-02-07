#pragma once

#include <string>

namespace search_engine {
namespace storage {

/**
 * @brief Parsed user-agent information (generic, no versions)
 * 
 * Privacy-first parsing: browser/OS family only, no version tracking.
 * Used for Tier 1 analytics without identifying specific users.
 */
struct UserAgentInfo {
    std::string browser = "Unknown";      // e.g., "Chrome", "Firefox", "Safari"
    std::string os = "Unknown";           // e.g., "Windows", "Android", "iOS"
    std::string deviceType = "Desktop";   // "Mobile", "Tablet", or "Desktop"
    
    bool isUnknown() const {
        return browser == "Unknown" && os == "Unknown";
    }
};

/**
 * @brief Simple User-Agent string parser
 * 
 * Extracts generic browser, OS, and device type from User-Agent strings.
 * No version numbers or fine-grained tracking to preserve privacy.
 * 
 * Pattern matching based on common User-Agent signatures:
 * - Browsers: Chrome, Firefox, Safari, Edge, Opera
 * - OS: Windows, macOS, Linux, Android, iOS
 * - Device: Mobile, Tablet, Desktop (based on Mobile/Tablet keywords)
 */
class UserAgentParser {
public:
    /**
     * @brief Parse User-Agent string into generic components
     * @param userAgent Raw User-Agent string from HTTP request
     * @return UserAgentInfo with browser, OS, and device type
     */
    static UserAgentInfo parse(const std::string& userAgent);
    
private:
    static std::string detectBrowser(const std::string& ua);
    static std::string detectOS(const std::string& ua);
    static std::string detectDeviceType(const std::string& ua);
    
    // Helper to check if string contains substring (case-insensitive)
    static bool contains(const std::string& str, const std::string& substr);
};

} // namespace storage
} // namespace search_engine
