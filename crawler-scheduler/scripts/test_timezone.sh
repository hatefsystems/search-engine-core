#!/bin/bash
# Test timezone detection in crawler scheduler

set -e

echo "=================================="
echo "Timezone Detection Test"
echo "=================================="
echo ""

# Check if Docker is running
if ! docker info > /dev/null 2>&1; then
  echo "✗ Docker is not running. Please start Docker first."
  exit 1
fi

echo "✓ Docker is running"
echo ""

# Build the image if needed
echo "Building crawler-scheduler image..."
cd "$(dirname "$0")/.."
docker build -t crawler-scheduler:test -q . > /dev/null 2>&1
echo "✓ Image built"
echo ""

# Test 1: Default timezone detection
echo "Test 1: Default timezone (auto-detect)"
echo "--------------------------------------"
TZ_DETECTED=$(docker run --rm crawler-scheduler:test python -c "
from app.config import Config
print(Config.TIMEZONE)
")
echo "Detected timezone: $TZ_DETECTED"
echo ""

# Test 2: Override with SCHEDULER_TIMEZONE
echo "Test 2: Override with SCHEDULER_TIMEZONE"
echo "-----------------------------------------"
TZ_OVERRIDE=$(docker run --rm -e SCHEDULER_TIMEZONE=America/New_York crawler-scheduler:test python -c "
from app.config import Config
print(Config.TIMEZONE)
")
echo "Expected: America/New_York"
echo "Got: $TZ_OVERRIDE"
if [ "$TZ_OVERRIDE" = "America/New_York" ]; then
  echo "✓ SCHEDULER_TIMEZONE override works"
else
  echo "✗ SCHEDULER_TIMEZONE override failed"
  exit 1
fi
echo ""

# Test 3: Override with TZ environment variable
echo "Test 3: Override with TZ variable"
echo "----------------------------------"
TZ_SYSTEM=$(docker run --rm -e TZ=Europe/London crawler-scheduler:test python -c "
from app.config import Config
print(Config.TIMEZONE)
")
echo "Expected: Europe/London"
echo "Got: $TZ_SYSTEM"
if [ "$TZ_SYSTEM" = "Europe/London" ]; then
  echo "✓ TZ environment variable works"
else
  echo "✗ TZ environment variable failed"
  exit 1
fi
echo ""

# Test 4: Priority test (SCHEDULER_TIMEZONE should win)
echo "Test 4: Priority test (SCHEDULER_TIMEZONE > TZ)"
echo "------------------------------------------------"
TZ_PRIORITY=$(docker run --rm \
  -e SCHEDULER_TIMEZONE=Asia/Tokyo \
  -e TZ=Europe/Paris \
  crawler-scheduler:test python -c "
from app.config import Config
print(Config.TIMEZONE)
")
echo "Expected: Asia/Tokyo (SCHEDULER_TIMEZONE has priority)"
echo "Got: $TZ_PRIORITY"
if [ "$TZ_PRIORITY" = "Asia/Tokyo" ]; then
  echo "✓ Priority order works correctly"
else
  echo "✗ Priority order failed"
  exit 1
fi
echo ""

echo "=================================="
echo "All Tests Passed! ✅"
echo "=================================="
echo ""
echo "Timezone detection is working correctly."
echo "The scheduler will use:"
echo "  1. SCHEDULER_TIMEZONE env var (if set)"
echo "  2. TZ env var (if set)"
echo "  3. System timezone (auto-detected)"
echo "  4. UTC (fallback)"

