cmake_minimum_required(VERSION 3.24)

# Find required packages for search_core
find_package(nlohmann_json 3.10.5 REQUIRED)

# Define search_core library
add_library(search_core
    SearchClient.cpp
    QueryParser.cpp
    Scorer.cpp
)

# Set include directories
target_include_directories(search_core 
    PUBLIC 
        ${CMAKE_SOURCE_DIR}/include
    PRIVATE
        ${REDIS_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(search_core
    PUBLIC 
        nlohmann_json::nlohmann_json
    PRIVATE
        ${REDIS_LIBRARIES}
)

# Set C++20 standard
target_compile_features(search_core PUBLIC cxx_std_20)

# Add compile definitions
target_compile_definitions(search_core PRIVATE REDIS_AVAILABLE)

# Optional: Add compiler-specific optimizations
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(search_core PRIVATE -O3 -DNDEBUG)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(search_core PRIVATE -g -O0)
endif() 