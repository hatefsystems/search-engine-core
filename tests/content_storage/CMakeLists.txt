cmake_minimum_required(VERSION 3.15)

# Find Catch2 package
find_package(Catch2 3 REQUIRED)
find_package(redis++ CONFIG REQUIRED)

# Add test executable
add_executable(content_storage_tests
    mongodb_tests.cpp
    redisearch_tests.cpp
)

# Include directories
target_include_directories(content_storage_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/crawler
    ${MONGOCXX_DRIVER_DIR}/include/mongocxx/v_noabi
    ${MONGOCXX_DRIVER_DIR}/include/bsoncxx/v_noabi
    ${MONGOC_DRIVER_DIR}/include/libbson-1.0
    ${MONGOC_DRIVER_DIR}/include/libmongoc-1.0
)

# Link libraries
target_link_libraries(content_storage_tests PRIVATE
    Catch2::Catch2WithMain
    bson-1.0
    mongoc-1.0
    bsoncxx
    mongocxx
    redis++::redis++
    nlohmann_json::nlohmann_json
)

# Add source files that need to be tested
target_sources(content_storage_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/content_storage.cpp
)

# Register with CTest
add_test(NAME content_storage_tests COMMAND content_storage_tests)