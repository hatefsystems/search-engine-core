# Flower Dashboard - Tehran Timezone Configuration

## Problem

The Flower dashboard was displaying all times in UTC instead of Tehran time (Asia/Tehran, UTC+3:30).

## Solution

Updated both the Celery worker and Flower dashboard containers to use Tehran timezone.

## Changes Made

### 1. Updated `docker-compose.yml`

#### Celery Worker (`crawler-scheduler`)

```yaml
environment:
  # Timezone Configuration
  - TZ=Asia/Tehran # System timezone for Celery worker
  - SCHEDULER_TIMEZONE=${SCHEDULER_TIMEZONE:-Asia/Tehran}
```

#### Flower Dashboard (`crawler-flower`)

```yaml
environment:
  # Timezone Configuration for Flower Dashboard
  - TZ=Asia/Tehran
  - SCHEDULER_TIMEZONE=${SCHEDULER_TIMEZONE:-Asia/Tehran}
```

### 2. Updated `docker/docker-compose.prod.yml`

Same changes applied to production Docker Compose file for consistency.

## Verification

After the changes, both services now display Tehran time:

```bash
# Check timezone in worker
docker exec crawler-scheduler-worker env | grep TZ
# Output: TZ=Asia/Tehran

# Check timezone in Flower
docker exec crawler-scheduler-flower env | grep TZ
# Output: TZ=Asia/Tehran

# Check Flower logs
docker logs --tail 20 crawler-scheduler-flower
# Output shows: [I 251019 02:58:43 tasks:17] Scheduler timezone configured: Asia/Tehran
```

## Result

✅ **All times in Flower dashboard now display in Tehran timezone (UTC+3:30)**
✅ **No clock drift warnings between worker and dashboard**
✅ **Task times (Received, Started, Succeeded, Expires, Timestamp) all in local time**

## Future Configuration

To change the timezone to a different location, modify the `TZ` environment variable in both services:

```yaml
# For New York time
- TZ=America/New_York

# For London time
- TZ=Europe/London

# For Tokyo time
- TZ=Asia/Tokyo
```

Then restart the services:

```bash
docker-compose up -d crawler-scheduler crawler-flower
```

## Notes

- The timezone is set to Tehran (Asia/Tehran) by default
- Can be overridden by setting `SCHEDULER_TIMEZONE` environment variable
- Both worker and dashboard must use the same timezone to avoid clock drift warnings
- Timezone affects task scheduling, task display times, and warm-up hour ranges
