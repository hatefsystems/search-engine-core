find_package(Catch2 3 REQUIRED)

add_executable(test_pulse_core
    test_pulse_core.cpp
    ${CMAKE_SOURCE_DIR}/src/pulse/PulseQueryNormalizer.cpp
    ${CMAKE_SOURCE_DIR}/src/pulse/PulseScoring.cpp
)

target_include_directories(test_pulse_core PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_pulse_core PRIVATE
    Catch2::Catch2WithMain
)

include(CTest)
include(Catch)

add_test(NAME pulse_core_test COMMAND test_pulse_core)
set_tests_properties(pulse_core_test PROPERTIES
    TIMEOUT 30
    LABELS "pulse;unit"
)

catch_discover_tests(test_pulse_core)

