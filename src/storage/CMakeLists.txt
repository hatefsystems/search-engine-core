cmake_minimum_required(VERSION 3.15)
project(Storage)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Threads REQUIRED)

# Set CMake policy to handle imported targets properly
cmake_policy(SET CMP0111 NEW)

# MongoDB dependencies
find_package(bsoncxx REQUIRED)
find_package(mongocxx REQUIRED)

# Fix MongoDB targets for all configurations
if(TARGET mongo::bsoncxx_shared)
    get_target_property(BSON_LIB_DEBUG mongo::bsoncxx_shared IMPORTED_IMPLIB_DEBUG)
    get_target_property(BSON_LIB_RELEASE mongo::bsoncxx_shared IMPORTED_IMPLIB_RELEASE)
    
    if(BSON_LIB_DEBUG OR BSON_LIB_RELEASE)
        set_target_properties(mongo::bsoncxx_shared PROPERTIES
            IMPORTED_IMPLIB_MINSIZEREL "${BSON_LIB_RELEASE}"
            IMPORTED_IMPLIB_RELWITHDEBINFO "${BSON_LIB_RELEASE}"
        )
    endif()
endif()

if(TARGET mongo::mongocxx_shared)
    get_target_property(MONGO_LIB_DEBUG mongo::mongocxx_shared IMPORTED_IMPLIB_DEBUG)
    get_target_property(MONGO_LIB_RELEASE mongo::mongocxx_shared IMPORTED_IMPLIB_RELEASE)
    
    if(MONGO_LIB_DEBUG OR MONGO_LIB_RELEASE)
        set_target_properties(mongo::mongocxx_shared PROPERTIES
            IMPORTED_IMPLIB_MINSIZEREL "${MONGO_LIB_RELEASE}"
            IMPORTED_IMPLIB_RELWITHDEBINFO "${MONGO_LIB_RELEASE}"
        )
    endif()
endif()

# Redis dependencies (optional - may not be available in all environments)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(HIREDIS QUIET hiredis)
endif()

# Try to find redis-plus-plus
find_package(redis++ QUIET)

# Define source files
set(STORAGE_SOURCES
    MongoDBStorage.cpp
    ContentStorage.cpp
    SponsorStorage.cpp
    JobStorage.cpp
    ../infrastructure.cpp
)

# Add RedisSearchStorage only if Redis dependencies are available
if(redis++_FOUND OR HIREDIS_FOUND)
    list(APPEND STORAGE_SOURCES RedisSearchStorage.cpp)
    set(REDIS_AVAILABLE TRUE)
    message(STATUS "Redis dependencies found - including RedisSearchStorage")
else()
    set(REDIS_AVAILABLE FALSE)
    message(WARNING "Redis dependencies not found - RedisSearchStorage will be excluded")
endif()

# Define header files
set(STORAGE_HEADERS
    ../../include/search_engine/storage/SiteProfile.h
    ../../include/search_engine/storage/SponsorProfile.h
    ../../include/search_engine/storage/MongoDBStorage.h
    ../../include/search_engine/storage/SponsorStorage.h
    ../../include/search_engine/storage/ContentStorage.h
    ../../include/search_engine/storage/JobStorage.h
    ../../include/infrastructure.h
)

if(REDIS_AVAILABLE)
    list(APPEND STORAGE_HEADERS ../../include/search_engine/storage/RedisSearchStorage.h)
endif()

# Create storage library
add_library(storage STATIC ${STORAGE_SOURCES} ${STORAGE_HEADERS})

# Create alias for consistent naming
add_library(SearchEngine::Storage ALIAS storage)

# Set include directories
target_include_directories(storage
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link required libraries
target_link_libraries(storage
    PUBLIC
        common
        mongo::bsoncxx_shared
        mongo::mongocxx_shared
        mongodb_instance
        Threads::Threads
)

# Link Redis libraries if available
if(REDIS_AVAILABLE)
    if(redis++_FOUND)
        target_link_libraries(storage PUBLIC redis++::redis++)
        target_compile_definitions(storage PUBLIC REDIS_AVAILABLE)
    elseif(HIREDIS_FOUND)
        target_link_libraries(storage PUBLIC ${HIREDIS_LIBRARIES})
        target_include_directories(storage PRIVATE ${HIREDIS_INCLUDE_DIRS})
        target_compile_definitions(storage PUBLIC REDIS_AVAILABLE)
    endif()
endif()

# Platform-specific settings removed (Linux-only build)

# Set compiler flags (Linux/GCC)
target_compile_options(storage PRIVATE 
    -Wall 
    -Wextra 
    -Wpedantic
    -Wno-unused-parameter  # MongoDB drivers have unused parameters
)

# Preprocessor definitions
target_compile_definitions(storage PRIVATE
    BSONCXX_STATIC
    MONGOCXX_STATIC
)

# Create individual component targets for selective building
add_library(MongoDBStorage STATIC MongoDBStorage.cpp ../infrastructure.cpp)
target_include_directories(MongoDBStorage 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(MongoDBStorage PUBLIC common mongo::bsoncxx_shared mongo::mongocxx_shared mongodb_instance)

if(REDIS_AVAILABLE)
    add_library(RedisSearchStorage STATIC RedisSearchStorage.cpp ../infrastructure.cpp)
    target_include_directories(RedisSearchStorage 
        PUBLIC 
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
            $<INSTALL_INTERFACE:include>
    )
    if(redis++_FOUND)
        target_link_libraries(RedisSearchStorage PUBLIC common redis++::redis++)
    else()
        target_link_libraries(RedisSearchStorage PUBLIC common)
    endif()
    target_compile_definitions(RedisSearchStorage PUBLIC REDIS_AVAILABLE)
endif()

add_library(SponsorStorage STATIC SponsorStorage.cpp ../infrastructure.cpp)
target_include_directories(SponsorStorage 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(SponsorStorage PUBLIC common mongo::bsoncxx_shared mongo::mongocxx_shared mongodb_instance)
target_compile_definitions(SponsorStorage PRIVATE
    BSONCXX_STATIC
    MONGOCXX_STATIC
)

add_library(ContentStorage STATIC ContentStorage.cpp ../infrastructure.cpp)
target_include_directories(ContentStorage 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(ContentStorage PUBLIC common MongoDBStorage)
if(REDIS_AVAILABLE)
    target_link_libraries(ContentStorage PUBLIC RedisSearchStorage)
    target_compile_definitions(ContentStorage PUBLIC REDIS_AVAILABLE)
endif()

add_library(JobStorage STATIC JobStorage.cpp ../infrastructure.cpp)
target_include_directories(JobStorage 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(JobStorage PUBLIC common mongo::bsoncxx_shared mongo::mongocxx_shared mongodb_instance job_models)
target_compile_definitions(JobStorage PRIVATE
    BSONCXX_STATIC
    MONGOCXX_STATIC
)

# Export targets for use by other CMake projects
install(TARGETS storage MongoDBStorage SponsorStorage ContentStorage JobStorage
    EXPORT StorageTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Also install common as part of StorageTargets since storage depends on it
install(TARGETS common
    EXPORT StorageTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

if(REDIS_AVAILABLE)
    install(TARGETS RedisSearchStorage
        EXPORT StorageTargets
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
    )
endif()

# Install headers
install(FILES ${STORAGE_HEADERS}
    DESTINATION include/search_engine/storage
)

# Export configuration
install(EXPORT StorageTargets
    FILE StorageTargets.cmake
    NAMESPACE SearchEngine::
    DESTINATION lib/cmake/SearchEngine
)

# Create config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    StorageConfigVersion.cmake
    VERSION 1.0.0
    COMPATIBILITY AnyNewerVersion
)

# Status reporting
message(STATUS "Storage Library Configuration:")
message(STATUS "  MongoDB Support: YES")
message(STATUS "  Redis Support: ${REDIS_AVAILABLE}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")

if(REDIS_AVAILABLE)
    message(STATUS "Storage library will include all components (MongoDB + Redis + ContentStorage)")
else()
    message(STATUS "Storage library will include limited components (MongoDB + ContentStorage only)")
    message(STATUS "Install Redis dependencies to enable full functionality")
endif() 