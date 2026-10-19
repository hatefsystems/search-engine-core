#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace search_engine {
namespace pulse {

enum class PulseSearchStatus {
    Ok,
    Empty,
    Error
};

struct PulseDeviceInfo {
    std::string browser = "Unknown";
    std::string os = "Unknown";
    std::string type = "Desktop";
};

struct PulseSearchAnalyticsEvent {
    std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();
    std::string rawQuery;
    std::string normalizedQuery;
    std::string normalizedQueryHash;
    int64_t resultCount = 0;
    double latencyMs = 0.0;
    PulseSearchStatus status = PulseSearchStatus::Ok;
    std::string languageEstimate = "unknown";
    std::string source = "api_search";
    PulseDeviceInfo device;
    std::optional<std::string> anonymousUserHash;
    std::optional<std::string> requestHash;
    bool publicSafe = false;

    std::optional<std::string> country;
    std::optional<std::string> province;
    std::optional<std::string> city;
};

std::string pulseSearchStatusToString(PulseSearchStatus status);
PulseSearchStatus pulseSearchStatusFromString(const std::string& status);

} // namespace pulse
} // namespace search_engine

