#include "../../include/search_engine/pulse/PulseSearchAnalyticsEvent.h"

namespace search_engine {
namespace pulse {

std::string pulseSearchStatusToString(PulseSearchStatus status) {
    switch (status) {
        case PulseSearchStatus::Ok:
            return "ok";
        case PulseSearchStatus::Empty:
            return "empty";
        case PulseSearchStatus::Error:
            return "error";
    }
    return "error";
}

PulseSearchStatus pulseSearchStatusFromString(const std::string& status) {
    if (status == "ok") {
        return PulseSearchStatus::Ok;
    }
    if (status == "empty") {
        return PulseSearchStatus::Empty;
    }
    return PulseSearchStatus::Error;
}

} // namespace pulse
} // namespace search_engine

