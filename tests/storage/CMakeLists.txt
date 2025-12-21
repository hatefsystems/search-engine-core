# Storage Tests CMakeLists.txt

# Find required packages for tests
find_package(Catch2 3 REQUIRED)

# Make sure storage library is available
if(NOT TARGET storage)
    message(FATAL_ERROR "Storage library target not found. Make sure src/storage is built first.")
endif()

# Individual test executables
add_executable(test_mongodb_storage test_mongodb_storage.cpp)
target_link_libraries(test_mongodb_storage PRIVATE
    Catch2::Catch2WithMain
    storage
)

add_executable(test_profile_storage test_profile_storage.cpp)
target_link_libraries(test_profile_storage PRIVATE
    Catch2::Catch2WithMain
    storage
)

# Add Redis-dependent tests only if Redis was detected in parent build
if(REDIS_AVAILABLE)
    add_executable(test_redis_search_storage test_redis_search_storage.cpp)
    target_link_libraries(test_redis_search_storage PRIVATE
        Catch2::Catch2WithMain
        storage
    )
    target_compile_definitions(test_redis_search_storage PRIVATE REDIS_AVAILABLE)
    
    add_executable(test_content_storage test_content_storage.cpp)
    target_link_libraries(test_content_storage PRIVATE
        Catch2::Catch2WithMain
        storage
    )
    target_compile_definitions(test_content_storage PRIVATE REDIS_AVAILABLE)
    
    # Add debug Redis utility
    add_executable(debug_redis debug_redis.cpp)
    target_link_libraries(debug_redis PRIVATE
        storage
    )
    target_compile_definitions(debug_redis PRIVATE REDIS_AVAILABLE)
    
    message(STATUS "Including Redis-dependent storage tests")
else()
    message(STATUS "Excluding Redis-dependent storage tests (Redis not available)")
endif()

# Combined storage test executable (for convenience)
set(COMBINED_TEST_SOURCES test_mongodb_storage.cpp test_profile_storage.cpp)
if(REDIS_AVAILABLE)
    list(APPEND COMBINED_TEST_SOURCES
        test_redis_search_storage.cpp
        test_content_storage.cpp
    )
endif()

add_executable(storage_tests ${COMBINED_TEST_SOURCES})
target_link_libraries(storage_tests PRIVATE
    Catch2::Catch2WithMain
    storage
)

if(REDIS_AVAILABLE)
    target_compile_definitions(storage_tests PRIVATE REDIS_AVAILABLE)
endif()

# Include test framework
include(CTest)
include(Catch)

# Add individual tests to CTest
add_test(NAME mongodb_storage_test COMMAND test_mongodb_storage)
set_tests_properties(mongodb_storage_test PROPERTIES
    TIMEOUT 300
    LABELS "storage;mongodb"
)

add_test(NAME profile_storage_test COMMAND test_profile_storage)
set_tests_properties(profile_storage_test PROPERTIES
    TIMEOUT 300
    LABELS "storage;profile"
)

if(REDIS_AVAILABLE)
    add_test(NAME redis_search_storage_test COMMAND test_redis_search_storage)
    set_tests_properties(redis_search_storage_test PROPERTIES
        TIMEOUT 300
        LABELS "storage;redis"
    )
    
    add_test(NAME content_storage_test COMMAND test_content_storage)
    set_tests_properties(content_storage_test PROPERTIES
        TIMEOUT 300
        LABELS "storage;content"
    )
endif()

# Add combined test to CTest
add_test(NAME storage_tests COMMAND storage_tests)
set_tests_properties(storage_tests PROPERTIES
    TIMEOUT 600
    LABELS "storage;integration;combined"
)

# Auto-discover individual test cases within each executable
catch_discover_tests(test_mongodb_storage)
catch_discover_tests(test_profile_storage)
catch_discover_tests(storage_tests)

if(REDIS_AVAILABLE)
    catch_discover_tests(test_redis_search_storage)
    catch_discover_tests(test_content_storage)
endif()

message(STATUS "Storage tests configured with individual and combined executables + auto-discovery") 
