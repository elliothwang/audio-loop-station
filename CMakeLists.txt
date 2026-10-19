cmake_minimum_required(VERSION 3.21)
project(AudioLoopStation LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# One interface target carries the shared warning flags; every target links it.
add_library(als_warnings INTERFACE)
if(MSVC)
    target_compile_options(als_warnings INTERFACE /W4 /WX)
else()
    target_compile_options(als_warnings INTERFACE -Wall -Wextra -Wpedantic -Werror)
endif()

include(CTest)

add_subdirectory(src/core)
add_subdirectory(src/audio)
add_subdirectory(src/storage)
add_subdirectory(src/ui)
add_subdirectory(src/app)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
