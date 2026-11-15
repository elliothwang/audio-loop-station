# Sample clock, loop timing, track data, loop controller, and pan law. Plain C++, no framework headers.
add_library(als_core STATIC
    ControllerSnapshot.cpp
    ControllerSnapshot.h
    LoopController.cpp
    LoopController.h
    LoopPlayback.cpp
    LoopPlayback.h
    LoopPlayer.cpp
    LoopPlayer.h
    LoopTiming.cpp
    LoopTiming.h
    PanLaw.cpp
    PanLaw.h
    SampleClock.cpp
    SampleClock.h
)
target_include_directories(als_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(als_core PRIVATE als_warnings)
