cmake_minimum_required(VERSION 3.21)
# The release workflow checks that this version is the tag's (vX.Y.Z).
project(sims2wide VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Only the melonDS core: its own Qt frontend is not used.
set(BUILD_QT_SDL OFF CACHE BOOL "" FORCE)
set(ENABLE_JIT OFF CACHE BOOL "" FORCE)
set(ENABLE_GDBSTUB OFF CACHE BOOL "" FORCE)
set(ENABLE_OGLRENDERER OFF CACHE BOOL "" FORCE)
add_subdirectory(melonDS EXCLUDE_FROM_ALL)

# CMake 4 dropped compatibility with projects asking for policies older than 3.5; SDL2's own
# CMakeLists still does. This keeps CMake 3.21 to 4.x working.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

include(FetchContent)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
FetchContent_Declare(sdl2
    URL https://github.com/libsdl-org/SDL/releases/download/release-2.30.9/SDL2-2.30.9.tar.gz
    URL_HASH SHA256=24b574f71c87a763f50704bbb630cbe38298d544a1f890f099a4696b1d6beba4)
FetchContent_MakeAvailable(sdl2)

add_executable(sims2wide
    src/main.cpp src/platform.cpp
    src/audio.cpp src/controls.cpp src/input.cpp src/layout.cpp src/menu.cpp src/picker.cpp
    src/scene.cpp src/script.cpp src/settings.cpp src/shots.cpp src/state.cpp src/ui.cpp
    src/video.cpp
    src/thirdparty/tinyfiledialogs.c)
target_include_directories(sims2wide PRIVATE melonDS/src src src/thirdparty)
target_link_libraries(sims2wide PRIVATE core SDL2::SDL2-static)
target_compile_definitions(sims2wide PRIVATE SIMS2WIDE_VERSION="${PROJECT_VERSION}")

# macOS: an app to double-click (the release builds it, for Apple silicon and Intel at once with
# -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"). Off by default, so that build/sims2wide stays a
# plain program for the command line.
option(SIMS2WIDE_MAC_APP "Build Sims 2 DS Wide.app instead of a plain program (macOS)" OFF)
if(APPLE AND SIMS2WIDE_MAC_APP)
    set(ICNS "${CMAKE_CURRENT_BINARY_DIR}/AppIcon.icns")
    add_custom_command(OUTPUT "${ICNS}"
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/packaging/make_icns.sh" "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icon.png" "${ICNS}"
        DEPENDS packaging/icon.png packaging/make_icns.sh)
    target_sources(sims2wide PRIVATE "${ICNS}")
    set_source_files_properties("${ICNS}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(sims2wide PROPERTIES
        MACOSX_BUNDLE TRUE
        OUTPUT_NAME "Sims 2 DS Wide"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/packaging/Info.plist.in"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}")
endif()

# Windows: a program that opens no console window when double-clicked (main.cpp attaches to the
# terminal it was started from, if any), with its icon, linked statically so that the .exe needs
# no DLL beside it. The file picker uses comdlg32 and ole32.
if(WIN32)
    target_sources(sims2wide PRIVATE packaging/sims2wide.rc)
    set_target_properties(sims2wide PROPERTIES WIN32_EXECUTABLE TRUE)
    if(TARGET SDL2::SDL2main)
        target_link_libraries(sims2wide PRIVATE SDL2::SDL2main)
    endif()
    target_link_libraries(sims2wide PRIVATE comdlg32 ole32)
    if(MINGW)
        target_link_options(sims2wide PRIVATE -static)
    endif()
endif()

# Checks of the layouts' touch mapping (not built by default): ninja -C build layout_test && ./build/layout_test
add_executable(layout_test EXCLUDE_FROM_ALL tests/layout_test.cpp src/layout.cpp)
target_include_directories(layout_test PRIVATE melonDS/src src)
