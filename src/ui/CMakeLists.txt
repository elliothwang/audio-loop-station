add_library(als_ui STATIC
    MainComponent.cpp
    MainComponent.h
    TrackRowComponent.cpp
    TrackRowComponent.h
    SaveLoopComponent.cpp
    SaveLoopComponent.h
    LoadLoopComponent.cpp
    LoadLoopComponent.h
)

target_include_directories(als_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(als_ui PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(als_ui
    PUBLIC
        als_core
        juce::juce_gui_basics
        juce::juce_audio_utils
    PRIVATE
        als_warnings
)