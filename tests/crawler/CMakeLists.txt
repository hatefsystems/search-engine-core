# Add test executable
add_executable(crawler_tests
    crawler_tests.cpp
    content_parser_tests.cpp
    robots_txt_parser_tests.cpp
    url_frontier_tests.cpp
    page_fetcher_tests.cpp
    template_validator_tests.cpp
    template_registry_tests.cpp
    template_storage_tests.cpp
    template_applier_tests.cpp
    template_integration_tests.cpp
    template_performance_tests.cpp
    ../../src/storage/ContentStorage.cpp
    ../../src/storage/MongoDBStorage.cpp
)

# Add include directories from the crawler library
# and storage library
# (storage already includes its own headers)
target_include_directories(crawler_tests PRIVATE $<TARGET_PROPERTY:crawler,INTERFACE_INCLUDE_DIRECTORIES>)

target_link_libraries(crawler_tests
    PRIVATE
        crawler
        storage
        Catch2::Catch2WithMain
)

# Include Catch2 CMake module
include(CTest)
include(Catch)

# Add test
catch_discover_tests(crawler_tests) 