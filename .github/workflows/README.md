# 🚀 GitHub Actions Workflows - Smart Caching System

## Overview

This project uses an intelligent source-based caching system for Docker images that automatically detects when rebuilds are needed based on actual file changes.

## 🎯 Problem Solved

**Before**: The workflow would skip rebuilding Docker images if they already existed in the registry, even when source files changed. This caused developers to pull outdated images.

**After**: The workflow calculates a hash of all source files and compares it with the hash stored in the existing Docker image. Rebuilds only happen when source files actually change.

## 📋 Workflow Structure

```
ci-cd-pipeline.yml (Main Workflow)
    ↓
docker-build-orchestrator.yml (Orchestrates all builds)
    ↓
    ├── build-mongodb-drivers.yml
    ├── build-js-minifier.yml
    ├── build-crawler-scheduler.yml (✨ Smart caching implemented)
    └── build-search-engine.yml
```

## 🔍 How Smart Caching Works

### 1. **Calculate Source Hash**
```bash
# Hashes all Python files, requirements.txt, and Dockerfile
SOURCE_HASH=$(find ./crawler-scheduler -type f \
  \( -name "*.py" -o -name "*.txt" -o -name "Dockerfile" \) \
  -exec sha256sum {} \; | sort | sha256sum | cut -d' ' -f1)
```

### 2. **Compare with Existing Image**
```bash
# Pull existing image and check its source-hash label
EXISTING_HASH=$(docker inspect image:latest \
  --format='{{index .Config.Labels "source-hash"}}')

if [ "$EXISTING_HASH" = "$SOURCE_HASH" ]; then
  # Skip build - source unchanged
else
  # Rebuild - source changed
fi
```

### 3. **Build with Hash Label**
```yaml
labels: |
  source-hash=${{ steps.source-hash.outputs.hash }}
  build-date=${{ github.event.head_commit.timestamp }}
```

## 🎬 Usage Examples

### Automatic Builds (on push)

```bash
# Just commit and push - smart caching happens automatically
git add crawler-scheduler/app/file_processor.py
git commit -m "fix: Update file processor logic"
git push origin master
```

**Workflow behavior**:
- ✅ Calculates new hash: `abc123...`
- 🔍 Checks existing image hash: `xyz789...`
- 🔄 **Detects change → Rebuilds image**

### Manual Trigger with Force Rebuild

If you need to force a rebuild (bypass cache):

1. Go to **Actions** tab in GitHub
2. Select **🚀 CI/CD Pipeline** workflow
3. Click **Run workflow**
4. Check **"Force rebuild all images"**
5. Click **Run workflow**

### Manual Trigger (Normal - Smart Cache)

To manually trigger with smart caching:

1. Go to **Actions** tab in GitHub
2. Select **🚀 CI/CD Pipeline** workflow
3. Click **Run workflow**
4. Leave **"Force rebuild all images"** unchecked
5. Click **Run workflow**

## 📊 Workflow Logs - What to Expect

### When Source Files Changed

```
📦 Source hash: abc123def456...
🔄 Source files changed (old: xyz789old123, new: abc123def456)
rebuild_needed=true
🔨 Building Crawler Scheduler Service Image
✅ Image pushed to ghcr.io/...
```

### When Source Files Unchanged

```
📦 Source hash: abc123def456...
✅ Image is up-to-date (hash: abc123def456)
rebuild_needed=false
⏭️  Skipping build (no changes detected)
```

### When Force Rebuild Enabled

```
🔨 Force rebuild requested
rebuild_needed=true
🔨 Building Crawler Scheduler Service Image
✅ Image pushed to ghcr.io/...
```

## 🛠️ Testing the Smart Cache Locally

You can simulate the caching logic locally:

```bash
# Calculate hash of your crawler-scheduler changes
SOURCE_HASH=$(find ./crawler-scheduler -type f \
  \( -name "*.py" -o -name "*.txt" -o -name "Dockerfile" \) \
  -exec sha256sum {} \; | sort | sha256sum | cut -d' ' -f1)

echo "Local source hash: $SOURCE_HASH"

# Pull existing image and check its hash
docker pull ghcr.io/yourusername/search-engine-core/crawler-scheduler:latest
EXISTING_HASH=$(docker inspect ghcr.io/yourusername/search-engine-core/crawler-scheduler:latest \
  --format='{{index .Config.Labels "source-hash"}}')

echo "Existing image hash: $EXISTING_HASH"

# Compare
if [ "$EXISTING_HASH" = "$SOURCE_HASH" ]; then
  echo "✅ No rebuild needed - hashes match"
else
  echo "🔄 Rebuild needed - hashes differ"
fi
```

## 🐛 Troubleshooting

### Build Still Not Running?

**Possible causes**:

1. **Source hash hasn't changed**: Only files in `crawler-scheduler/` directory trigger rebuilds
2. **Cache from previous run**: Try force rebuild option
3. **Workflow permissions**: Check if GitHub Actions has write access to packages

**Solution**:
```bash
# Option 1: Force rebuild via GitHub UI (see above)

# Option 2: Change cache version
# In GitHub Actions → Run workflow → Set cache_version to "2"

# Option 3: Commit a dummy change
echo "# $(date)" >> crawler-scheduler/README.md
git commit -m "chore: Trigger rebuild"
git push
```

### How to Verify Smart Caching is Working

Check the workflow logs for these lines:

```bash
# Look for source hash calculation
grep "📦 Source hash" workflow.log

# Look for cache decision
grep -E "(✅ Image is up-to-date|🔄 Source files changed)" workflow.log

# Look for rebuild status
grep "rebuild_needed=" workflow.log
```

### Image Labels Not Found

If you see `EXISTING_HASH=""`, the image was built before smart caching was implemented:

```bash
# First build after implementing smart caching will always rebuild
# This is expected and normal behavior
```

## 📈 Benefits

| Feature | Before | After |
|---------|--------|-------|
| **Unnecessary rebuilds** | ❌ Always skipped if image exists | ✅ Only rebuild when source changes |
| **Detection accuracy** | ❌ Tag-based only | ✅ Content hash-based |
| **Developer experience** | ❌ Manual cache busting needed | ✅ Automatic detection |
| **Build time** | ~5-10 minutes (always builds) | ~30 seconds (cached) / 5-10 min (changed) |
| **CI/CD speed** | Slow | Fast when no changes |

## 🔧 Configuration

### Files Included in Hash

Currently hashing:
- `**/*.py` - All Python source files
- `**/*.txt` - Requirements and config files
- `**/Dockerfile` - Docker build instructions

To add more file types, edit `.github/workflows/build-crawler-scheduler.yml`:

```yaml
SOURCE_HASH=$(find ./crawler-scheduler -type f \
  \( -name "*.py" -o -name "*.txt" -o -name "*.json" -o -name "*.yaml" -o -name "Dockerfile" \) \
  -exec sha256sum {} \; | sort | sha256sum | cut -d' ' -f1)
```

### Disable Smart Caching

If you want to always rebuild (not recommended):

```yaml
# In build-crawler-scheduler.yml
- name: Build Crawler Scheduler Service Image
  if: true  # Always run
```

## 📝 Workflow Parameters

### ci-cd-pipeline.yml

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `cache_version` | string | "1" | Docker buildx cache version (change to bust cache) |
| `force_rebuild` | boolean | false | Force rebuild all images (ignore hash comparison) |

### docker-build-orchestrator.yml

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `cache_version` | string | "1" | Passed to all build workflows |
| `force_rebuild` | boolean | false | Passed to all build workflows |

### build-crawler-scheduler.yml

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `docker_image` | string | required | Full image name (e.g., ghcr.io/user/repo) |
| `docker_tag` | string | required | Image tag (e.g., latest, v1.0.0) |
| `cache_version` | string | "1" | Buildx cache version |
| `force_rebuild` | boolean | false | Skip hash comparison, always rebuild |

## 🚀 Best Practices

1. **Let smart caching do its job**: Don't force rebuild unless necessary
2. **Commit related changes together**: Hash includes all files, so atomic commits work best
3. **Use semantic versioning for tags**: Consider using git commit SHA as docker tag for production
4. **Monitor workflow logs**: Check if caching is working as expected
5. **Test locally first**: Verify changes work before pushing to master

## 📚 Related Documentation

- [Docker Build Push Action](https://github.com/docker/build-push-action)
- [GitHub Actions Cache](https://docs.github.com/en/actions/using-workflows/caching-dependencies-to-speed-up-workflows)
- [Docker Labels](https://docs.docker.com/config/labels-custom-metadata/)

## 🎉 Summary

Your workflow now intelligently detects when rebuilds are needed based on actual source file changes, saving CI/CD time and ensuring fresh images when code changes. Just commit your changes and let the smart caching system handle the rest! 🚀

