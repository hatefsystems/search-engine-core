# Crawler Scheduler - Project Overview

## 📋 Summary

Production-ready **Celery + Flower** scheduler system for automated crawler task management with progressive warm-up rate limiting.

**Created**: October 17, 2025  
**Language**: Python 3.11  
**Framework**: Celery + Redis + MongoDB  
**UI**: Flower Web Dashboard  
**Status**: ✅ Ready for Production

---

## 🎯 Requirements Implemented

✅ **Task runs every 1 minute** - Configurable via `TASK_INTERVAL_SECONDS`  
✅ **Progressive warm-up** - Stair-step scheduling (50→100→200→400→800)  
✅ **Time window control** - Only process between 10:00-12:00 (configurable)  
✅ **Jitter/randomization** - ±30-60 seconds delay to avoid exact timing  
✅ **File-based processing** - Read JSON files from directory  
✅ **API integration** - Call `http://localhost:3000/api/v2/website-profile`  
✅ **Duplicate prevention** - MongoDB tracking, no re-processing  
✅ **File management** - Auto-move to processed/failed folders  
✅ **Web UI** - Beautiful Flower dashboard for monitoring  
✅ **Docker containerized** - Easy deployment and scaling  

---

## 📁 Project Structure

```
crawler-scheduler/
├── app/
│   ├── __init__.py           # Package initializer
│   ├── celery_app.py         # Celery configuration (44 lines)
│   ├── config.py             # Environment configuration (57 lines)
│   ├── database.py           # MongoDB tracking (164 lines)
│   ├── file_processor.py     # File processing logic (193 lines)
│   ├── rate_limiter.py       # Warm-up rate limiting (116 lines)
│   └── tasks.py              # Celery tasks (160 lines)
│
├── data/
│   ├── pending/              # Place JSON files here
│   ├── processed/            # Successfully processed files
│   └── failed/               # Failed files
│
├── scripts/
│   ├── start.sh              # Quick start script
│   ├── stop.sh               # Stop services
│   ├── status.sh             # Check status
│   └── test_api.sh           # Test API endpoint
│
├── Dockerfile                # Container definition
├── docker-compose.yml        # Service orchestration
├── requirements.txt          # Python dependencies
├── README.md                 # Full documentation
├── QUICKSTART.md             # 5-minute setup guide
├── INTEGRATION.md            # Integration guide
└── PROJECT_OVERVIEW.md       # This file

Total Python Code: 736 lines
```

---

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                      Crawler Scheduler System                    │
└─────────────────────────────────────────────────────────────────┘

┌──────────────┐     ┌────────────────┐     ┌─────────────────┐
│  JSON Files  │────>│ Celery Worker  │────>│  Core C++ API   │
│ data/pending │     │ (File Proc.)   │     │ :3000/api/v2/.. │
└──────────────┘     └────────────────┘     └─────────────────┘
                            │                        │
                            │                        │
                            ▼                        ▼
                     ┌─────────────┐         ┌─────────────┐
                     │   Redis     │         │  MongoDB    │
                     │ (Task Queue)│         │ (Tracking)  │
                     └─────────────┘         └─────────────┘
                            │
                            │
                            ▼
                     ┌─────────────┐
                     │   Flower    │
                     │  :5555 UI   │
                     └─────────────┘
```

---

## 🔄 Processing Flow

```
1. File Monitoring (Every 60 seconds)
   └─> Celery Beat triggers: process_pending_files()

2. Rate Limiter Check
   ├─> In time window? (10:00-12:00)
   ├─> Under daily limit? (Day 1: 50, Day 2: 100, etc.)
   └─> Can process? → Continue : Skip

3. File Selection
   └─> Pick first unprocessed file from data/pending/

4. Processing Pipeline
   ├─> Parse JSON
   ├─> Check MongoDB (already processed?)
   ├─> Mark as "processing" (atomic)
   ├─> Apply jitter (30-60 sec delay)
   ├─> POST to API
   └─> Update status

5. Result Handling
   ├─> Success:
   │   ├─> Mark "processed" in MongoDB
   │   └─> Move file to data/processed/
   │
   └─> Failure:
       ├─> Mark "failed" in MongoDB
       └─> Move file to data/failed/
```

---

## ⚙️ Configuration

### Core Settings

| Setting | Default | Description |
|---------|---------|-------------|
| `WARMUP_ENABLED` | `true` | Enable progressive rate limiting |
| `WARMUP_SCHEDULE` | `50,100,200,400,800` | Daily limits per day |
| `WARMUP_START_HOUR` | `10` | Start processing at 10:00 |
| `WARMUP_END_HOUR` | `12` | Stop processing at 12:00 |
| `JITTER_MIN_SECONDS` | `30` | Minimum random delay |
| `JITTER_MAX_SECONDS` | `60` | Maximum random delay |
| `TASK_INTERVAL_SECONDS` | `60` | Check interval (1 minute) |

### Warm-up Schedule Breakdown

| Day | Files/Day | Processing Window | Average Rate | Total Duration |
|-----|-----------|-------------------|--------------|----------------|
| 1   | 50        | 10:00-12:00 (2h)  | 1 every 2.4 min | 2 hours |
| 2   | 100       | 10:00-12:00 (2h)  | 1 every 1.2 min | 2 hours |
| 3   | 200       | 10:00-12:00 (2h)  | 1 every 36 sec  | 2 hours |
| 4   | 400       | 10:00-12:00 (2h)  | 1 every 18 sec  | 2 hours |
| 5+  | 800       | 10:00-12:00 (2h)  | 1 every 9 sec   | 2 hours |

**With 200 files:**
- Day 1: Process 50 files
- Day 2: Process 100 files
- Day 3: Process 50 remaining files (all done!)

---

## 🚀 Deployment

### Quick Start (Standalone)

```bash
cd crawler-scheduler
./scripts/start.sh
```

### Production (Integrated)

Add to main `docker-compose.yml`:

```yaml
services:
  crawler-scheduler:
    build: ./crawler-scheduler
    # ... configuration ...
  
  crawler-flower:
    build: ./crawler-scheduler
    # ... configuration ...
```

---

## 📊 Monitoring

### Flower Dashboard

**URL**: http://localhost:5555  
**Auth**: admin / admin123

Features:
- ✅ Real-time task monitoring
- ✅ Worker health checks
- ✅ Task history and statistics
- ✅ Manual task execution
- ✅ Success/failure graphs
- ✅ Task retry controls

### Log Monitoring

```bash
# Follow live logs
docker logs -f crawler-scheduler-worker

# View recent logs
docker logs --tail 50 crawler-scheduler-worker

# Search for errors
docker logs crawler-scheduler-worker | grep ERROR
```

### Database Monitoring

```bash
# View processing statistics
docker exec mongodb_test mongosh --username admin --password password123 --eval "
use('search-engine');
db.crawler_scheduler_tracking.aggregate([
  { \$group: { _id: '\$status', count: { \$sum: 1 }}}
]);
"
```

---

## 🗄️ Data Storage

### MongoDB Collection: `crawler_scheduler_tracking`

```javascript
{
  _id: ObjectId("..."),
  filename: "domain_123.json",          // Unique index
  status: "processed",                  // processing | processed | failed
  file_data: { business_name: "...", ...},  // Original JSON
  started_at: ISODate("2025-10-17T10:15:30Z"),
  processed_at: ISODate("2025-10-17T10:16:45Z"),
  attempts: 1,
  api_response: { success: true, ...},  // API response
  error_message: null
}
```

### File System

```
data/
├── pending/          # Input: Place 200 JSON files here
├── processed/        # Output: Successfully processed files
└── failed/           # Output: Failed files for investigation
```

---

## 🧪 Testing

### Test API Endpoint

```bash
./scripts/test_api.sh
```

### Test with Sample File

```bash
# Use example file
cp data/pending/example_domain.json data/pending/test_001.json

# Watch processing in real-time
docker logs -f crawler-scheduler-worker
```

### Manual Task Execution

In Flower dashboard (http://localhost:5555):
1. Go to **Tasks** tab
2. Click **Execute Task**
3. Select `app.tasks.process_pending_files`
4. Click **Execute**

---

## 📦 Dependencies

```
celery[redis]==5.3.4    # Task queue
flower==2.0.1           # Web monitoring UI
redis==5.0.1            # Message broker
pymongo==4.6.1          # MongoDB driver
requests==2.31.0        # HTTP client
python-dotenv==1.0.0    # Environment config
jdatetime==4.1.1        # Persian date support
```

---

## 🔧 Customization Examples

### Disable Rate Limiting (Process Everything ASAP)

```yaml
environment:
  - WARMUP_ENABLED=false
```

### Change Time Window (8 AM - 6 PM)

```yaml
environment:
  - WARMUP_START_HOUR=8
  - WARMUP_END_HOUR=18
```

### Custom Warm-up Schedule

```yaml
environment:
  - WARMUP_SCHEDULE=10,25,50,100,200
```

### Faster Processing (Check every 30 seconds)

```yaml
environment:
  - TASK_INTERVAL_SECONDS=30
```

---

## 🛠️ Available Tasks

| Task | Description | Usage |
|------|-------------|-------|
| `process_pending_files` | Main periodic task | Auto-runs every 60s |
| `get_scheduler_status` | Get current status | Manual execution |
| `process_single_file` | Process specific file | Manual execution |
| `reset_warmup_schedule` | Clear processing history | Manual execution |

---

## 🐛 Troubleshooting

### Issue: No files being processed

**Cause**: Outside time window or daily limit reached

**Solution**: Check logs for rate limiter status
```bash
docker logs --tail 20 crawler-scheduler-worker | grep "Rate limiter"
```

### Issue: API calls failing

**Cause**: Core service not running or wrong URL

**Solution**: Test API endpoint
```bash
./scripts/test_api.sh
```

### Issue: Files not moving to processed folder

**Cause**: Permission issues or path problems

**Solution**: Check volume mounts in docker-compose.yml
```bash
docker exec crawler-scheduler-worker ls -la /app/data/
```

---

## 📈 Scaling Options

### Multiple Workers

```yaml
crawler-scheduler:
  command: celery -A app.celery_app worker --concurrency=4
  # Process 4 files simultaneously
```

### Separate Beat Scheduler

```yaml
crawler-worker:
  command: celery -A app.celery_app worker --concurrency=2

crawler-beat:
  command: celery -A app.celery_app beat
  # Dedicated scheduler, no processing
```

### Multiple Worker Containers

```yaml
crawler-worker-1:
  build: ./crawler-scheduler
  command: celery -A app.celery_app worker

crawler-worker-2:
  build: ./crawler-scheduler
  command: celery -A app.celery_app worker
```

---

## 🔒 Security Checklist

- [ ] Change Flower password (default: admin/admin123)
- [ ] Enable TLS for Flower dashboard
- [ ] Restrict Flower port with firewall
- [ ] Use Docker secrets for credentials
- [ ] Enable Redis password protection
- [ ] Configure MongoDB authentication
- [ ] Set up network policies
- [ ] Enable audit logging

---

## 📚 Documentation

- **README.md** - Comprehensive documentation
- **QUICKSTART.md** - 5-minute setup guide
- **INTEGRATION.md** - Integration with main project
- **PROJECT_OVERVIEW.md** - This file (high-level overview)

---

## 🎓 Key Features Explained

### Progressive Warm-up

Gradually increases load to avoid overwhelming API or triggering rate limits:
- Start slow (50 requests)
- Double capacity daily (50→100→200→400→800)
- Monitor API performance
- Adjust schedule as needed

### Jitter/Randomization

Adds 30-60 seconds random delay before each request:
- Prevents thundering herd
- Makes traffic pattern organic
- Avoids hitting API at exact intervals
- Better for distributed systems

### Duplicate Prevention

MongoDB tracking ensures each file processed exactly once:
- Unique index on filename
- Atomic "mark as processing" operation
- Survives worker restarts
- Prevents race conditions

### Time Window Control

Only process files during specific hours:
- Respect API maintenance windows
- Avoid peak traffic hours
- Control costs (if API is metered)
- Predictable load patterns

---

## ✅ Production Readiness

| Aspect | Status | Notes |
|--------|--------|-------|
| Containerization | ✅ Complete | Dockerfile + docker-compose |
| Configuration | ✅ Complete | Environment variables |
| Monitoring | ✅ Complete | Flower dashboard + logs |
| Error Handling | ✅ Complete | Try-catch, retries, tracking |
| Logging | ✅ Complete | Structured logging |
| Data Persistence | ✅ Complete | MongoDB + file system |
| Scalability | ✅ Complete | Multiple workers supported |
| Documentation | ✅ Complete | README + guides |
| Testing | ✅ Complete | Test scripts included |
| Security | ⚠️ Update | Change default passwords |

---

## 🚀 Next Steps

1. **Deploy**: Run `./scripts/start.sh`
2. **Add files**: Copy 200 JSON files to `data/pending/`
3. **Monitor**: Open http://localhost:5555
4. **Adjust**: Tune warm-up schedule based on results
5. **Scale**: Add more workers if needed

---

## 📞 Support

- **Quick Help**: `./scripts/status.sh`
- **Logs**: `docker logs crawler-scheduler-worker`
- **Dashboard**: http://localhost:5555
- **Documentation**: See README.md

---

**System Status**: ✅ Ready for Production  
**Code Quality**: 736 lines of clean Python  
**Test Coverage**: Manual testing scripts included  
**Documentation**: Comprehensive guides included  

Happy scheduling! 🎉

