#!/bin/bash
# Verify crawler scheduler setup is complete and correct

echo "=================================="
echo "Crawler Scheduler Setup Verification"
echo "=================================="
echo ""

ERRORS=0
WARNINGS=0

# Check 1: Project structure
echo "✓ Checking project structure..."
REQUIRED_DIRS=(
    "app"
    "data/pending"
    "data/processed"
    "data/failed"
    "scripts"
)

for dir in "${REQUIRED_DIRS[@]}"; do
    if [ -d "$dir" ]; then
        echo "  ✓ $dir"
    else
        echo "  ✗ $dir (MISSING)"
        ERRORS=$((ERRORS + 1))
    fi
done
echo ""

# Check 2: Python files
echo "✓ Checking Python application files..."
REQUIRED_FILES=(
    "app/__init__.py"
    "app/config.py"
    "app/celery_app.py"
    "app/database.py"
    "app/rate_limiter.py"
    "app/file_processor.py"
    "app/tasks.py"
)

for file in "${REQUIRED_FILES[@]}"; do
    if [ -f "$file" ]; then
        echo "  ✓ $file"
    else
        echo "  ✗ $file (MISSING)"
        ERRORS=$((ERRORS + 1))
    fi
done
echo ""

# Check 3: Docker files
echo "✓ Checking Docker configuration..."
DOCKER_FILES=(
    "Dockerfile"
    "docker-compose.yml"
    "requirements.txt"
)

for file in "${DOCKER_FILES[@]}"; do
    if [ -f "$file" ]; then
        echo "  ✓ $file"
    else
        echo "  ✗ $file (MISSING)"
        ERRORS=$((ERRORS + 1))
    fi
done
echo ""

# Check 4: Documentation
echo "✓ Checking documentation..."
DOC_FILES=(
    "README.md"
    "QUICKSTART.md"
    "INTEGRATION.md"
    "PROJECT_OVERVIEW.md"
)

for file in "${DOC_FILES[@]}"; do
    if [ -f "$file" ]; then
        echo "  ✓ $file"
    else
        echo "  ✗ $file (MISSING)"
        WARNINGS=$((WARNINGS + 1))
    fi
done
echo ""

# Check 5: Scripts
echo "✓ Checking helper scripts..."
SCRIPT_FILES=(
    "scripts/start.sh"
    "scripts/stop.sh"
    "scripts/status.sh"
    "scripts/test_api.sh"
)

for file in "${SCRIPT_FILES[@]}"; do
    if [ -f "$file" ] && [ -x "$file" ]; then
        echo "  ✓ $file (executable)"
    elif [ -f "$file" ]; then
        echo "  ⚠ $file (not executable)"
        WARNINGS=$((WARNINGS + 1))
    else
        echo "  ✗ $file (MISSING)"
        ERRORS=$((ERRORS + 1))
    fi
done
echo ""

# Check 6: Docker availability
echo "✓ Checking Docker availability..."
if command -v docker &> /dev/null; then
    echo "  ✓ Docker installed"
    if docker info &> /dev/null; then
        echo "  ✓ Docker running"
    else
        echo "  ⚠ Docker not running (start Docker to continue)"
        WARNINGS=$((WARNINGS + 1))
    fi
else
    echo "  ✗ Docker not installed"
    ERRORS=$((ERRORS + 1))
fi
echo ""

# Check 7: Network
echo "✓ Checking Docker network..."
if docker network inspect search-engine-network &> /dev/null 2>&1; then
    echo "  ✓ search-engine-network exists"
else
    echo "  ⚠ search-engine-network not found (will be created on first start)"
    WARNINGS=$((WARNINGS + 1))
fi
echo ""

# Check 8: Example file
echo "✓ Checking example data..."
if [ -f "data/pending/example_domain.json" ]; then
    echo "  ✓ Example domain file exists"
else
    echo "  ⚠ Example domain file missing (not critical)"
    WARNINGS=$((WARNINGS + 1))
fi
echo ""

# Summary
echo "=================================="
echo "Verification Summary"
echo "=================================="
echo "Errors: $ERRORS"
echo "Warnings: $WARNINGS"
echo ""

if [ $ERRORS -eq 0 ]; then
    echo "✓ Setup is complete and ready!"
    echo ""
    echo "Next steps:"
    echo "  1. Run: ./scripts/start.sh"
    echo "  2. Add JSON files to data/pending/"
    echo "  3. Open Flower: http://localhost:5555"
    echo ""
    exit 0
else
    echo "✗ Setup has $ERRORS critical errors"
    echo ""
    echo "Please fix the errors above and run verification again."
    echo ""
    exit 1
fi

