# ✅ Crawler Scheduler Integration - Complete!

The crawler scheduler has been successfully integrated into both development and production docker-compose files.

---

## 🎉 What Was Added

### Services Added

1. **`crawler-scheduler`** - Celery worker + Beat scheduler
   - Processes JSON files from directory
   - Progressive warm-up rate limiting (50→100→200→400→800)
   - Calls `/api/v2/website-profile` endpoint
   - MongoDB duplicate prevention
   - Automatic file management (moves to processed/failed)

2. **`crawler-flower`** - Web monitoring dashboard
   - Real-time task monitoring
   - Worker health checks
   - Task history and statistics
   - Accessible at http://localhost:5555

### Files Modified

✅ `/root/search-engine-core/docker-compose.yml` - Development configuration  
✅ `/root/search-engine-core/docker/docker-compose.prod.yml` - Production configuration

### New Documentation

✅ `crawler-scheduler/INTEGRATED_USAGE.md` - Integration usage guide  
✅ All existing scheduler documentation remains valid

---

## 🚀 Quick Start (Development)

### 1. Start All Services

```bash
cd /root/search-engine-core

# Start everything (including scheduler)
docker-compose up -d

# Check services are running
docker-compose ps
```

You should see:

- ✅ `core` - Main search engine
- ✅ `mongodb_test` - MongoDB
- ✅ `redis` - Redis
- ✅ `browserless` - Chrome
- ✅ `js-minifier` - JS minifier
- ✅ `redis-sync` - MongoDB to Redis synchronization service
- ✅ **`crawler-scheduler-worker`** ← NEW
- ✅ **`crawler-scheduler-flower`** ← NEW

### 2. Access Flower Dashboard

Open: **http://localhost:5555**

- Username: `admin`
- Password: `admin123`

### 3. Add Your 200 Domain Files

```bash
# Copy JSON files to pending directory
cp /path/to/your/200-domains/*.json crawler-scheduler/data/pending/
```

### 4. Monitor Processing

```bash
# Watch logs
docker logs -f crawler-scheduler-worker

# Or use Flower dashboard
# http://localhost:5555
```

---

## ⚙️ Configuration (Optional)

### Customize via `.env` File

Add to your main `.env` file:

```bash
# Crawler Scheduler Configuration

# Warm-up Schedule
CRAWLER_WARMUP_ENABLED=true
CRAWLER_WARMUP_SCHEDULE=50,100,200,400,800  # Daily limits
CRAWLER_WARMUP_START_HOUR=10  # Start at 10:00 AM
CRAWLER_WARMUP_END_HOUR=12    # End at 12:00 PM

# Jitter (Random Delay)
CRAWLER_JITTER_MIN=30   # Min delay (seconds)
CRAWLER_JITTER_MAX=60   # Max delay (seconds)

# Task Settings
CRAWLER_TASK_INTERVAL=60   # Check every 60 seconds
CRAWLER_MAX_RETRIES=3      # Retry 3 times on failure
CRAWLER_RETRY_DELAY=300    # Wait 5 min between retries

# Flower Authentication (Change this!)
FLOWER_BASIC_AUTH=admin:your_secure_password
```

After editing `.env`:

```bash
docker-compose restart crawler-scheduler crawler-flower
```

---

## 📊 Default Behavior

### Without Configuration

If you don't set any environment variables, the scheduler uses these defaults:

| Setting        | Default            | Behavior                         |
| -------------- | ------------------ | -------------------------------- |
| Warm-up        | Enabled            | Progressive rate limiting active |
| Schedule       | 50,100,200,400,800 | Day 1: 50, Day 2: 100, etc.      |
| Time Window    | 10:00-12:00        | Only process during this time    |
| Jitter         | 30-60 seconds      | Random delay before each request |
| Check Interval | 60 seconds         | Check for new files every minute |
| Retries        | 3 attempts         | Retry failed API calls 3 times   |

### With Your 200 Domains

**Timeline with defaults:**

- **Day 1 (10:00-12:00)**: Process 50 files → 50 total
- **Day 2 (10:00-12:00)**: Process 100 files → 150 total
- **Day 3 (10:00-12:00)**: Process 50 remaining → **200 total ✓**

**To process faster:**

```bash
# In .env file
CRAWLER_WARMUP_ENABLED=false  # Disable rate limiting
CRAWLER_TASK_INTERVAL=30       # Check every 30 seconds

# Restart
docker-compose restart crawler-scheduler
```

---

## 🔍 Monitoring Options

### 1. Flower Web Dashboard (Recommended)

**URL**: http://localhost:5555  
**Features**:

- Real-time task monitoring
- Success/failure graphs
- Worker health status
- Manual task execution
- Task retry controls

### 2. Docker Logs

```bash
# Worker logs (processing)
docker logs -f crawler-scheduler-worker

# Flower logs (UI)
docker logs -f crawler-scheduler-flower

# Follow both
docker-compose logs -f crawler-scheduler crawler-flower
```

### 3. File Counts

```bash
# Quick status
echo "Pending: $(ls -1 crawler-scheduler/data/pending/*.json 2>/dev/null | wc -l)"
echo "Processed: $(ls -1 crawler-scheduler/data/processed/*.json 2>/dev/null | wc -l)"
echo "Failed: $(ls -1 crawler-scheduler/data/failed/*.json 2>/dev/null | wc -l)"
```

### 4. MongoDB Stats

```bash
docker exec mongodb_test mongosh --username admin --password password123 --eval "
use('search-engine');
db.crawler_scheduler_tracking.aggregate([
  { \$group: { _id: '\$status', count: { \$sum: 1 }}}
]);
"
```

---

## 🔧 Common Operations

### Start/Stop Scheduler Only

```bash
# Stop scheduler (keeps other services running)
docker-compose stop crawler-scheduler crawler-flower

# Start scheduler
docker-compose start crawler-scheduler crawler-flower

# Restart scheduler
docker-compose restart crawler-scheduler crawler-flower
```

### Disable Scheduler Temporarily

```bash
# Edit docker-compose.yml, comment out scheduler services
# Or just stop them:
docker-compose stop crawler-scheduler crawler-flower
```

### Check Scheduler Status

```bash
# Service status
docker-compose ps crawler-scheduler crawler-flower

# Resource usage
docker stats crawler-scheduler-worker crawler-scheduler-flower

# Recent logs
docker logs --tail 50 crawler-scheduler-worker
```

---

## 🚀 Production Deployment

### Using Production Compose

```bash
cd /root/search-engine-core/docker

# Create production .env file with required variables
cat > .env << EOF
# Required for production
MONGODB_URI=mongodb://user:password@your-mongo-host:27017
API_BASE_URL=http://search-engine-core:3000
FLOWER_BASIC_AUTH=admin:your_very_strong_password

# Optional customization
CRAWLER_WARMUP_SCHEDULE=50,100,200,400,800
CRAWLER_WARMUP_START_HOUR=10
CRAWLER_WARMUP_END_HOUR=12
EOF

# Deploy
docker-compose -f docker-compose.prod.yml up -d
```

### Production Features

✅ **Production image**: Uses `ghcr.io/hatefsystems/search-engine-core/crawler-scheduler:latest`  
✅ **Resource limits**: 512MB RAM, 0.5 CPU (optimized for 8GB server)  
✅ **Concurrency**: Processes 2 files simultaneously  
✅ **Logging**: JSON file driver with rotation (10MB max, 3 files)  
✅ **Named volume**: Data persisted in `crawler_data` volume  
✅ **Production logging**: Warning level (less verbose)

---

## 📁 File Structure

```
/root/search-engine-core/
├── docker-compose.yml                         # ✅ MODIFIED (includes scheduler)
├── docker/
│   └── docker-compose.prod.yml                # ✅ MODIFIED (includes scheduler)
├── crawler-scheduler/                         # ✅ NEW (scheduler service)
│   ├── app/                                   # Python application
│   ├── data/
│   │   ├── pending/       ← Place JSON files here
│   │   ├── processed/     ← Successfully processed
│   │   └── failed/        ← Failed files
│   ├── scripts/                               # Helper scripts
│   ├── Dockerfile
│   ├── docker-compose.yml                     # Standalone (optional)
│   ├── requirements.txt
│   ├── README.md                              # Full documentation
│   ├── QUICKSTART.md                          # 5-minute guide
│   ├── INTEGRATION.md                         # Integration details
│   ├── INTEGRATED_USAGE.md                    # ✅ NEW (usage after integration)
│   └── PROJECT_OVERVIEW.md                    # Architecture overview
└── SCHEDULER_INTEGRATION_SUMMARY.md           # ✅ NEW (this file)
```

---

## 🐛 Troubleshooting

### Scheduler Not Starting

```bash
# Check logs
docker logs crawler-scheduler-worker

# Common issues:
# 1. Redis not running → docker-compose ps redis
# 2. MongoDB not accessible → docker-compose ps mongodb
# 3. Network issues → docker network inspect search-network
```

### Files Not Being Processed

```bash
# Check if in time window
docker logs --tail 10 crawler-scheduler-worker | grep "time window"

# Check daily limit
docker logs --tail 10 crawler-scheduler-worker | grep "Daily limit"

# Disable rate limiting for testing
echo "CRAWLER_WARMUP_ENABLED=false" >> .env
docker-compose restart crawler-scheduler
```

### API Calls Failing

```bash
# Test API endpoint
curl -X POST http://localhost:3000/api/v2/website-profile \
  -H "Content-Type: application/json" \
  -d '{"test": "data"}'

# Check core service
docker-compose ps search-engine

# Check network connectivity
docker exec crawler-scheduler-worker curl -I http://core:3000
```

---

## 📚 Documentation

| Document                           | Description                                    |
| ---------------------------------- | ---------------------------------------------- |
| **`INTEGRATED_USAGE.md`**          | Usage guide after integration ← **Start here** |
| `README.md`                        | Comprehensive documentation                    |
| `QUICKSTART.md`                    | 5-minute setup guide                           |
| `INTEGRATION.md`                   | Integration technical details                  |
| `PROJECT_OVERVIEW.md`              | Architecture and features                      |
| `SCHEDULER_INTEGRATION_SUMMARY.md` | This file (overview)                           |

---

## ✅ Integration Checklist

- [x] Scheduler services added to `docker-compose.yml`
- [x] Scheduler services added to `docker-compose.prod.yml`
- [x] Configuration via environment variables
- [x] Documentation created
- [x] Docker compose files validated
- [x] Services properly networked
- [x] Resource limits set (production)
- [x] Logging configured
- [x] Volume mounts configured
- [x] Dependencies configured

---

## 🎯 Next Steps

### For Development

1. **Start services**: `docker-compose up -d`
2. **Add test file**: `cp crawler-scheduler/data/pending/example_domain.json crawler-scheduler/data/pending/test.json`
3. **Open Flower**: http://localhost:5555
4. **Watch it process**: Monitor in Flower or logs

### For Production

1. **Build scheduler image**: `docker build -t ghcr.io/hatefsystems/search-engine-core/crawler-scheduler:latest crawler-scheduler/`
2. **Push to registry**: `docker push ghcr.io/hatefsystems/search-engine-core/crawler-scheduler:latest`
3. **Set production env vars**: Edit `docker/.env`
4. **Deploy**: `docker-compose -f docker/docker-compose.prod.yml up -d`

### For Your 200 Domains

1. **Copy JSON files**: `cp /path/to/domains/*.json crawler-scheduler/data/pending/`
2. **Start services**: `docker-compose up -d`
3. **Monitor progress**: Open http://localhost:5555
4. **Wait**: Files process automatically according to schedule

---

## 💡 Pro Tips

1. **Test with rate limiting disabled** first to verify API works
2. **Use Flower dashboard** for best monitoring experience
3. **Check failed files** in `data/failed/` to debug issues
4. **Backup MongoDB tracking collection** periodically
5. **Set strong password** for Flower in production
6. **Monitor disk space** in `data/` directories
7. **Use log aggregation** in production (ELK, Loki, etc.)

---

## 🎉 Success!

Your crawler scheduler is now fully integrated with your search engine core project!

**Everything is ready to process your 200 domains automatically with progressive warm-up rate limiting.**

Just:

1. Start: `docker-compose up -d`
2. Add files: Copy to `crawler-scheduler/data/pending/`
3. Monitor: http://localhost:5555
4. Done: Sit back and watch the magic happen! ✨

---

## 📞 Support

- **Quick Status**: `docker-compose ps`
- **View Logs**: `docker logs -f crawler-scheduler-worker`
- **Flower Dashboard**: http://localhost:5555
- **Full Docs**: See `crawler-scheduler/README.md`

Happy scheduling! 🚀
