# Timezone Detection Behavior

## ✅ How It Works (Ubuntu 24)

The crawler scheduler **automatically detects your Ubuntu 24 system timezone** and uses it by default. You can **override** this by setting configuration variables.

### 🎯 Priority Order

```
1. SCHEDULER_TIMEZONE env var → If set, OVERRIDES system timezone
2. TZ env var              → If set, OVERRIDES system timezone
3. /etc/timezone           → Ubuntu 24 system timezone (DEFAULT) ✅
4. /etc/localtime          → Fallback system timezone detection
5. UTC                     → Last resort fallback
```

**In simple terms:**
- **By default**: Uses your Ubuntu 24 system timezone
- **With config**: Override by setting `SCHEDULER_TIMEZONE` environment variable

---

## 📋 Your Current System (Ubuntu 24)

```bash
# Check your system timezone
$ cat /etc/timezone
Asia/Tehran

# Verify with symlink
$ ls -la /etc/localtime
lrwxrwxrwx 1 root root 31 Oct 17 16:00 /etc/localtime -> /usr/share/zoneinfo/Asia/Tehran
```

**Result**: Your scheduler automatically uses **Asia/Tehran** timezone! ✅

---

## 🔧 Usage Examples

### Example 1: Use System Timezone (Default - Ubuntu 24)

**No configuration needed!** Just start the scheduler:

```bash
docker-compose up -d
```

**What happens:**
```
[Config] Timezone: Asia/Tehran (auto-detected from system /etc/timezone file)
```

**Result**: Scheduler uses **Asia/Tehran** from your Ubuntu 24 system ✅

---

### Example 2: Override with Custom Timezone

If you want to use a **different timezone** (not your system default):

```yaml
# docker-compose.yml
services:
  crawler-scheduler:
    environment:
      - SCHEDULER_TIMEZONE=America/New_York  # Override system timezone
```

**What happens:**
```
[Config] Timezone: America/New_York (from SCHEDULER_TIMEZONE environment variable)
```

**Result**: Scheduler uses **America/New_York** (ignores system Asia/Tehran) ✅

---

### Example 3: Override with TZ Variable

Alternative way to override system timezone:

```yaml
# docker-compose.yml
services:
  crawler-scheduler:
    environment:
      - TZ=Europe/London  # Override system timezone
```

**What happens:**
```
[Config] Timezone: Europe/London (from TZ environment variable)
```

**Result**: Scheduler uses **Europe/London** ✅

---

### Example 4: Priority Test (Both Set)

If you set **both** SCHEDULER_TIMEZONE and TZ:

```yaml
environment:
  - SCHEDULER_TIMEZONE=Asia/Tokyo
  - TZ=Europe/Paris
```

**What happens:**
```
[Config] Timezone: Asia/Tokyo (from SCHEDULER_TIMEZONE environment variable)
```

**Result**: `SCHEDULER_TIMEZONE` wins (higher priority) ✅

---

## 🧪 Testing

### Test 1: Verify System Detection

```bash
cd crawler-scheduler
python3 -c "from app.config import Config; print(f'Detected: {Config.TIMEZONE}')"
```

**Expected output:**
```
[Config] Timezone: Asia/Tehran (auto-detected from system /etc/timezone file)
Detected: Asia/Tehran
```

---

### Test 2: Verify Override Works

```bash
cd crawler-scheduler
SCHEDULER_TIMEZONE=America/New_York python3 -c "from app.config import Config; print(f'Detected: {Config.TIMEZONE}')"
```

**Expected output:**
```
[Config] Timezone: America/New_York (from SCHEDULER_TIMEZONE environment variable)
Detected: America/New_York
```

---

## 📊 Real-World Scenarios

### Scenario A: Development on Ubuntu 24 (Your Case)

**Setup:**
- Ubuntu 24 system timezone: `Asia/Tehran`
- No SCHEDULER_TIMEZONE or TZ set

**Result:**
```
✅ Auto-detects Asia/Tehran from /etc/timezone
✅ All time windows respect Asia/Tehran time
✅ WARMUP_START_HOUR=10 → 10:00 AM Tehran time
```

---

### Scenario B: Deploy to Different Region

**Setup:**
- Ubuntu 24 system timezone: `Asia/Tehran`
- Want to process during US hours: Set `SCHEDULER_TIMEZONE=America/New_York`

**Result:**
```
✅ Overrides system timezone with America/New_York
✅ All time windows respect New York time
✅ WARMUP_START_HOUR=10 → 10:00 AM New York time
```

---

### Scenario C: Multiple Instances

**Setup:**
- Deploy multiple schedulers in different regions
- Each uses local system timezone

**Instance 1 (Tehran server):**
```bash
# No override → uses system timezone
WARMUP_START_HOUR=10  # 10:00 AM Tehran time
```

**Instance 2 (New York server):**
```bash
# System timezone: America/New_York
WARMUP_START_HOUR=10  # 10:00 AM New York time
```

**Result:** Each instance processes at local business hours ✅

---

## 🔍 Verification in Logs

### Startup Log

When the scheduler starts, you'll see:

```log
[Config] Timezone: Asia/Tehran (auto-detected from system /etc/timezone file)
Scheduler timezone configured: Asia/Tehran
```

### Time Window Check Logs

```log
[INFO] Rate limiter check: Can process. Progress: 5/50, Remaining: 45 (Day 1)
```

Or when outside window:

```log
[WARNING] Cannot process files: Outside processing window. 
Current: 08:30 (Asia/Tehran), Allowed: 10:00-12:59
```

**Note:** Now includes timezone in the message! ✅

---

## 🎨 Visual Flow

```
┌─────────────────────────────────────────────────────────────┐
│                   Timezone Detection Flow                    │
└─────────────────────────────────────────────────────────────┘

START
  │
  ├─ Check SCHEDULER_TIMEZONE env var
  │  └─ Set? → Use it (OVERRIDE) ✓
  │
  ├─ Check TZ env var
  │  └─ Set? → Use it (OVERRIDE) ✓
  │
  ├─ Check /etc/timezone
  │  └─ Exists? → Use it (UBUNTU 24 DEFAULT) ✓
  │
  ├─ Check /etc/localtime symlink
  │  └─ Exists? → Use it (FALLBACK) ✓
  │
  └─ Use UTC (LAST RESORT)

RESULT: Timezone configured ✓
```

---

## 🚀 Quick Reference

| Scenario | Configuration | Result |
|----------|---------------|--------|
| **Default** | No config | Uses Ubuntu 24 system timezone (`Asia/Tehran`) ✅ |
| **Override** | `SCHEDULER_TIMEZONE=America/New_York` | Uses `America/New_York` ✅ |
| **Alt Override** | `TZ=Europe/London` | Uses `Europe/London` ✅ |
| **Both Set** | Both `SCHEDULER_TIMEZONE` and `TZ` | `SCHEDULER_TIMEZONE` wins ✅ |

---

## ⚠️ Important Notes

### 1. Time Windows Use Configured Timezone

```yaml
SCHEDULER_TIMEZONE=America/New_York
WARMUP_START_HOUR=10
WARMUP_END_HOUR=17
```

**Means:** Process from 10:00 AM to 5:59 PM **New York time**

### 2. Daily Counts Use Configured Timezone

The "day" starts at midnight in the **configured timezone**:
- If timezone is `Asia/Tehran` → Day starts at 00:00 Tehran time
- If timezone is `America/New_York` → Day starts at 00:00 New York time

### 3. Database Timestamps

All timestamps stored in MongoDB now use the **configured timezone**:
- `started_at` → Timezone-aware
- `processed_at` → Timezone-aware
- `failed_at` → Timezone-aware

---

## 📝 Summary

**Your Ubuntu 24 Setup:**
```
✅ System timezone: Asia/Tehran (detected from /etc/timezone)
✅ Scheduler automatically uses Asia/Tehran
✅ Can override with SCHEDULER_TIMEZONE or TZ if needed
✅ All logs show timezone for clarity
✅ Time windows respect configured timezone
```

**To Override:**
```yaml
# In docker-compose.yml
environment:
  - SCHEDULER_TIMEZONE=Your/Timezone  # Optional override
```

**No override needed? Perfect!** The scheduler automatically uses your Ubuntu 24 system timezone (Asia/Tehran) ✅

---

## 🔗 Related Documentation

- **Configuration Guide**: `INTEGRATED_USAGE.md`
- **Timezone Details**: `TIMEZONE_CONFIGURATION.md`
- **Main Documentation**: `README.md`

---

**System Behavior**: ✅ Auto-detects Ubuntu 24 timezone, configurable override available

**Your Current Setup**: ✅ Using Asia/Tehran from Ubuntu 24 system

