cmake_minimum_required(VERSION 3.15)
project(JobModels)

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

# Define source files for job models
set(JOB_MODELS_SOURCES
    Job.cpp
    JobConfig.cpp
    JobResult.cpp
    ../infrastructure.cpp
)

# Define header files
set(JOB_MODELS_HEADERS
    ../../include/search_engine/models/Job.h
    ../../include/search_engine/models/JobConfig.h
    ../../include/search_engine/models/JobResult.h
    ../../include/infrastructure.h
)

# Create job models library
add_library(job_models STATIC ${JOB_MODELS_SOURCES} ${JOB_MODELS_HEADERS})

# Create alias for consistent naming
add_library(SearchEngine::JobModels ALIAS job_models)

# Set include directories
target_include_directories(job_models
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link required libraries
target_link_libraries(job_models
    PUBLIC
        common
        mongo::bsoncxx_shared
        mongo::mongocxx_shared
        mongodb_instance
        Threads::Threads
)

# Platform-specific settings removed (Linux-only build)

# Set compiler flags (Linux/GCC)
target_compile_options(job_models PRIVATE 
    -Wall 
    -Wextra 
    -Wpedantic
    -Wno-unused-parameter  # MongoDB drivers have unused parameters
)

# Preprocessor definitions
target_compile_definitions(job_models PRIVATE
    BSONCXX_STATIC
    MONGOCXX_STATIC
)

# Create individual component targets for selective building
add_library(Job STATIC Job.cpp ../infrastructure.cpp)
target_include_directories(Job 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(Job PUBLIC common mongo::bsoncxx_shared mongo::mongocxx_shared mongodb_instance)
target_compile_definitions(Job PRIVATE
    BSONCXX_STATIC
    MONGOCXX_STATIC
)

add_library(JobConfig STATIC JobConfig.cpp ../infrastructure.cpp)
target_include_directories(JobConfig 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(JobConfig PUBLIC common mongo::bsoncxx_shared mongo::mongocxx_shared mongodb_instance)
target_compile_definitions(JobConfig PRIVATE
    BSONCXX_STATIC
    MONGOCXX_STATIC
)

add_library(JobResult STATIC JobResult.cpp ../infrastructure.cpp)
target_include_directories(JobResult 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(JobResult PUBLIC common mongo::bsoncxx_shared mongo::mongocxx_shared mongodb_instance)
target_compile_definitions(JobResult PRIVATE
    BSONCXX_STATIC
    MONGOCXX_STATIC
)

# Export targets for use by other CMake projects
install(TARGETS job_models Job JobConfig JobResult
    EXPORT JobModelsTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Also install common as part of JobModelsTargets since models depend on it
install(TARGETS common
    EXPORT JobModelsTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install headers
install(FILES ${JOB_MODELS_HEADERS}
    DESTINATION include/search_engine/models
)

# Export configuration
install(EXPORT JobModelsTargets
    FILE JobModelsTargets.cmake
    NAMESPACE SearchEngine::
    DESTINATION lib/cmake/SearchEngine
)

# Create config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    JobModelsConfigVersion.cmake
    VERSION 1.0.0
    COMPATIBILITY AnyNewerVersion
)

# Status reporting
message(STATUS "Job Models Library Configuration:")
message(STATUS "  MongoDB Support: YES")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")

message(STATUS "Job Models library will include Job, JobConfig, and JobResult classes")
