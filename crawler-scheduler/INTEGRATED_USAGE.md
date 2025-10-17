# Crawler Scheduler - Integrated Usage Guide

The crawler scheduler has been integrated into the main and production docker-compose files.

## 🚀 Quick Start (Development)

### 1. Start All Services

```bash
cd /root/search-engine-core

# Start everything including scheduler
docker-compose up -d

# Or rebuild if needed
docker-compose up --build -d
```

### 2. Verify Scheduler is Running

```bash
# Check all services
docker-compose ps

# Check scheduler logs
docker logs -f crawler-scheduler-worker

# Check Flower UI logs
docker logs -f crawler-scheduler-flower
```

### 3. Access Flower Dashboard

Open: **http://localhost:5555**

- Username: `admin`
- Password: `admin123` (configurable)

### 4. Add Files to Process

```bash
# Copy your JSON files to the scheduler data directory
cp /path/to/your/domains/*.json ./crawler-scheduler/data/pending/
```

---

## 🔧 Configuration via Environment Variables

### Main `.env` File Configuration

Add these to your main `.env` file to customize the scheduler:

```bash
# Crawler Scheduler Configuration

# Warm-up Schedule (Progressive Rate Limiting)
CRAWLER_WARMUP_ENABLED=true
CRAWLER_WARMUP_SCHEDULE=50,100,200,400,800  # Day 1: 50, Day 2: 100, etc.
CRAWLER_WARMUP_START_HOUR=10  # Start processing at 10:00 AM
CRAWLER_WARMUP_END_HOUR=12    # Stop processing at 12:00 PM

# Jitter (Random Delay)
CRAWLER_JITTER_MIN=30   # Minimum random delay (seconds)
CRAWLER_JITTER_MAX=60   # Maximum random delay (seconds)

# Task Configuration
CRAWLER_TASK_INTERVAL=60   # Check for new files every 60 seconds
CRAWLER_MAX_RETRIES=3      # Retry failed API calls 3 times
CRAWLER_RETRY_DELAY=300    # Wait 5 minutes between retries

# Flower Authentication
FLOWER_BASIC_AUTH=admin:your_secure_password_here
```

### Available Configuration Options

| Variable | Default | Description |
|----------|---------|-------------|
| `CRAWLER_WARMUP_ENABLED` | `true` | Enable progressive rate limiting |
| `CRAWLER_WARMUP_SCHEDULE` | `50,100,200,400,800` | Daily limits per day |
| `CRAWLER_WARMUP_START_HOUR` | `10` | Start hour (24h format) |
| `CRAWLER_WARMUP_END_HOUR` | `12` | End hour (24h format) |
| `CRAWLER_JITTER_MIN` | `30` | Minimum random delay (seconds) |
| `CRAWLER_JITTER_MAX` | `60` | Maximum random delay (seconds) |
| `CRAWLER_TASK_INTERVAL` | `60` | Check interval (seconds) |
| `CRAWLER_MAX_RETRIES` | `3` | API call retry attempts |
| `CRAWLER_RETRY_DELAY` | `300` | Retry delay (seconds) |
| `FLOWER_BASIC_AUTH` | `admin:admin123` | Flower dashboard credentials |

---

## 📊 Monitoring

### Flower Web Dashboard

Access: **http://localhost:5555**

Features:
- Real-time task monitoring
- Worker health status
- Task history and statistics
- Manual task execution
- Task retry controls

### Docker Logs

```bash
# Worker logs (processing)
docker logs -f crawler-scheduler-worker

# Flower logs (UI)
docker logs -f crawler-scheduler-flower

# Follow all scheduler logs
docker-compose logs -f crawler-scheduler crawler-flower
```

### Database Monitoring

```bash
# Check processing statistics
docker exec mongodb_test mongosh --username admin --password password123 --eval "
use('search-engine');
db.crawler_scheduler_tracking.aggregate([
  { \$group: { _id: '\$status', count: { \$sum: 1 }}}
]);
"
```

---

## 🔄 Common Operations

### Start/Stop Scheduler Only

```bash
# Stop scheduler services
docker-compose stop crawler-scheduler crawler-flower

# Start scheduler services
docker-compose start crawler-scheduler crawler-flower

# Restart scheduler services
docker-compose restart crawler-scheduler crawler-flower
```

### View Status

```bash
# Check service status
docker-compose ps crawler-scheduler crawler-flower

# Check resource usage
docker stats crawler-scheduler-worker crawler-scheduler-flower
```

### Update Configuration

```bash
# 1. Edit .env file with new values
nano .env

# 2. Restart scheduler to apply changes
docker-compose restart crawler-scheduler crawler-flower
```

### Scale Workers (Process More Files in Parallel)

```bash
# Edit docker-compose.yml, change concurrency:
# command: celery -A app.celery_app worker --beat --loglevel=info --concurrency=4

# Then restart
docker-compose restart crawler-scheduler
```

---

## 📁 File Management

### File Locations

```
crawler-scheduler/data/
├── pending/      # Place JSON files here
├── processed/    # Successfully processed files
└── failed/       # Failed files for investigation
```

### Add Files

```bash
# Copy files to pending directory
cp your_files/*.json crawler-scheduler/data/pending/

# Or move files
mv your_files/*.json crawler-scheduler/data/pending/
```

### Check File Status

```bash
# Count pending files
ls -1 crawler-scheduler/data/pending/*.json | wc -l

# Count processed files
ls -1 crawler-scheduler/data/processed/*.json | wc -l

# Count failed files
ls -1 crawler-scheduler/data/failed/*.json | wc -l
```

### Clean Up Old Files

```bash
# Archive processed files older than 30 days
find crawler-scheduler/data/processed -name "*.json" -mtime +30 -exec mv {} /backup/archive/ \;

# Remove failed files older than 7 days (after investigation)
find crawler-scheduler/data/failed -name "*.json" -mtime +7 -delete
```

---

## 🐛 Troubleshooting

### Scheduler Not Processing Files

**Check 1: Is it in time window?**
```bash
docker logs --tail 20 crawler-scheduler-worker | grep "time window"
```

**Check 2: Daily limit reached?**
```bash
docker logs --tail 20 crawler-scheduler-worker | grep "Daily limit"
```

**Check 3: Files in pending directory?**
```bash
ls -l crawler-scheduler/data/pending/
```

### API Calls Failing

**Check 1: Core service running?**
```bash
docker ps | grep core
curl http://localhost:3000/health || echo "Core service not responding"
```

**Check 2: Network connectivity?**
```bash
docker exec crawler-scheduler-worker curl -I http://core:3000
```

### Reset Everything

```bash
# Stop scheduler
docker-compose stop crawler-scheduler crawler-flower

# Clear tracking database
docker exec mongodb_test mongosh --username admin --password password123 --eval "
use('search-engine');
db.crawler_scheduler_tracking.deleteMany({});
"

# Clear data directories
rm -rf crawler-scheduler/data/processed/*
rm -rf crawler-scheduler/data/failed/*

# Restart scheduler
docker-compose start crawler-scheduler crawler-flower
```

---

## 🚀 Production Deployment

### Using Production Docker Compose

```bash
cd /root/search-engine-core/docker

# Set production environment variables in .env file
# Make sure to set:
# - MONGODB_URI
# - FLOWER_BASIC_AUTH (strong password!)
# - CRAWLER_WARMUP_SCHEDULE (based on your needs)

# Deploy
docker-compose -f docker-compose.prod.yml up -d

# Access Flower at configured port (default: 5555)
```

### Production Environment Variables

Add to production `.env`:

```bash
# MongoDB (required)
MONGODB_URI=mongodb://user:password@your-mongo-host:27017

# MongoDB Database
MONGODB_DB=search-engine

# API Base URL (required)
API_BASE_URL=http://search-engine-core:3000

# Flower Authentication (REQUIRED - change this!)
FLOWER_BASIC_AUTH=admin:your_very_strong_password_here

# Optional: Custom port for Flower
FLOWER_PORT=5555

# Celery Configuration (optional)
CELERY_BROKER_URL=redis://redis:6379/2
CELERY_RESULT_BACKEND=redis://redis:6379/2

# Warm-up Schedule (adjust based on your needs)
CRAWLER_WARMUP_ENABLED=true
CRAWLER_WARMUP_SCHEDULE=50,100,200,400,800
CRAWLER_WARMUP_START_HOUR=10
CRAWLER_WARMUP_END_HOUR=12
```

### Production Security Checklist

- [ ] Change `FLOWER_BASIC_AUTH` to strong credentials
- [ ] Set up firewall rules for port 5555
- [ ] Enable TLS/SSL for Flower (use reverse proxy)
- [ ] Set up log aggregation
- [ ] Configure monitoring/alerting
- [ ] Set up backup for MongoDB tracking collection
- [ ] Restrict network access to scheduler services

---

## 📈 Scaling in Production

### Multiple Workers

Edit `docker-compose.prod.yml`:

```yaml
crawler-scheduler:
  command: celery -A app.celery_app worker --beat --loglevel=warning --concurrency=4
  # Process 4 files simultaneously
```

### Separate Beat Scheduler (Recommended for Production)

```yaml
# Worker (no beat)
crawler-scheduler-worker:
  command: celery -A app.celery_app worker --loglevel=warning --concurrency=4

# Dedicated scheduler
crawler-scheduler-beat:
  command: celery -A app.celery_app beat --loglevel=warning
```

---

## 📚 Additional Resources

- **Full Documentation**: See `crawler-scheduler/README.md`
- **Quick Start Guide**: See `crawler-scheduler/QUICKSTART.md`
- **Integration Details**: See `crawler-scheduler/INTEGRATION.md`
- **Project Overview**: See `crawler-scheduler/PROJECT_OVERVIEW.md`

---

## 🎯 Example: Process 200 Domains

### Step 1: Prepare Your JSON Files

```bash
# Copy all 200 domain files
cp /path/to/200-domains/*.json crawler-scheduler/data/pending/
```

### Step 2: Start Services

```bash
docker-compose up -d
```

### Step 3: Monitor Progress

```bash
# Open Flower dashboard
# http://localhost:5555

# Or watch logs
docker logs -f crawler-scheduler-worker
```

### Step 4: Check Progress

```bash
# File counts
echo "Pending: $(ls -1 crawler-scheduler/data/pending/*.json 2>/dev/null | wc -l)"
echo "Processed: $(ls -1 crawler-scheduler/data/processed/*.json 2>/dev/null | wc -l)"
echo "Failed: $(ls -1 crawler-scheduler/data/failed/*.json 2>/dev/null | wc -l)"
```

### Expected Timeline (with default warm-up)

- **Day 1**: Process 50 files (10:00-12:00)
- **Day 2**: Process 100 files (10:00-12:00)
- **Day 3**: Process 50 remaining files (10:00-12:00)
- **Total**: All 200 domains processed in 3 days

---

## 💡 Tips

1. **Disable rate limiting for testing**: Set `CRAWLER_WARMUP_ENABLED=false`
2. **Speed up processing**: Reduce `CRAWLER_TASK_INTERVAL` to 30 seconds
3. **Monitor in real-time**: Keep Flower dashboard open during processing
4. **Check failed files**: Investigate files in `data/failed/` for issues
5. **Backup tracking data**: Periodically backup the MongoDB collection

---

## ✅ Integration Complete!

Your crawler scheduler is now fully integrated with the main project. Just:

1. **Start services**: `docker-compose up -d`
2. **Add files**: Copy JSON files to `crawler-scheduler/data/pending/`
3. **Monitor**: Open http://localhost:5555
4. **Done**: Files process automatically according to schedule

🎉 Happy scheduling!

