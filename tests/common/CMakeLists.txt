# Common Tests CMakeLists.txt

# Find Catch2 package
find_package(Catch2 3 REQUIRED)

# Test executable for JS Minifier file processing
add_executable(test_js_minifier_file TestJsMinifierFile.cpp)

# Link libraries
target_link_libraries(test_js_minifier_file
    PRIVATE
        Catch2::Catch2WithMain
        common
        ${CURL_LIBRARIES}
        CURL::libcurl
)

# Include directories
target_include_directories(test_js_minifier_file PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

# Test executable for SlugGenerator
add_executable(test_slug_generator test_slug_generator.cpp)

# Link libraries
target_link_libraries(test_slug_generator
    PRIVATE
        Catch2::Catch2WithMain
        common
)

# Include directories
target_include_directories(test_slug_generator PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

# Test executable for SlugCache
add_executable(test_slug_cache test_slug_cache.cpp)

# Link libraries
target_link_libraries(test_slug_cache
    PRIVATE
        Catch2::Catch2WithMain
        common
)

# Include directories
target_include_directories(test_slug_cache PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

# Register tests with CTest
include(CTest)
include(Catch)

# Add test to CTest
add_test(NAME js_minifier_file_test COMMAND test_js_minifier_file)
set_tests_properties(js_minifier_file_test PROPERTIES
    TIMEOUT 60
    LABELS "common;js_minifier;file_processing"
)

# Add SlugGenerator test to CTest
add_test(NAME slug_generator_test COMMAND test_slug_generator)
set_tests_properties(slug_generator_test PROPERTIES
    TIMEOUT 30
    LABELS "common;slug;unicode"
)

# Add SlugCache test to CTest
add_test(NAME slug_cache_test COMMAND test_slug_cache)
set_tests_properties(slug_cache_test PROPERTIES
    TIMEOUT 30
    LABELS "common;slug;cache"
)

# Auto-discover test cases
catch_discover_tests(test_js_minifier_file)
catch_discover_tests(test_slug_generator)
catch_discover_tests(test_slug_cache)

message(STATUS "Common tests configured with JS Minifier file processing test")
