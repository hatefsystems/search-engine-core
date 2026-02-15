# SEO Tests CMakeLists.txt

# Find required packages for tests
find_package(GTest REQUIRED)

# Make sure SEO library is available
if(NOT TARGET seo)
    message(FATAL_ERROR "SEO library target not found. Make sure src/seo is built first.")
endif()

# Individual test executables
add_executable(test_seo_generator test_seo_generator.cpp)
target_link_libraries(test_seo_generator PRIVATE
    GTest::gtest
    GTest::gtest_main
    seo
    storage
)

add_executable(test_schema_validator test_schema_validator.cpp)
target_link_libraries(test_schema_validator PRIVATE
    GTest::gtest
    GTest::gtest_main
    seo
)

add_executable(test_sitemap_generator test_sitemap_generator.cpp)
target_link_libraries(test_sitemap_generator PRIVATE
    GTest::gtest
    GTest::gtest_main
    seo
    storage
)

# Combined SEO test executable (for convenience)
add_executable(seo_tests 
    test_seo_generator.cpp
    test_schema_validator.cpp
    test_sitemap_generator.cpp
)
target_link_libraries(seo_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    seo
    storage
)
target_compile_definitions(seo_tests PRIVATE SEO_TESTS_COMBINED)

# Include test framework
include(CTest)
include(GoogleTest)

# Add individual tests to CTest
add_test(NAME seo_generator_test COMMAND test_seo_generator)
set_tests_properties(seo_generator_test PROPERTIES
    TIMEOUT 60
    LABELS "seo;generator"
)

add_test(NAME schema_validator_test COMMAND test_schema_validator)
set_tests_properties(schema_validator_test PROPERTIES
    TIMEOUT 60
    LABELS "seo;validator"
)

add_test(NAME sitemap_generator_test COMMAND test_sitemap_generator)
set_tests_properties(sitemap_generator_test PROPERTIES
    TIMEOUT 60
    LABELS "seo;sitemap"
)

# Add combined test to CTest
add_test(NAME seo_tests COMMAND seo_tests)
set_tests_properties(seo_tests PROPERTIES
    TIMEOUT 120
    LABELS "seo;integration;combined"
)

# Auto-discover individual test cases within each executable
gtest_discover_tests(test_seo_generator)
gtest_discover_tests(test_schema_validator)
gtest_discover_tests(test_sitemap_generator)
gtest_discover_tests(seo_tests)

message(STATUS "SEO tests configured with individual and combined executables + auto-discovery")
