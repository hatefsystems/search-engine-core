#!/bin/bash

echo "Building and running tests..."

# Create build directory if it doesn't exist
if [ ! -d "build" ]; then
    mkdir build
fi

# Navigate to build directory
cd build

# Configure CMake if needed
if [ ! -f "CMakeCache.txt" ]; then
    echo "Configuring CMake..."
    cmake .. \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_STANDARD=20 \
        -DCMAKE_CXX_STANDARD_REQUIRED=ON \
        -DCMAKE_CXX_EXTENSIONS=OFF \
        -DPkgConfig_DIR=/usr/share/pkgconfig \
        -DBUILD_TESTS=ON
fi

# Build the tests
echo "Building tests..."
cmake --build . --config Debug 
#--target crawler_tests
#cmake --build . --config Debug --target storage_tests

# Check for timeout parameter
TEST_TIMEOUT=${TEST_TIMEOUT:-5}  # Default 5 seconds, can be overridden

# Run tests based on parameter
if [ -z "$1" ]; then
    echo "Running all tests..."
    ctest --timeout $TEST_TIMEOUT
else
    export LOG_LEVEL=TRACE
    echo "Running test: $1 (timeout: ${TEST_TIMEOUT}s)"
    
    # Check if this is a storage test (contains keywords related to storage tests)
    # Be more specific about Redis tests - only match actual storage tests, not search_core tests
    if [[ "$1" == *"Storage"* ]] || [[ "$1" == *"MongoDB"* ]] || [[ "$1" == *"RedisSearch Storage"* ]] || [[ "$1" == *"Content"* ]] || [[ "$1" == *"Index"* ]] || [[ "$1" == *"Document"* ]] || [[ "$1" == *"CRUD"* ]] || [[ "$1" == *"Profile"* ]]; then
        echo "Detected storage test, running storage_tests with filter..."
        cd tests/storage
        # Use Catch2 syntax for filtering tests (supports wildcards with *)
        ./storage_tests "*$1*"
        cd ../..
    else
        # Regular CTest for crawler tests
        ctest -V -R "$1" --timeout $TEST_TIMEOUT
    fi
fi

# Return to original directory
cd ..

echo "Done!"
echo ""
echo "Environment variables:"
echo "  TEST_TIMEOUT=<seconds>  Set test timeout (default: 5)"
echo ""
echo "Examples:"
echo "  # Run all tests"
echo "  ./build_and_test.sh"
echo ""
echo "  # Crawler tests (individual test names)"
echo "  ./build_and_test.sh \"URLFrontier handles visited URLs\""
echo "  ./build_and_test.sh \"PageFetcher handles basic page fetching\""
echo ""
echo "  # Storage tests (by category)"
echo "  ./build_and_test.sh \"MongoDB Storage\""
echo "  ./build_and_test.sh \"RedisSearch Storage\""
echo "  ./build_and_test.sh \"Content Storage\""
echo ""
echo "  # Storage tests (individual sections)"
echo "  ./build_and_test.sh \"Document Indexing and Retrieval\""
echo "  ./build_and_test.sh \"CRUD Operations\""
echo "  ./build_and_test.sh \"indexed page\""
echo ""
echo "  # With custom timeout"
echo "  TEST_TIMEOUT=60 ./build_and_test.sh \"MongoDB Storage\"" 