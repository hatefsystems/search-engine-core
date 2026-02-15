# Integration Tests

This folder contains integration test scripts for the Search Engine Core API.

## Test Scripts

### Profile API Tests

**`test_profile_api.sh`** - Comprehensive profile CRUD operations test
- Creates, reads, updates, and deletes profiles
- Tests authentication with owner tokens
- Validates privacy controls and encryption
- Tests rate limiting and error handling

**`test_website_profile_api.sh`** - Website profile specific tests
- Tests business profile creation and validation
- Validates website-specific fields (company info, services, location)

**`test_link_blocks.sh`** - Link blocks and analytics system test
- Tests link CRUD operations
- Validates secure redirects (302 Found)
- Tests privacy-first click analytics
- Validates privacy controls (PUBLIC, HIDDEN, DISABLED)
- Tests rate limiting on redirects
- Validates data retention cleanup

### Performance Tests

**`test_10_concurrent.sh`** - Concurrent request handling test
- Tests server stability under concurrent load
- Validates proper handling of multiple simultaneous requests

## Running Tests

### Prerequisites

```bash
# Ensure server is running
docker-compose up -d

# Or run directly
cd build && ./server
```

### Run Individual Tests

```bash
# Profile API test
./tests/integration/test_profile_api.sh

# Link blocks test
./tests/integration/test_link_blocks.sh

# Website profile test
./tests/integration/test_website_profile_api.sh

# Concurrent test
./tests/integration/test_10_concurrent.sh
```

### Run All Integration Tests

```bash
# From project root
for test in tests/integration/test_*.sh; do
    echo "Running $test..."
    bash "$test"
    echo "---"
done
```

## Test Requirements

All test scripts require:
- `curl` - HTTP client for API requests
- `jq` - JSON processor for response parsing
- Server running on `localhost:3000` (default)
- MongoDB connection available
- Redis connection available (for rate limiting)

## Environment Variables

Tests respect the following environment variables:

```bash
# Server endpoint (default: http://localhost:3000)
export API_BASE_URL="http://localhost:3000"

# Enable verbose output
export TEST_VERBOSE=1

# MongoDB URI (if testing with custom database)
export MONGODB_URI="mongodb://localhost:27017"
```

## Test Output

All tests follow a consistent format:
- ✅ Green checkmarks for passed tests
- ❌ Red X marks for failed tests
- Detailed error messages on failure
- Summary statistics at the end

## Adding New Tests

When adding new integration tests:

1. **Naming:** Use `test_<feature_name>.sh` format
2. **Shebang:** Start with `#!/bin/bash`
3. **Error Handling:** Use `set -e` to exit on errors
4. **Cleanup:** Always clean up created test data
5. **Documentation:** Add description to this README

### Test Template

```bash
#!/bin/bash
set -e

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

API_URL="${API_BASE_URL:-http://localhost:3000}"

echo "Testing Feature X..."

# Test setup
test_data=$(cat <<EOF
{
    "field": "value"
}
EOF
)

# Run test
response=$(curl -s -X POST "$API_URL/api/endpoint" \
    -H "Content-Type: application/json" \
    -d "$test_data")

# Validate response
if echo "$response" | jq -e '.success == true' > /dev/null; then
    echo -e "${GREEN}✅ Test passed${NC}"
else
    echo -e "${RED}❌ Test failed${NC}"
    echo "$response" | jq .
    exit 1
fi

# Cleanup
# ... cleanup code ...

echo "All tests passed!"
```

## CI/CD Integration

These tests are designed to be run in CI/CD pipelines:

```yaml
# Example GitHub Actions workflow
- name: Run Integration Tests
  run: |
    docker-compose up -d
    sleep 5  # Wait for services to be ready
    for test in tests/integration/test_*.sh; do
      bash "$test" || exit 1
    done
    docker-compose down
```

## Debugging Failed Tests

If tests fail:

1. **Check server logs:**
   ```bash
   docker logs core
   # or
   tail -f /tmp/server.log
   ```

2. **Verify services are running:**
   ```bash
   docker-compose ps
   ```

3. **Test API manually:**
   ```bash
   curl -v http://localhost:3000/api/health
   ```

4. **Check MongoDB connection:**
   ```bash
   docker exec -it mongodb mongosh
   ```

5. **Enable verbose test output:**
   ```bash
   TEST_VERBOSE=1 ./tests/integration/test_profile_api.sh
   ```

## Performance Benchmarks

Expected performance for integration tests:
- Profile CRUD operations: < 100ms per request
- Link redirects: < 50ms
- Analytics queries: < 200ms
- Full test suite: < 60 seconds

## Security Notes

- Tests use randomly generated data to avoid conflicts
- All test data is cleaned up after execution
- Owner tokens are never committed to version control
- Rate limiting tests use controlled delays to avoid triggering limits

## Related Documentation

- **API Documentation:** `docs/api/`
- **Feature Guides:** `docs/features/`
- **Quick Start:** `docs/api/LINK_BLOCKS_QUICK_START.md`
- **Architecture:** `docs/architecture/`
