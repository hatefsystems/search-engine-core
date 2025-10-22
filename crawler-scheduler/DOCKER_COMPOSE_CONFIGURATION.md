# Docker Compose Configuration Reference

Complete guide for configuring the Crawler Scheduler in both development and production environments.

---

## 📋 Overview

The crawler scheduler is now integrated into both:
- **Development**: `/docker-compose.yml`
- **Production**: `/docker/docker-compose.prod.yml`

Both configurations support **automatic timezone detection** with optional override capabilities.

---

## 🔧 Development Configuration

### File: `docker-compose.yml`

```yaml
crawler-scheduler:
  build: ./crawler-scheduler
  container_name: crawler-scheduler-worker
  restart: unless-stopped
  command: celery -A app.celery_app worker --beat --loglevel=info
  volumes:
    - ./crawler-scheduler/data:/app/data  # ← Host files accessible in container
    - ./crawler-scheduler/app:/app/app    # ← Hot reload for development
  environment:
    # ... configuration options below ...
```

### Volume Mappings

| Host Path | Container Path | Purpose |
|-----------|---------------|---------|
| `./crawler-scheduler/data` | `/app/data` | Persistent data (pending/processed/failed files) |
| `./crawler-scheduler/app` | `/app/app` | Code hot reload for development |

---

## 🚀 Production Configuration

### File: `docker/docker-compose.prod.yml`

```yaml
crawler-scheduler:
  image: ghcr.io/hatefsystems/search-engine-core/crawler-scheduler:latest
  container_name: crawler-scheduler-worker
  restart: unless-stopped
  command: celery -A app.celery_app worker --beat --loglevel=warning --concurrency=2
  volumes:
    - crawler_data:/app/data  # ← Named volume for persistence
  environment:
    # ... configuration options below ...
  deploy:
    resources:
      limits:
        memory: 512M
        cpus: '0.5'
```

### Key Differences

| Aspect | Development | Production |
|--------|-------------|------------|
| **Image** | Built locally | Pulled from GHCR |
| **Volumes** | Bind mounts (host paths) | Named volumes |
| **Concurrency** | 1 (default) | 2 workers |
| **Log Level** | `info` | `warning` |
| **Resources** | Unlimited | Limited (512MB RAM, 0.5 CPU) |

---

## ⚙️ Environment Variables

### 🌍 Timezone Configuration

```yaml
# Auto-detects system timezone by default (Ubuntu 24: Asia/Tehran)
environment:
  # Optional: Override system timezone
  - SCHEDULER_TIMEZONE=${SCHEDULER_TIMEZONE}  
  # Example values:
  # - SCHEDULER_TIMEZONE=America/New_York
  # - SCHEDULER_TIMEZONE=Europe/London
  # - SCHEDULER_TIMEZONE=Asia/Tokyo
  
  # Alternative: Use TZ variable
  # - TZ=America/New_York
```

**Behavior:**
- **Not set**: Auto-detects from `/etc/timezone` (Ubuntu 24)
- **SCHEDULER_TIMEZONE set**: Overrides system timezone
- **TZ set**: Alternative override method
- **Priority**: `SCHEDULER_TIMEZONE` > `TZ` > system timezone > UTC

---

### 📅 Warm-up Configuration (Progressive Rate Limiting)

```yaml
environment:
  # Enable/disable progressive warm-up
  - WARMUP_ENABLED=${CRAWLER_WARMUP_ENABLED:-true}
  
  # Daily limits (comma-separated)
  # Day 1: 50, Day 2: 100, Day 3: 200, Day 4: 400, Day 5+: 800
  - WARMUP_SCHEDULE=${CRAWLER_WARMUP_SCHEDULE:-50,100,200,400,800}
  
  # Processing time window (in configured timezone)
  - WARMUP_START_HOUR=${CRAWLER_WARMUP_START_HOUR:-0}   # Start hour (0-23)
  - WARMUP_END_HOUR=${CRAWLER_WARMUP_END_HOUR:-23}      # End hour (0-23, INCLUSIVE)
```

**Time Window Examples:**

| Configuration | Processing Window | Use Case |
|---------------|-------------------|----------|
| `START=0, END=23` | 00:00 - 23:59 (full day) | 24/7 processing |
| `START=9, END=17` | 09:00 - 17:59 | Business hours |
| `START=10, END=12` | 10:00 - 12:59 | Limited window |
| `START=22, END=2` | 22:00-23:59, 00:00-02:59 | Night processing (wrap-around) |

**Important**: End hour is **INCLUSIVE** - the entire hour is processed.

---

### 🎲 Jitter Configuration (Randomization)

```yaml
environment:
  # Random delay before each API call (prevents exact timing patterns)
  - JITTER_MIN_SECONDS=${CRAWLER_JITTER_MIN:-30}
  - JITTER_MAX_SECONDS=${CRAWLER_JITTER_MAX:-60}
```

**Purpose**: Adds 30-60 seconds random delay to make traffic patterns organic.

---

### ⚡ Task Configuration

```yaml
environment:
  # Check for new files every N seconds
  - TASK_INTERVAL_SECONDS=${CRAWLER_TASK_INTERVAL:-60}
  
  # Retry configuration
  - MAX_RETRIES=${CRAWLER_MAX_RETRIES:-3}
  - RETRY_DELAY_SECONDS=${CRAWLER_RETRY_DELAY:-300}
```

---

### 🗄️ Database Configuration

```yaml
environment:
  # Celery/Redis
  - CELERY_BROKER_URL=redis://redis:6379/2
  - CELERY_RESULT_BACKEND=redis://redis:6379/2
  
  # MongoDB
  - MONGODB_URI=mongodb://admin:password123@mongodb_test:27017
  - MONGODB_DB=search-engine
```

---

### 🔗 API Configuration

```yaml
environment:
  # Core service API endpoint
  - API_BASE_URL=http://core:3000
```

---

### 📊 Flower Dashboard Configuration

```yaml
crawler-flower:
  build: ./crawler-scheduler  # or image in production
  command: celery -A app.celery_app flower --port=5555
  ports:
    - "5555:5555"
  environment:
    - CELERY_BROKER_URL=redis://redis:6379/2
    - CELERY_RESULT_BACKEND=redis://redis:6379/2
    - FLOWER_BASIC_AUTH=${FLOWER_BASIC_AUTH:-admin:admin123}
```

**Access**: http://localhost:5555

---

## 📁 Using the Scheduler

### 1. Add Files for Processing

```bash
# Copy JSON files to pending directory
cp /path/to/your/domains/*.json ./crawler-scheduler/data/pending/

# Files are immediately visible in container (thanks to volumes!)
```

### 2. Monitor Processing

```bash
# View logs
docker logs -f crawler-scheduler-worker

# Check Flower dashboard
open http://localhost:5555

# Check file counts
ls -l crawler-scheduler/data/pending/    # Waiting to process
ls -l crawler-scheduler/data/processed/  # Successfully processed
ls -l crawler-scheduler/data/failed/     # Failed (for investigation)
```

### 3. Check Statistics

```bash
# View database statistics
docker exec mongodb_test mongosh --username admin --password password123 --eval "
use('search-engine');
db.crawler_scheduler_tracking.aggregate([
  { \$group: { _id: '\$status', count: { \$sum: 1 }}}
]);
"
```

---

## 🔧 Common Configuration Scenarios

### Scenario 1: Full Day Processing (Default - Ubuntu 24)

```yaml
environment:
  # No SCHEDULER_TIMEZONE set → Auto-detects Asia/Tehran from system
  - WARMUP_START_HOUR=0
  - WARMUP_END_HOUR=23
```

**Result**: Processes 00:00 - 23:59 in **Asia/Tehran** timezone ✅

---

### Scenario 2: Business Hours (US Eastern Time)

```yaml
environment:
  - SCHEDULER_TIMEZONE=America/New_York  # Override system timezone
  - WARMUP_START_HOUR=9   # 9 AM Eastern
  - WARMUP_END_HOUR=17    # 5 PM Eastern (through 17:59)
```

**Result**: Processes 09:00 - 17:59 in **America/New_York** timezone ✅

---

### Scenario 3: Limited Daily Window (2 hours)

```yaml
environment:
  # Uses system timezone (Asia/Tehran)
  - WARMUP_START_HOUR=10  # 10 AM
  - WARMUP_END_HOUR=12    # 12 PM (through 12:59)
  - WARMUP_SCHEDULE=50,100,200,400,800  # Progressive limits
```

**Result**: Processes 10:00 - 12:59 Tehran time, 50 files day 1, 100 day 2, etc. ✅

---

### Scenario 4: Disable Rate Limiting (Process Everything ASAP)

```yaml
environment:
  - WARMUP_ENABLED=false  # Disable all rate limiting
```

**Result**: Processes all pending files immediately, no daily limits ✅

---

### Scenario 5: Multiple Regions (Different Instances)

**Instance 1 (Tehran Server):**
```yaml
environment:
  # No override → uses system Asia/Tehran
  - WARMUP_START_HOUR=10
  - WARMUP_END_HOUR=12
```

**Instance 2 (New York Server):**
```yaml
environment:
  # No override → uses system America/New_York
  - WARMUP_START_HOUR=10
  - WARMUP_END_HOUR=12
```

**Result**: Each instance processes during local business hours ✅

---

## 🚀 Deployment Commands

### Development

```bash
# Start all services
docker-compose up -d

# Start only scheduler
docker-compose up -d crawler-scheduler crawler-flower

# Rebuild and start
docker-compose up --build -d crawler-scheduler

# View logs
docker-compose logs -f crawler-scheduler

# Restart
docker-compose restart crawler-scheduler crawler-flower
```

### Production

```bash
cd docker

# Start all services
docker-compose -f docker-compose.prod.yml up -d

# Pull latest images
docker-compose -f docker-compose.prod.yml pull

# Start with new images
docker-compose -f docker-compose.prod.yml up -d --force-recreate

# View logs
docker-compose -f docker-compose.prod.yml logs -f crawler-scheduler

# Scale workers (edit compose file first to add concurrency)
docker-compose -f docker-compose.prod.yml up -d --scale crawler-scheduler=2
```

---

## 🔍 Troubleshooting

### Check Timezone Detection

```bash
# View startup logs to see detected timezone
docker logs crawler-scheduler-worker 2>&1 | grep "Timezone:"

# Expected output:
# [Config] Timezone: Asia/Tehran (auto-detected from system /etc/timezone file)
```

### Check Current Time Window Status

```bash
# View recent logs
docker logs --tail 20 crawler-scheduler-worker | grep "time window"

# Outside window:
# Cannot process files: Outside processing window. Current: 08:30 (Asia/Tehran), Allowed: 10:00-12:59

# Inside window:
# Can process. Progress: 5/50, Remaining: 45 (Day 1)
```

### Verify Volume Mounting

```bash
# Add test file on host
echo '{"test": "data"}' > crawler-scheduler/data/pending/test.json

# Check if visible in container
docker exec crawler-scheduler-worker ls /app/data/pending/

# Should show: test.json ✅
```

### Check Resource Usage

```bash
# View container stats
docker stats crawler-scheduler-worker crawler-scheduler-flower

# Check resource limits (production)
docker inspect crawler-scheduler-worker | grep -A 10 "Memory"
```

---

## 📚 Related Documentation

- **Main README**: `crawler-scheduler/README.md`
- **Quick Start**: `crawler-scheduler/QUICKSTART.md`
- **Timezone Guide**: `crawler-scheduler/TIMEZONE_CONFIGURATION.md`
- **Timezone Detection**: `crawler-scheduler/TIMEZONE_DETECTION.md`
- **Integration Guide**: `crawler-scheduler/INTEGRATION.md`
- **Time Window Fix**: `crawler-scheduler/TIME_WINDOW_FIX.md`

---

## ✅ Summary

Both docker-compose files are now updated with:

✅ **Timezone auto-detection** from Ubuntu 24 system  
✅ **Optional timezone override** via environment variables  
✅ **Comprehensive configuration** options documented  
✅ **Volume mappings** for data persistence  
✅ **Flower dashboard** for monitoring  
✅ **Production-ready** with resource limits  
✅ **Development-friendly** with hot reload  

**Ready to use!** Just start the services and add your JSON files to `./crawler-scheduler/data/pending/`

