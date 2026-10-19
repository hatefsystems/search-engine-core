cmake_minimum_required(VERSION 3.16)

# Also buildable independently of the application's database/browser toolchain.
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    project(CrawlSessionTests LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    find_package(Threads REQUIRED)
    find_package(CURL REQUIRED)
    find_package(Catch2 3 QUIET)
    if(NOT Catch2_FOUND)
        include(FetchContent)
        FetchContent_Declare(Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG 6e79e682b726f524310d55dec8ddac4e9c52fb5f
        )
        FetchContent_MakeAvailable(Catch2)
    endif()
    enable_testing()
endif()

get_filename_component(SEARCH_ENGINE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
add_executable(session_scheduler_tests
    ../session_priority_queue_tests.cpp
    ../crawler_manager_tests.cpp
    ${SEARCH_ENGINE_ROOT}/src/crawler/CrawlerManager.cpp
    ${SEARCH_ENGINE_ROOT}/src/common/Logger.cpp
)
target_include_directories(session_scheduler_tests PRIVATE ${SEARCH_ENGINE_ROOT}/include ${CURL_INCLUDE_DIRS})
target_link_libraries(session_scheduler_tests PRIVATE Catch2::Catch2WithMain Threads::Threads)

set(SESSION_SANITIZER "none" CACHE STRING "Session test sanitizer: none or address")
if(SESSION_SANITIZER STREQUAL "address")
    target_compile_options(session_scheduler_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(session_scheduler_tests PRIVATE -fsanitize=address,undefined)
elseif(NOT SESSION_SANITIZER STREQUAL "none")
    message(FATAL_ERROR "Unsupported SESSION_SANITIZER")
endif()

add_test(NAME session_scheduler_tests COMMAND session_scheduler_tests)
set_tests_properties(session_scheduler_tests PROPERTIES TIMEOUT 60)
