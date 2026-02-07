cmake_minimum_required(VERSION 3.15)

# Find Catch2 package
find_package(Catch2 3 REQUIRED)

# Enable testing
enable_testing()

# Add test executables
add_subdirectory(crawler)
add_subdirectory(storage)
add_subdirectory(search_core)
add_subdirectory(scoring)
add_subdirectory(webserver)
add_subdirectory(common)
add_subdirectory(privacy) 