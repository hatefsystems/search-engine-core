#include "../../include/search_engine/pulse/PulseScoring.h"

#include <algorithm>
#include <cmath>

namespace search_engine {
namespace pulse {

int PulseRelativeScore::fromCount(int64_t value, int64_t maxValue) {
    if (value <= 0 || maxValue <= 0) {
        return 0;
    }
    if (value >= maxValue) {
        return 100;
    }
    return clamp((static_cast<double>(value) / static_cast<double>(maxValue)) * 100.0);
}

int PulseRelativeScore::clamp(double value) {
    if (value <= 0.0) {
        return 0;
    }
    if (value >= 100.0) {
        return 100;
    }
    return static_cast<int>(std::round(value));
}

int PulseTrendScore::calculate(int64_t currentCount, int64_t previousCount) {
    if (currentCount <= 0) {
        return 0;
    }
    if (previousCount <= 0) {
        return currentCount >= 3 ? 100 : 50;
    }

    double lift = static_cast<double>(currentCount - previousCount) / static_cast<double>(previousCount);
    if (lift <= 0.0) {
        return 0;
    }

    return PulseRelativeScore::clamp(std::min(100.0, lift * 50.0));
}

} // namespace pulse
} // namespace search_engine

