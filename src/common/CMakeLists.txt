cmake_minimum_required(VERSION 3.12)
project(common)

# Add common library
add_library(common STATIC JsMinifierClient.cpp Logger.cpp UrlSanitizer.cpp UrlCanonicalizer.cpp SlugGenerator.cpp)

target_include_directories(common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include)

# Set C++ standard
set_target_properties(common PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Add logger_test executable
add_executable(logger_test logger_test.cpp)
target_sources(logger_test PRIVATE Logger.cpp)
target_link_libraries(logger_test PRIVATE common) 