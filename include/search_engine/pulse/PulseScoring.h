#pragma once

#include <cstdint>

namespace search_engine {
namespace pulse {

class PulseRelativeScore {
public:
    static int fromCount(int64_t value, int64_t maxValue);
    static int clamp(double value);
};

class PulseTrendScore {
public:
    static int calculate(int64_t currentCount, int64_t previousCount);
};

} // namespace pulse
} // namespace search_engine

