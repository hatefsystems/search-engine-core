cmake_minimum_required(VERSION 3.12)
project("search-engine-core" VERSION 0.1.0 LANGUAGES CXX)

# Enable Hot Reload for MSVC compilers if supported
if (POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# Set vcpkg toolchain
set(CMAKE_TOOLCHAIN_FILE "D:/Projects/hatef.ir/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set architecture to x64
set(CMAKE_VS_PLATFORM_NAME "x64")
set(CMAKE_GENERATOR_PLATFORM "x64")

# Set runtime library to MultiThreadedDebugDLL
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebugDLL")

# Find required packages
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(libuv CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(unofficial-gumbo CONFIG REQUIRED)
find_package(redis++ CONFIG REQUIRED)

# Add include directory to include path for all targets
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Find uWebSockets
find_path(UWS_INCLUDE_DIR
    NAMES uWebSockets/App.h
    PATHS
    ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/include
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/uWebSockets
    /usr/local/include
    /usr/include
)

find_library(UWS_LIBRARY
    NAMES uSockets
    PATHS
    ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/uWebSockets
    /usr/local/lib
    /usr/lib
)

if(NOT UWS_INCLUDE_DIR OR NOT UWS_LIBRARY)
    message(FATAL_ERROR "uWebSockets not found. Please install it using vcpkg: vcpkg install uwebsockets:x64-windows")
endif()

# MongoDB paths
set(MONGOC_DRIVER_DIR "C:/mongo-c-driver")
set(MONGOCXX_DRIVER_DIR "C:/mongo-cxx-driver")

# Set environment paths
set(ENV{PATH} "${MONGOCXX_DRIVER_DIR}/bin;${MONGOC_DRIVER_DIR}/bin;$ENV{PATH}")

# Add the crawler library subdirectory
add_subdirectory(src/crawler)

# Add the common library subdirectory
add_subdirectory(src/common)

# Create imported targets for MongoDB libraries
add_library(bson-1.0 STATIC IMPORTED)
set_target_properties(bson-1.0 PROPERTIES
    IMPORTED_LOCATION "${MONGOC_DRIVER_DIR}/lib/bson-1.0.lib"
)

add_library(mongoc-1.0 STATIC IMPORTED)
set_target_properties(mongoc-1.0 PROPERTIES
    IMPORTED_LOCATION "${MONGOC_DRIVER_DIR}/lib/mongoc-1.0.lib"
)

add_library(bsoncxx STATIC IMPORTED)
set_target_properties(bsoncxx PROPERTIES
    IMPORTED_LOCATION "${MONGOCXX_DRIVER_DIR}/lib/bsoncxx-v_noabi-rhi-x64-v143-md.lib"
)

add_library(mongocxx STATIC IMPORTED)
set_target_properties(mongocxx PROPERTIES
    IMPORTED_LOCATION "${MONGOCXX_DRIVER_DIR}/lib/mongocxx-v_noabi-rhi-x64-v143-md.lib"
)

# Add source files for the main executable (excluding crawler sources)
file(GLOB_RECURSE MAIN_SOURCES "src/*.cpp" "src/main.cpp") # Assuming main.cpp is your entry point

# Add Logger implementation
list(APPEND MAIN_SOURCES "src/common/Logger.cpp")

# Remove crawler sources and test sources from MAIN_SOURCES if they were picked up by GLOB_RECURSE
list(FILTER MAIN_SOURCES EXCLUDE REGEX ".*crawler/.*\\.cpp$")
list(FILTER MAIN_SOURCES EXCLUDE REGEX ".*test.*\\.cpp$")

file(GLOB_RECURSE HEADERS "include/*.h" "include/*.hpp")

# Add executable
add_executable(${PROJECT_NAME} ${MAIN_SOURCES} ${HEADERS})

# Set include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/include
    ${MONGOCXX_DRIVER_DIR}/include/mongocxx/v_noabi
    ${MONGOCXX_DRIVER_DIR}/include/bsoncxx/v_noabi
    ${MONGOC_DRIVER_DIR}/include/libbson-1.0
    ${MONGOC_DRIVER_DIR}/include/libmongoc-1.0
    ${UWS_INCLUDE_DIR}
    ${CURL_INCLUDE_DIRS}
)

# Link directories
target_link_directories(${PROJECT_NAME} PRIVATE
    ${MONGOC_DRIVER_DIR}/lib
    ${MONGOCXX_DRIVER_DIR}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/lib
)

# Link libraries - Note the order is important here
target_link_libraries(${PROJECT_NAME} PRIVATE
    crawler 
    $<IF:$<TARGET_EXISTS:libuv::uv_a>,libuv::uv_a,libuv::uv>  
    ${UWS_LIBRARY}    
    OpenSSL::SSL
    OpenSSL::Crypto
    ZLIB::ZLIB
    nlohmann_json::nlohmann_json
    CURL::libcurl
    bson-1.0
    mongoc-1.0
    bsoncxx
    mongocxx
    redis++::redis++
    ws2_32
    iphlpapi
    crypt32
    mswsock
    secur32
    bcrypt
    userenv
    msvcrtd
    msvcprtd
    vcruntimed
    ucrtd
    kernel32
    user32
    gdi32
    winspool
    shell32
    ole32
    oleaut32
    uuid
    comdlg32
    advapi32
)

# Set runtime environment
set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_ENVIRONMENT "PATH=${MONGOCXX_DRIVER_DIR}/bin;${MONGOC_DRIVER_DIR}/bin;$(LocalDebuggerEnvironment)"
)

# Install rules
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Add tests if enabled
option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    # Ensure src/crawler (which defines the 'crawler' target) is added before tests
    # add_subdirectory(src/crawler) # This is now added earlier
    add_subdirectory(tests)
endif()
