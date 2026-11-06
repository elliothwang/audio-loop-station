# Saving and loading loops. Uses JUCE for files, JSON, and WAV, but not its UI or audio devices.
add_library(als_storage STATIC
    LoopFile.cpp
    LoopFile.h
)
target_link_libraries(als_storage
    PUBLIC
        als_core
        juce::juce_audio_formats
    PRIVATE
        als_warnings
)
