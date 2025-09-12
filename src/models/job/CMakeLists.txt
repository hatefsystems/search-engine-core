add_library(job_models_modular STATIC
    Job.cpp
    JobConfig.cpp
    JobResult.cpp
)

target_include_directories(job_models_modular
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(job_models_modular
    PUBLIC
        common
        mongo::bsoncxx_shared
        mongo::mongocxx_shared
        mongodb_instance
)

target_compile_definitions(job_models_modular PRIVATE
    BSONCXX_STATIC
    MONGOCXX_STATIC
)

# Create alias for easier transition (can be used as job_models)
add_library(job_models ALIAS job_models_modular)
