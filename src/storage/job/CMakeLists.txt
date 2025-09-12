add_library(job_storage_modular STATIC
    JobStorage.cpp
)

target_include_directories(job_storage_modular
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(job_storage_modular
    PUBLIC
        job_models
        job_database
        common
        mongo::bsoncxx_shared
        mongo::mongocxx_shared
        mongodb_instance
)

target_compile_definitions(job_storage_modular PRIVATE
    BSONCXX_STATIC
    MONGOCXX_STATIC
)

# Create alias for easier transition (can be used as job_storage)
add_library(job_storage ALIAS job_storage_modular)
