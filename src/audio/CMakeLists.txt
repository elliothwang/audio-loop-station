add_library(als_audio STATIC
    MainComponentAudio.cpp)

target_include_directories(als_audio PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui
)

target_link_libraries(als_audio 
    PUBLIC
        als_core
        juce::juce_audio_utils
    PRIVATE
        als_warnings
    )
