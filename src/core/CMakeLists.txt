# Loop clock, track data, and loop controller go here. Plain C++, no framework headers.
add_library(als_core INTERFACE)
target_include_directories(als_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/..)
