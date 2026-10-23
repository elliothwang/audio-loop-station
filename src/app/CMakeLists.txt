juce_add_gui_app(audio_loop_station
    PRODUCT_NAME "Audio Loop Station"
)

target_sources(audio_loop_station PRIVATE
    main.cpp
)

target_link_libraries(audio_loop_station PRIVATE
    als_core
    als_audio
    als_storage
    als_ui
    als_warnings
    juce::juce_recommended_config_flags
)