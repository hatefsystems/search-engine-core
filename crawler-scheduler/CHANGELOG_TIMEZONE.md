# Changelog: Timezone Configuration Update

## Date: October 17, 2025

### 🎯 Summary

The Crawler Scheduler has been updated to support **automatic timezone detection** and **configurable timezone settings**, making it work correctly based on your system's current timezone instead of being hardcoded to Asia/Tehran.

---

## 🔄 Changes Made

### 1. **Code Changes**

#### `app/config.py`
- ✅ **Added** `_detect_timezone()` function for automatic timezone detection
- ✅ **Added** `Config.TIMEZONE` attribute (replaces hardcoded value)
- ✅ **Implements** priority-based timezone detection:
  1. `SCHEDULER_TIMEZONE` environment variable (highest priority)
  2. `TZ` environment variable
  3. System timezone from `/etc/timezone`
  4. System timezone from `/etc/localtime` symlink
  5. Falls back to `UTC`

#### `app/celery_app.py`
- ✅ **Changed** `timezone='Asia/Tehran'` → `timezone=Config.TIMEZONE`
- ✅ **Added** comment explaining timezone configuration source

#### `app/tasks.py`
- ✅ **Added** `Config` import
- ✅ **Added** startup logging to display configured timezone
- ✅ **Logs** timezone on worker startup for verification

#### `Dockerfile`
- ✅ **Added** `tzdata` package installation for timezone support
- ✅ **Ensures** proper timezone database availability in container

### 2. **Configuration Changes**

#### `docker-compose.yml`
- ✅ **Added** timezone configuration section with examples
- ✅ **Documented** `SCHEDULER_TIMEZONE` environment variable
- ✅ **Documented** `TZ` environment variable as alternative
- ✅ **Added** inline comments explaining auto-detection behavior

### 3. **Documentation Updates**

#### `README.md`
- ✅ **Added** "Timezone Configuration" section
- ✅ **Documented** configuration priority order
- ✅ **Provided** examples for different timezones
- ✅ **Explained** how time windows respect configured timezone

#### `INTEGRATED_USAGE.md`
- ✅ **Updated** configuration examples with timezone settings
- ✅ **Added** timezone variables to configuration table
- ✅ **Clarified** that time windows use configured timezone

#### `TIMEZONE_CONFIGURATION.md` (NEW)
- ✅ **Created** comprehensive timezone configuration guide
- ✅ **Included** priority order explanation
- ✅ **Provided** multiple configuration examples
- ✅ **Added** troubleshooting section
- ✅ **Documented** common timezone formats
- ✅ **Included** best practices

#### `CHANGELOG_TIMEZONE.md` (NEW)
- ✅ **Created** this changelog document

### 4. **Testing Scripts**

#### `scripts/test_timezone.sh` (NEW)
- ✅ **Created** automated timezone detection test script
- ✅ **Tests** default timezone detection
- ✅ **Tests** `SCHEDULER_TIMEZONE` override
- ✅ **Tests** `TZ` environment variable
- ✅ **Tests** priority order
- ✅ **Made** script executable

---

## 📊 Behavior Changes

### Before (Hardcoded)
```python
# Always used Asia/Tehran timezone regardless of system or configuration
timezone='Asia/Tehran'
```

**Impact:**
- All time windows used Asia/Tehran time
- `WARMUP_START_HOUR=10` meant 10:00 AM Iran time
- No way to override without code changes
- Confusing for deployments in other regions

### After (Configurable)
```python
# Automatically detects timezone or uses configured value
timezone=Config.TIMEZONE
```

**Impact:**
- Uses system timezone by default
- Can be overridden with `SCHEDULER_TIMEZONE` or `TZ` env vars
- Time windows respect configured timezone
- `WARMUP_START_HOUR=10` means 10:00 AM in **your** timezone
- Works correctly in any region

---

## 🔧 Migration Guide

### For Existing Deployments (Asia/Tehran)

**No action required** - System will auto-detect Asia/Tehran if that's your system timezone.

**To explicitly maintain Asia/Tehran:**
```yaml
# docker-compose.yml
environment:
  - SCHEDULER_TIMEZONE=Asia/Tehran
```

### For New Deployments (Other Regions)

**Option 1: Use system timezone (auto-detect)**
```yaml
# docker-compose.yml
environment:
  # No SCHEDULER_TIMEZONE or TZ needed - auto-detects
```

**Option 2: Explicitly set timezone**
```yaml
# docker-compose.yml
environment:
  - SCHEDULER_TIMEZONE=America/New_York
  # OR
  - TZ=America/New_York
```

---

## ✅ Testing Results

All timezone detection methods tested and verified:

```bash
✓ Default timezone detection works (detected: Asia/Tehran)
✓ SCHEDULER_TIMEZONE override works (tested: America/New_York)
✓ TZ environment variable works (tested: Europe/London)
✓ Priority order correct (SCHEDULER_TIMEZONE > TZ > system)
```

**Test Command:**
```bash
cd crawler-scheduler
./scripts/test_timezone.sh
```

---

## 🎯 Benefits

1. **✅ Automatic Detection**: Works with system timezone out of the box
2. **✅ Configurable**: Easy to override for any timezone
3. **✅ Flexible**: Multiple configuration methods (SCHEDULER_TIMEZONE, TZ, auto-detect)
4. **✅ Transparent**: Logs configured timezone on startup
5. **✅ Tested**: Comprehensive test script included
6. **✅ Documented**: Full documentation with examples
7. **✅ Backward Compatible**: Existing Asia/Tehran deployments continue to work
8. **✅ Production Ready**: No breaking changes, safe to deploy

---

## 📝 Configuration Examples

### US East Coast
```yaml
environment:
  - SCHEDULER_TIMEZONE=America/New_York
  - WARMUP_START_HOUR=9   # 9 AM Eastern
  - WARMUP_END_HOUR=17    # 5 PM Eastern
```

### Europe
```yaml
environment:
  - SCHEDULER_TIMEZONE=Europe/London
  - WARMUP_START_HOUR=8   # 8 AM GMT/BST
  - WARMUP_END_HOUR=18    # 6 PM GMT/BST
```

### Asia
```yaml
environment:
  - SCHEDULER_TIMEZONE=Asia/Tokyo
  - WARMUP_START_HOUR=10  # 10 AM Japan Time
  - WARMUP_END_HOUR=12    # 12 PM Japan Time
```

### UTC (24/7 Operations)
```yaml
environment:
  - SCHEDULER_TIMEZONE=UTC
  - WARMUP_START_HOUR=0   # Midnight UTC
  - WARMUP_END_HOUR=23    # 11 PM UTC
```

---

## 🔍 Verification

### Check Configured Timezone

```bash
# View timezone in logs
docker logs crawler-scheduler-worker | grep "timezone configured"

# Output example:
# Scheduler timezone configured: America/New_York
```

### Check Current Time Window Status

```bash
# Check if scheduler is in processing window
docker logs --tail 20 crawler-scheduler-worker | grep "time window"

# Output when in window:
# Can process. Progress: 5/50, Remaining: 45 (Day 1)

# Output when outside window:
# Outside processing window. Current: 08:30, Allowed: 10:00-12:00
```

---

## 🔗 Related Documentation

- **Comprehensive Guide**: `TIMEZONE_CONFIGURATION.md`
- **Integration Guide**: `INTEGRATED_USAGE.md`
- **Quick Start**: `QUICKSTART.md`
- **Main Documentation**: `README.md`
- **Test Script**: `scripts/test_timezone.sh`

---

## 📦 Files Modified

### Python Code (3 files)
1. `app/config.py` - Added timezone detection
2. `app/celery_app.py` - Use Config.TIMEZONE
3. `app/tasks.py` - Log timezone on startup

### Configuration (2 files)
4. `Dockerfile` - Added tzdata package
5. `docker-compose.yml` - Added timezone env vars

### Documentation (3 files)
6. `README.md` - Added timezone section
7. `INTEGRATED_USAGE.md` - Updated config docs
8. `TIMEZONE_CONFIGURATION.md` - New comprehensive guide

### Testing (1 file)
9. `scripts/test_timezone.sh` - New test script

### Changelog (1 file)
10. `CHANGELOG_TIMEZONE.md` - This file

**Total: 10 files changed/added**

---

## 🚀 Deployment Notes

### Production Checklist

- [ ] Review current timezone (default will auto-detect)
- [ ] Set `SCHEDULER_TIMEZONE` explicitly if desired
- [ ] Verify time windows are correct for your timezone
- [ ] Test with `./scripts/test_timezone.sh` before deploying
- [ ] Check logs after deployment to confirm timezone
- [ ] Monitor first scheduled run to verify timing

### Rollback Plan

If issues occur, you can revert to hardcoded Asia/Tehran by:

1. Set environment variable:
   ```yaml
   environment:
     - SCHEDULER_TIMEZONE=Asia/Tehran
   ```

2. Or modify code (not recommended):
   ```python
   # app/celery_app.py
   timezone='Asia/Tehran',  # Revert to hardcoded
   ```

---

## ✨ Future Enhancements

Potential future improvements:

- [ ] Multiple time windows per day
- [ ] Different schedules for different days of week
- [ ] Holiday calendar support
- [ ] Daylight saving time awareness (already handled by IANA TZ database)

---

## 📞 Support

For questions or issues related to timezone configuration:

1. **Check Documentation**: `TIMEZONE_CONFIGURATION.md`
2. **Run Test Script**: `./scripts/test_timezone.sh`
3. **Check Logs**: `docker logs crawler-scheduler-worker`
4. **Verify Timezone**: See "Verification" section above

---

**Status**: ✅ Complete and Production Ready

**Version**: 1.1.0 (Timezone Support)

**Compatibility**: Fully backward compatible with existing deployments

