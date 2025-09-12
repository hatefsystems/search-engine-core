cmake_minimum_required(VERSION 3.15)
project(JobDatabase)

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
    get_target_property(BSON_LIB_RELEASE mongo::mongocxx_shared IMPORTED_IMPLIB_RELEASE)
    
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

# Define source files for job database components - job sources moved to src/database/job/
set(JOB_DATABASE_SOURCES
    ../infrastructure.cpp
)

# Define header files (headers are inline in the cpp files for this component)
set(JOB_DATABASE_HEADERS
    ../../include/Logger.h
    ../../include/mongodb.h
    ../../include/infrastructure.h
)

# Create job database library
add_library(job_database STATIC ${JOB_DATABASE_SOURCES} ${JOB_DATABASE_HEADERS})

# Create alias for consistent naming
add_library(SearchEngine::JobDatabase ALIAS job_database)

# Set include directories
target_include_directories(job_database
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link required libraries
target_link_libraries(job_database
    PUBLIC
        common
        mongo::bsoncxx_shared
        mongo::mongocxx_shared
        mongodb_instance
        Threads::Threads
)

# Platform-specific settings removed (Linux-only build)

# Set compiler flags (Linux/GCC)
target_compile_options(job_database PRIVATE 
    -Wall 
    -Wextra 
    -Wpedantic
    -Wno-unused-parameter  # MongoDB drivers have unused parameters
)

# Preprocessor definitions
target_compile_definitions(job_database PRIVATE
    BSONCXX_STATIC
    MONGOCXX_STATIC
)

# Individual job component targets moved to src/database/job/CMakeLists.txt
# (This allows for Grok-style modular structure)

# Export targets for use by other CMake projects
install(TARGETS job_database
    EXPORT JobDatabaseTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Also install common as part of JobDatabaseTargets since database depends on it
install(TARGETS common
    EXPORT JobDatabaseTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install headers
install(FILES ${JOB_DATABASE_HEADERS}
    DESTINATION include/search_engine/database
)

# Export configuration
install(EXPORT JobDatabaseTargets
    FILE JobDatabaseTargets.cmake
    NAMESPACE SearchEngine::
    DESTINATION lib/cmake/SearchEngine
)

# Create config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    JobDatabaseConfigVersion.cmake
    VERSION 1.0.0
    COMPATIBILITY AnyNewerVersion
)

# Status reporting
message(STATUS "Job Database Library Configuration:")
message(STATUS "  MongoDB Support: YES")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")

message(STATUS "Job Database library will include JobIndexes and JobMigrations utilities")
