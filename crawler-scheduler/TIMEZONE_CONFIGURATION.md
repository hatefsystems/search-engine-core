# Timezone Configuration Guide

The Crawler Scheduler now automatically detects and uses your system's timezone for all time-based scheduling operations.

## 🌍 Overview

The scheduler determines timezone in the following priority order:

1. **`SCHEDULER_TIMEZONE` environment variable** (highest priority)
2. **`TZ` environment variable** (system-wide timezone)
3. **System timezone** from `/etc/timezone` file
4. **System timezone** from `/etc/localtime` symlink
5. **UTC** as fallback (if all detection methods fail)

## 📋 Configuration Methods

### Method 1: SCHEDULER_TIMEZONE Environment Variable (Recommended)

This is the recommended method for explicitly setting the scheduler's timezone:

```bash
# In docker-compose.yml
environment:
  - SCHEDULER_TIMEZONE=America/New_York
```

```bash
# In .env file
SCHEDULER_TIMEZONE=America/New_York
```

```bash
# Command line
docker run -e SCHEDULER_TIMEZONE=Europe/London crawler-scheduler
```

**Common Timezone Values:**
- `America/New_York` - US Eastern Time
- `America/Los_Angeles` - US Pacific Time
- `America/Chicago` - US Central Time
- `Europe/London` - UK Time
- `Europe/Paris` - Central European Time
- `Asia/Tokyo` - Japan Time
- `Asia/Shanghai` - China Time
- `Asia/Tehran` - Iran Time
- `UTC` - Coordinated Universal Time

### Method 2: TZ Environment Variable (System-wide)

Use the standard `TZ` environment variable to set the timezone:

```bash
# In docker-compose.yml
environment:
  - TZ=Asia/Tokyo
```

**Note:** If both `SCHEDULER_TIMEZONE` and `TZ` are set, `SCHEDULER_TIMEZONE` takes priority.

### Method 3: Auto-Detection (Default)

If no environment variables are set, the scheduler automatically detects the system timezone:

```bash
# No configuration needed - uses container/host system timezone
docker run crawler-scheduler
```

The detection checks:
1. `/etc/timezone` file (Debian/Ubuntu systems)
2. `/etc/localtime` symlink (modern Linux systems)
3. Falls back to `UTC` if detection fails

## ⚙️ How It Works

### Time Window Processing

All time-based settings use the configured timezone:

```bash
WARMUP_START_HOUR=10  # 10:00 AM in configured timezone
WARMUP_END_HOUR=12    # 12:00 PM in configured timezone
```

**Example:**
- If `SCHEDULER_TIMEZONE=America/New_York`
- And `WARMUP_START_HOUR=10`
- Processing starts at **10:00 AM Eastern Time**

### Logging

The scheduler logs the configured timezone on startup:

```
2025-10-17 10:00:00 - app.tasks - INFO - Scheduler timezone configured: America/New_York
```

## 🧪 Testing Timezone Configuration

Use the included test script to verify timezone detection:

```bash
cd crawler-scheduler
./scripts/test_timezone.sh
```

This script tests:
- Default timezone detection
- `SCHEDULER_TIMEZONE` override
- `TZ` environment variable override
- Priority order (SCHEDULER_TIMEZONE > TZ)

## 📝 Example Configurations

### Example 1: US East Coast

```yaml
# docker-compose.yml
services:
  crawler-scheduler:
    environment:
      - SCHEDULER_TIMEZONE=America/New_York
      - WARMUP_START_HOUR=9   # 9:00 AM Eastern Time
      - WARMUP_END_HOUR=17    # 5:00 PM Eastern Time
```

### Example 2: European Operations

```yaml
# docker-compose.yml
services:
  crawler-scheduler:
    environment:
      - SCHEDULER_TIMEZONE=Europe/London
      - WARMUP_START_HOUR=8   # 8:00 AM GMT/BST
      - WARMUP_END_HOUR=18    # 6:00 PM GMT/BST
```

### Example 3: Asian Operations

```yaml
# docker-compose.yml
services:
  crawler-scheduler:
    environment:
      - SCHEDULER_TIMEZONE=Asia/Tehran
      - WARMUP_START_HOUR=10  # 10:00 AM Iran Time
      - WARMUP_END_HOUR=12    # 12:00 PM Iran Time
```

### Example 4: UTC (24/7 Operations)

```yaml
# docker-compose.yml
services:
  crawler-scheduler:
    environment:
      - SCHEDULER_TIMEZONE=UTC
      - WARMUP_START_HOUR=0   # Midnight UTC
      - WARMUP_END_HOUR=23    # 11:00 PM UTC
```

### Example 5: Multiple Instances (Different Timezones)

```yaml
# docker-compose.yml
services:
  # US scheduler
  crawler-scheduler-us:
    environment:
      - SCHEDULER_TIMEZONE=America/New_York
      - WARMUP_START_HOUR=9
      - WARMUP_END_HOUR=17
  
  # EU scheduler
  crawler-scheduler-eu:
    environment:
      - SCHEDULER_TIMEZONE=Europe/London
      - WARMUP_START_HOUR=8
      - WARMUP_END_HOUR=18
```

## 🔍 Troubleshooting

### Check Current Timezone

View the configured timezone in logs:

```bash
docker logs crawler-scheduler-worker | grep "timezone configured"
```

Expected output:
```
Scheduler timezone configured: America/New_York
```

### Manual Timezone Check

Check timezone inside the container:

```bash
docker exec crawler-scheduler-worker python -c "from app.config import Config; print(Config.TIMEZONE)"
```

### Verify Time Window

Check if scheduler is currently in the processing window:

```bash
docker logs --tail 20 crawler-scheduler-worker | grep "time window"
```

Expected output when outside window:
```
Outside processing window. Current: 08:30, Allowed: 10:00-12:00
```

Expected output when inside window:
```
Can process. Progress: 5/50, Remaining: 45 (Day 1)
```

## 🌐 Common Timezone Formats

The scheduler uses IANA Time Zone Database format:

**Format:** `Continent/City` or `Region/City`

**Valid Examples:**
- ✅ `America/New_York`
- ✅ `Europe/London`
- ✅ `Asia/Tokyo`
- ✅ `UTC`

**Invalid Examples:**
- ❌ `EST` (use `America/New_York`)
- ❌ `PST` (use `America/Los_Angeles`)
- ❌ `GMT` (use `UTC` or `Europe/London`)

**Full List:** [https://en.wikipedia.org/wiki/List_of_tz_database_time_zones](https://en.wikipedia.org/wiki/List_of_tz_database_time_zones)

## 💡 Best Practices

1. **Explicit Configuration**: Set `SCHEDULER_TIMEZONE` explicitly in production to avoid surprises
2. **UTC for Global Operations**: Use `UTC` if running 24/7 or across multiple regions
3. **Local Time for Regional**: Use local timezone if targeting specific regional business hours
4. **Test First**: Always test timezone configuration before deploying to production
5. **Document Settings**: Document your timezone choice in deployment documentation
6. **Consistent Configuration**: Use the same timezone across all scheduler instances in a deployment

## 🔄 Migration from Previous Version

If you were using the hardcoded `Asia/Tehran` timezone:

### Before (Hardcoded)
```python
# app/celery_app.py
timezone='Asia/Tehran',  # Hardcoded
```

### After (Configurable)
```python
# app/celery_app.py
timezone=Config.TIMEZONE,  # Auto-detected or configured
```

**To maintain previous behavior:**
```yaml
# docker-compose.yml
environment:
  - SCHEDULER_TIMEZONE=Asia/Tehran
```

**To use system timezone:**
```yaml
# docker-compose.yml
environment:
  # No SCHEDULER_TIMEZONE or TZ - auto-detects system timezone
```

## 📚 Related Documentation

- **Configuration Guide**: `INTEGRATED_USAGE.md`
- **Quick Start**: `QUICKSTART.md`
- **Main Documentation**: `README.md`
- **Test Script**: `scripts/test_timezone.sh`

## ✅ Summary

The crawler scheduler is now **timezone-aware** and works with your current system timezone:

- ✅ **Auto-detects** system timezone by default
- ✅ **Configurable** via `SCHEDULER_TIMEZONE` or `TZ` environment variables
- ✅ **Explicit fallback** to UTC if detection fails
- ✅ **Logged** on startup for verification
- ✅ **Tested** with included test script
- ✅ **Documented** with examples

All time-based operations (`WARMUP_START_HOUR`, `WARMUP_END_HOUR`) now respect the configured timezone, making the scheduler work correctly regardless of where it's deployed! 🌍

