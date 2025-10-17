# Quick Start Guide

Get up and running with the Crawler Scheduler in 5 minutes.

## Prerequisites

- Docker and Docker Compose installed
- Core API service running at `http://localhost:3000`
- MongoDB running (for tracking)
- Redis running (for task queue)

## 1. Start the Scheduler

### Option A: Using Helper Script (Recommended)

```bash
cd crawler-scheduler
./scripts/start.sh
```

### Option B: Manual Start

```bash
cd crawler-scheduler

# Create network if needed
docker network create search-engine-network

# Build and start
docker build -t crawler-scheduler:latest .
docker-compose up -d
```

## 2. Verify Services

```bash
# Check status
./scripts/status.sh

# Or manually:
docker ps | grep crawler
```

You should see:
- `crawler-scheduler-worker` (running)
- `crawler-scheduler-flower` (running)

## 3. Access Flower Dashboard

Open your browser: **http://localhost:5555**

- Username: `admin`
- Password: `admin123`

## 4. Add Files to Process

### Use Example File

```bash
# Add example file
cp data/pending/example_domain.json data/pending/test_001.json

# Add your own files
cp /path/to/your/domains/*.json data/pending/
```

### File Format

Your JSON files should match this structure:

```json
{
  "business_name": "Your Business",
  "website_url": "www.example.com",
  "owner_name": "Owner Name",
  "email": "owner@example.com",
  "phone": "1234567890",
  "location": {
    "latitude": 36.292088,
    "longitude": 59.592343
  },
  ...
}
```

## 5. Watch Processing

### View in Flower Dashboard

1. Go to **Tasks** tab
2. See real-time task execution
3. Click any task to see details

### View in Logs

```bash
# Follow worker logs
docker logs -f crawler-scheduler-worker

# Recent logs only
docker logs --tail 50 crawler-scheduler-worker
```

## 6. Check Results

### Processed Files

```bash
ls -l data/processed/
```

Successfully processed files are moved here.

### Failed Files

```bash
ls -l data/failed/
```

Failed files are moved here for investigation.

### MongoDB Tracking

```bash
docker exec mongodb_test mongosh --username admin --password password123 --eval "
use('search-engine');
db.crawler_scheduler_tracking.find().pretty();
"
```

## Understanding the Warm-up Schedule

The scheduler implements progressive rate limiting:

| Day | Files/Day | Time Window | Rate |
|-----|-----------|-------------|------|
| 1   | 50        | 10:00-12:00 | ~1 file every 2.4 min |
| 2   | 100       | 10:00-12:00 | ~1 file every 1.2 min |
| 3   | 200       | 10:00-12:00 | ~1 file every 36 sec |
| 4   | 400       | 10:00-12:00 | ~1 file every 18 sec |
| 5+  | 800       | 10:00-12:00 | ~1 file every 9 sec |

**Note**: 
- Days are counted from first processed file
- Processing only happens between 10:00-12:00
- Each request has 30-60 seconds random jitter

## Customizing Configuration

### Change Warm-up Schedule

Edit `docker-compose.yml`:

```yaml
environment:
  - WARMUP_SCHEDULE=10,25,50,100,200  # Custom schedule
```

### Change Time Window

```yaml
environment:
  - WARMUP_START_HOUR=8   # Start at 8 AM
  - WARMUP_END_HOUR=18    # End at 6 PM
```

### Disable Rate Limiting (Process Everything ASAP)

```yaml
environment:
  - WARMUP_ENABLED=false
```

### After Configuration Changes

```bash
docker-compose down
docker-compose up -d
```

## Common Tasks

### Add More Files

```bash
# Just copy files to pending directory
cp your_files/*.json data/pending/
```

Files are automatically picked up every 60 seconds.

### Manually Trigger Processing

In Flower dashboard:
1. Go to **Tasks** tab
2. Click **Execute Task**
3. Select `app.tasks.process_pending_files`
4. Click **Execute**

### View Statistics

```bash
# Use helper script
./scripts/status.sh

# Or in Flower dashboard, execute:
# Task: app.tasks.get_scheduler_status
```

### Reset Warm-up Schedule

⚠️ **Warning**: This clears all processing history!

In Flower dashboard:
1. Go to **Tasks** tab
2. Execute task: `app.tasks.reset_warmup_schedule`

### Stop Services

```bash
./scripts/stop.sh

# Or manually:
docker-compose down
```

## Troubleshooting

### No Files Being Processed

**Check 1: Are we in time window?**

```bash
docker logs --tail 10 crawler-scheduler-worker | grep "time window"
```

**Check 2: Daily limit reached?**

```bash
docker logs --tail 10 crawler-scheduler-worker | grep "Daily limit"
```

**Check 3: Files in pending directory?**

```bash
ls -l data/pending/*.json
```

### API Calls Failing

**Test API endpoint:**

```bash
./scripts/test_api.sh
```

**Check core service:**

```bash
docker ps | grep core
curl http://localhost:3000/api/v2/website-profile
```

### Services Not Starting

**Check logs:**

```bash
docker logs crawler-scheduler-worker
docker logs crawler-scheduler-flower
```

**Common issues:**
- Redis not running → Start Redis
- MongoDB not accessible → Check connection string
- Network not found → `docker network create search-engine-network`
- Port 5555 in use → Change port in docker-compose.yml

### Reset Everything

```bash
# Stop services
docker-compose down

# Clear data
rm -rf data/processed/*
rm -rf data/failed/*

# Clear database tracking
docker exec mongodb_test mongosh --username admin --password password123 --eval "
use('search-engine');
db.crawler_scheduler_tracking.deleteMany({});
"

# Restart
docker-compose up -d
```

## What Happens Next?

1. **Scheduler picks up file** from `data/pending/`
2. **Checks rate limits** (warm-up schedule, time window)
3. **Applies jitter** (30-60 sec random delay)
4. **Calls your API**: `POST /api/v2/website-profile`
5. **Your API processes** the domain:
   - Stores in database
   - Triggers crawler
   - **Sends email** to domain manager (your internal logic)
6. **Scheduler tracks result** in MongoDB
7. **Moves file** to `processed/` or `failed/`

## Monitoring

### Real-time Monitoring

**Flower Dashboard**: http://localhost:5555
- See active tasks
- View success/failure rates
- Monitor worker health
- Execute tasks manually

### Log Monitoring

```bash
# Follow logs
docker logs -f crawler-scheduler-worker

# Search logs
docker logs crawler-scheduler-worker | grep ERROR
docker logs crawler-scheduler-worker | grep SUCCESS
```

### Database Monitoring

```bash
# Get statistics
docker exec mongodb_test mongosh --username admin --password password123 --eval "
use('search-engine');
db.crawler_scheduler_tracking.aggregate([
  { \$group: { 
    _id: '\$status', 
    count: { \$sum: 1 } 
  }}
]).pretty();
"
```

## Next Steps

- **Add all 200 domains** to `data/pending/`
- **Monitor progress** in Flower dashboard
- **Adjust warm-up schedule** based on API performance
- **Set up alerts** for failed tasks (optional)
- **Integrate with main docker-compose** (see INTEGRATION.md)

## Getting Help

- **Check logs**: `docker logs crawler-scheduler-worker`
- **View Flower**: http://localhost:5555
- **Test API**: `./scripts/test_api.sh`
- **Check status**: `./scripts/status.sh`

---

**Ready to process your 200 domains? Just copy the JSON files to `data/pending/` and watch Flower! 🚀**

