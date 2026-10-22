include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1)
FetchContent_MakeAvailable(Catch2)

add_executable(als_tests test_smoke.cpp test_sample_clock.cpp test_loop_timing.cpp)
target_link_libraries(als_tests PRIVATE als_core Catch2::Catch2WithMain als_warnings)

include(Catch)
catch_discover_tests(als_tests)

# Enforces the architecture rule: core and storage must not depend on an audio or UI framework.
add_test(
    NAME no_framework_includes
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${PROJECT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_no_framework_includes.cmake)
