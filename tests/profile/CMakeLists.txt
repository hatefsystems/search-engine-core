cmake_minimum_required(VERSION 3.15)

# May also run independently of MongoDB/uWebSockets:
# cmake -S tests/profile -B build/profile && cmake --build build/profile
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(public_profile_tests LANGUAGES CXX)
    enable_testing()
endif()
find_package(GTest REQUIRED)
get_filename_component(PROFILE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
add_executable(test_public_profile
    test_public_profile.cpp
    ${PROFILE_ROOT}/src/seo/SEOGenerator.cpp
    ${PROFILE_ROOT}/src/common/Logger.cpp
)
target_compile_features(test_public_profile PRIVATE cxx_std_20)
target_include_directories(test_public_profile PRIVATE ${PROFILE_ROOT}/include)
target_link_libraries(test_public_profile PRIVATE GTest::gtest_main)
target_compile_definitions(test_public_profile PRIVATE PROFILE_ROOT="${PROFILE_ROOT}")
add_test(NAME public_profile_test COMMAND test_public_profile)

find_package(OpenSSL REQUIRED)
add_executable(test_profile_editor test_profile_editor.cpp)
target_compile_features(test_profile_editor PRIVATE cxx_std_20)
target_include_directories(test_profile_editor PRIVATE ${PROFILE_ROOT}/include)
target_link_libraries(test_profile_editor PRIVATE GTest::gtest_main OpenSSL::Crypto)
add_test(NAME profile_editor_test COMMAND test_profile_editor)

foreach(profile_target test_public_profile test_profile_editor)
    target_sources(${profile_target} PRIVATE ${PROFILE_ROOT}/src/profile/ProfileContent.cpp ${PROFILE_ROOT}/src/profile/ProfileProjection.cpp)
    target_link_libraries(${profile_target} PRIVATE OpenSSL::Crypto)
endforeach()

add_executable(test_profile_content test_profile_content.cpp ${PROFILE_ROOT}/src/profile/ProfileContent.cpp ${PROFILE_ROOT}/src/profile/ProfileProjection.cpp)
target_compile_features(test_profile_content PRIVATE cxx_std_20)
target_include_directories(test_profile_content PRIVATE ${PROFILE_ROOT}/include)
target_link_libraries(test_profile_content PRIVATE GTest::gtest_main OpenSSL::Crypto)
add_test(NAME profile_content_test COMMAND test_profile_content)
