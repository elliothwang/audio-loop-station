# Sample clock, loop timing, track data, and loop controller. Plain C++, no framework headers.
add_library(als_core STATIC
    SampleClock.cpp
    SampleClock.h
)
target_include_directories(als_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(als_core PRIVATE als_warnings)
