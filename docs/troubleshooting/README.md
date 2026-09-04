# Troubleshooting Guide

This directory contains troubleshooting documentation, fix guides, and problem-solving resources for common issues in the Search Engine Core project.

## 📋 Available Guides

### MongoDB Issues

- **[FIX_MONGODB_WARNING.md](./FIX_MONGODB_WARNING.md)** - Fix for "No MongoDB storage available - frontier will not be persistent" warning
  - Complete fix implementation and deployment guide
  - Root cause analysis
  - Testing and verification steps
  - Impact assessment

- **[MONGODB_WARNING_ANALYSIS.md](./MONGODB_WARNING_ANALYSIS.md)** - Technical analysis of MongoDB storage initialization
  - Detailed root cause investigation
  - Code flow analysis
  - Solution explanation
  - Related files and code references

### Search & Redis Issues

- **[SEARCH_RESULT_COUNT_MISMATCH.md](./SEARCH_RESULT_COUNT_MISMATCH.md)** - Fix for mismatch between reported total results and rendered items
  - Root cause analysis: non-deterministic key generation, Redis raw count vs deduplicated set
  - Deterministic SHA-256 key hashing in Python (`sync.py`) and C++ (`RedisSearchStorage.cpp`)
  - Verification with automated `--clear` re-sync and live search checks

## 🔍 Common Issues

### MongoDB Connection Issues

**Symptom:** Crawler logs warning about MongoDB storage not being available

**Solution:** See [FIX_MONGODB_WARNING.md](./FIX_MONGODB_WARNING.md)

**Root Cause:** Lazy initialization race condition in ContentStorage class

### Search Result Count Mismatch

**Symptom:** Total results counter says N items found, but fewer items are displayed on page

**Solution:** See [SEARCH_RESULT_COUNT_MISMATCH.md](./SEARCH_RESULT_COUNT_MISMATCH.md)

**Root Cause:** Non-deterministic key hashing in `sync.py` creating orphan duplicates in Redis and raw Redis count used prior to deduplication.

---

### Adding New Troubleshooting Guides

When documenting new issues or fixes:

1. **Create a detailed fix guide** with:
   - Clear problem description
   - Root cause analysis
   - Step-by-step solution
   - Testing and verification
   - Prevention strategies

2. **Include code examples** showing:
   - Before/after comparisons
   - Actual fix implementation
   - Related code locations

3. **Add references** to:
   - Related source files
   - API documentation
   - Architecture documents

4. **Update this README** with links to new guides

## 📚 Related Documentation

- **[../development/](../development/)** - Development guides and best practices
- **[../architecture/](../architecture/)** - System architecture documentation
- **[../guides/](../guides/)** - User and deployment guides
- **[../api/](../api/)** - API endpoint documentation

## 🆘 Getting Help

If you encounter an issue not covered here:

1. Check the [main README](../../README.md) for general information
2. Review [architecture documentation](../architecture/) for system design
3. Search existing GitHub issues
4. Create a new issue with detailed reproduction steps

---

**Last Updated:** October 2025
**Maintainer:** Search Engine Core Team
