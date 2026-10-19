# Must stay free of JUCE, miniaudio, and Dear ImGui headers (checked by ctest).
add_library(als_storage INTERFACE)
target_link_libraries(als_storage INTERFACE als_core)
