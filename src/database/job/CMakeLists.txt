add_library(job_database_modular STATIC
    JobIndexes.cpp
    JobMigrations.cpp
)

target_include_directories(job_database_modular
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(job_database_modular
    PUBLIC
        job_models
        common
        mongo::bsoncxx_shared
        mongo::mongocxx_shared
        mongodb_instance
)

target_compile_definitions(job_database_modular PRIVATE
    BSONCXX_STATIC
    MONGOCXX_STATIC
)

# Create alias for easier transition (can be used as job_database)
add_library(job_database ALIAS job_database_modular)
