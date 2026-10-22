# Fails if any file under core/ or storage/ includes a JUCE, miniaudio, or ImGui header.
# Run with: cmake -DSOURCE_ROOT=<repo root> -P check_no_framework_includes.cmake
file(GLOB_RECURSE sources
    "${SOURCE_ROOT}/src/core/*.h" "${SOURCE_ROOT}/src/core/*.hpp" "${SOURCE_ROOT}/src/core/*.cpp"
    "${SOURCE_ROOT}/src/storage/*.h" "${SOURCE_ROOT}/src/storage/*.hpp"
    "${SOURCE_ROOT}/src/storage/*.cpp")

set(violations "")
foreach(source IN LISTS sources)
    file(STRINGS "${source}" matches REGEX "#[ \t]*include[ \t]*[<\"].*([Jj][Uu][Cc][Ee]|miniaudio|imgui)")
    if(matches)
        string(APPEND violations "${source}: ${matches}\n")
    endif()
endforeach()

if(violations)
    message(FATAL_ERROR "Framework includes found in core/ or storage/:\n${violations}")
endif()
