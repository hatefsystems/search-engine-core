cmake_minimum_required(VERSION 3.10)
project(search_engine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Disable uWebSockets header
add_definitions(-DUWS_HTTPRESPONSE_NO_WRITEMARK)

# Add MongoDB specific paths
include_directories(/usr/local/include/mongocxx/v_noabi /usr/local/include/bsoncxx/v_noabi)
list(APPEND CMAKE_PREFIX_PATH "/usr/local/lib/cmake/mongocxx-4.0.0")
list(APPEND CMAKE_PREFIX_PATH "/usr/local/lib/cmake/bsoncxx-4.0.0")

# Find required packages
find_package(mongocxx REQUIRED CONFIG)
find_package(bsoncxx REQUIRED CONFIG)
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GUMBO REQUIRED gumbo)
find_package(Threads REQUIRED)

# Find WebSocket++ and nlohmann/json
find_path(WEBSOCKETPP_INCLUDE_DIR NAMES websocketpp/endpoint.hpp PATHS /usr/include /usr/local/include)
find_path(NLOHMANN_JSON_INCLUDE_DIR NAMES nlohmann/json.hpp PATHS /usr/include /usr/local/include)

# Add Inja template engine (header-only)
find_path(INJA_INCLUDE_DIR NAMES inja/inja.hpp PATHS ${CMAKE_SOURCE_DIR}/include /usr/include /usr/local/include)
if(INJA_INCLUDE_DIR)
    set(INJA_AVAILABLE TRUE)
    message(STATUS "Inja found at: ${INJA_INCLUDE_DIR}")
else()
    message(FATAL_ERROR "Inja header file not found. Please run: mkdir -p include/inja && curl -L https://github.com/pantor/inja/releases/download/v3.4.0/inja.hpp -o include/inja/inja.hpp")
endif()

# Remove WebSocket++ and nlohmann/json checks for browserless
# Always build BrowserlessClient if referenced

# Redis and redis-plus-plus detection
find_path(HIREDIS_INCLUDE_DIR NAMES hiredis/hiredis.h PATHS /usr/include /usr/local/include)
find_library(HIREDIS_LIBRARY NAMES hiredis PATHS /usr/lib /usr/local/lib)
find_path(REDIS_PLUS_PLUS_INCLUDE_DIR NAMES sw/redis++/redis++.h PATHS /usr/include /usr/local/include)
find_library(REDIS_PLUS_PLUS_LIBRARY NAMES redis++ PATHS /usr/lib /usr/local/lib)

if(HIREDIS_INCLUDE_DIR AND HIREDIS_LIBRARY AND REDIS_PLUS_PLUS_INCLUDE_DIR AND REDIS_PLUS_PLUS_LIBRARY)
    set(REDIS_AVAILABLE TRUE)
    set(REDIS_LIBRARIES ${HIREDIS_LIBRARY} ${REDIS_PLUS_PLUS_LIBRARY})
    set(REDIS_INCLUDE_DIRS ${HIREDIS_INCLUDE_DIR} ${REDIS_PLUS_PLUS_INCLUDE_DIR})
    message(STATUS "Redis and redis-plus-plus found - RedisSearchStorage will be available")
    message(STATUS "  hiredis: ${HIREDIS_LIBRARY}")
    message(STATUS "  redis++: ${REDIS_PLUS_PLUS_LIBRARY}")
else()
    set(REDIS_AVAILABLE FALSE)
    message(STATUS "Redis dependencies not found - RedisSearchStorage will be excluded")
    if(NOT HIREDIS_INCLUDE_DIR OR NOT HIREDIS_LIBRARY)
        message(STATUS "  Missing hiredis")
    endif()
    if(NOT REDIS_PLUS_PLUS_INCLUDE_DIR OR NOT REDIS_PLUS_PLUS_LIBRARY)
        message(STATUS "  Missing redis-plus-plus")
    endif()
endif()

# Add include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/public
    /usr/local/include/uwebsockets
    /usr/local/include/usockets
    ${GUMBO_INCLUDE_DIRS}
)

# Add WebSocket include directories if available
# Always include BrowserlessClient if referenced
include_directories(${WEBSOCKETPP_INCLUDE_DIR} ${NLOHMANN_JSON_INCLUDE_DIR})

# Add Inja include directories if available
if(INJA_AVAILABLE AND INJA_INCLUDE_DIR)
    include_directories(${INJA_INCLUDE_DIR})
endif()

# Add Redis include directories if available
if(REDIS_AVAILABLE)
    include_directories(${REDIS_INCLUDE_DIRS})
endif()

# Create a common library for shared code
add_library(common STATIC
    src/common/Logger.cpp
    src/common/JsMinifierClient.cpp
    src/common/UrlSanitizer.cpp
)
target_include_directories(common 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(common
    PUBLIC
        ${CURL_LIBRARIES}
        CURL::libcurl
)

# Install common library
install(TARGETS common
    EXPORT CommonTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Add subdirectories for libraries
add_subdirectory(src/crawler)
add_subdirectory(src/storage)
add_subdirectory(src/scoring)
add_subdirectory(src/search_core)
add_subdirectory(src/models)
add_subdirectory(src/database)

# Add source files recursively to include subdirectories like src/common/
# file(GLOB_RECURSE SOURCES "src/*.cpp")

# # Remove test sources from SOURCES if they were picked up by GLOB_RECURSE
# list(FILTER SOURCES EXCLUDE REGEX ".*test.*\\.cpp$")

# file(GLOB_RECURSE HEADERS "include/*.h" "include/*.hpp")

file(GLOB_RECURSE MAIN_SOURCES "src/*.cpp" "src/main.cpp")

# Remove crawler sources, storage sources, test sources, common sources, models, and database sources from MAIN_SOURCES
list(FILTER MAIN_SOURCES EXCLUDE REGEX ".*crawler/.*\\.cpp$")
list(FILTER MAIN_SOURCES EXCLUDE REGEX ".*storage/.*\\.cpp$")
list(FILTER MAIN_SOURCES EXCLUDE REGEX ".*test.*\\.cpp$")
list(FILTER MAIN_SOURCES EXCLUDE REGEX ".*common/.*\\.cpp$")
list(FILTER MAIN_SOURCES EXCLUDE REGEX ".*scoring/.*\\.cpp$")
list(FILTER MAIN_SOURCES EXCLUDE REGEX ".*search_core/.*\\.cpp$")
list(FILTER MAIN_SOURCES EXCLUDE REGEX ".*kafka/.*\\.cpp$")
list(FILTER MAIN_SOURCES EXCLUDE REGEX ".*models/.*\\.cpp$")
list(FILTER MAIN_SOURCES EXCLUDE REGEX ".*database/.*\\.cpp$")

file(GLOB_RECURSE HEADERS "include/*.h" "include/*.hpp")

# Create MongoDB instance library for shared use
add_library(mongodb_instance STATIC src/mongodb.cpp)
target_include_directories(mongodb_instance 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(mongodb_instance 
    PUBLIC 
        mongo::bsoncxx_shared 
        mongo::mongocxx_shared
)
target_compile_definitions(mongodb_instance PRIVATE
    BSONCXX_STATIC
    MONGOCXX_STATIC
)

# Filter out mongodb.cpp from MAIN_SOURCES since it's now in a separate library  
list(FILTER MAIN_SOURCES EXCLUDE REGEX ".*mongodb\\.cpp$")

# Create executable
add_executable(server ${MAIN_SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(server
    PRIVATE
        common
        storage
        SponsorStorage
        crawler
        search_core
        scoring
        job_models
        job_database
        mongodb_instance
        /usr/local/lib/libmongocxx.so
        /usr/local/lib/libbsoncxx.so
        OpenSSL::SSL
        OpenSSL::Crypto
        pthread
        z
        ${CURL_LIBRARIES}
        CURL::libcurl
        ${GUMBO_LIBRARIES}
        Threads::Threads
)

# Link Inja if available (header-only library, no linking required)

# Link Redis libraries if available
if(REDIS_AVAILABLE)
    target_link_libraries(server PRIVATE ${REDIS_LIBRARIES})
    target_compile_definitions(server PRIVATE REDIS_AVAILABLE)
endif()

# Link uSockets library
target_link_libraries(server
    PRIVATE
        /usr/local/lib/libuSockets.a
)

# Install mongodb_instance target
install(TARGETS mongodb_instance
    EXPORT MongoDBInstanceTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Export mongodb_instance configuration
install(EXPORT MongoDBInstanceTargets
    FILE MongoDBInstanceTargets.cmake
    NAMESPACE SearchEngine::
    DESTINATION lib/cmake/SearchEngine
)

# Enable testing and add tests
option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
