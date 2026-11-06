cmake_minimum_required(VERSION 3.21)
# The release workflow checks that this version is the tag's (vX.Y.Z).
project(sims2wide VERSION 0.1.1 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Only the melonDS core: its own Qt frontend is not used.
set(BUILD_QT_SDL OFF CACHE BOOL "" FORCE)
set(ENABLE_JIT OFF CACHE BOOL "" FORCE)
set(ENABLE_GDBSTUB OFF CACHE BOOL "" FORCE)
set(ENABLE_OGLRENDERER OFF CACHE BOOL "" FORCE)

# melonDS stays the upstream 1.1 release; the few hooks the looks need (sharper and widescreen
# 3D, 60 fps in-between frames) are patches in patches/melonDS, applied to the submodule's
# files here, in order. Idempotent: a patch that already reverses cleanly is applied already.
# To get the pristine files back: git -C melonDS checkout -- . (then configure again).
find_package(Git REQUIRED)
file(GLOB SIMS2WIDE_MELONDS_PATCHES "${CMAKE_CURRENT_SOURCE_DIR}/patches/melonDS/*.patch")
list(SORT SIMS2WIDE_MELONDS_PATCHES)
foreach(patch IN LISTS SIMS2WIDE_MELONDS_PATCHES)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${patch}")
    execute_process(COMMAND "${GIT_EXECUTABLE}" apply --reverse --check "${patch}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/melonDS" RESULT_VARIABLE already OUTPUT_QUIET ERROR_QUIET)
    if(NOT already EQUAL 0)
        execute_process(COMMAND "${GIT_EXECUTABLE}" apply --whitespace=nowarn "${patch}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/melonDS" RESULT_VARIABLE failed ERROR_VARIABLE why)
        if(failed)
            message(FATAL_ERROR "Cannot apply ${patch} to melonDS: ${why}Is the submodule at the 1.1 tag? (git submodule update --init)")
        endif()
        get_filename_component(name "${patch}" NAME)
        message(STATUS "melonDS: applied ${name}")
    endif()
endforeach()

add_subdirectory(melonDS EXCLUDE_FROM_ALL)

# The sharper and widescreen 3D picture (src/melonds_ext, from Pokemon FullDex), built into
# melonDS's core library, which calls it. Its rasteriser is a hot per-pixel loop.
target_sources(core PRIVATE src/melonds_ext/GPU3D_Wide.cpp)
target_include_directories(core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/melonds_ext" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/melonDS/src")
if(NOT MSVC)
    set_source_files_properties(src/melonds_ext/GPU3D_Wide.cpp TARGET_DIRECTORY core PROPERTIES COMPILE_OPTIONS "-O3;-ffast-math")
endif()

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

# The UI kit (src/ui): the HD interface's panels, text and chips, rasterised in software. Its
# font (assets/fonts, Nunito, SIL OFL 1.1) is embedded as byte arrays generated at build time
# by cmake/embed.cmake, so the program stays one file.
set(SIMS2WIDE_FONT_NAMES "nunito_bold|nunito_extrabold")
set(SIMS2WIDE_FONT_FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/Nunito-Bold.ttf|${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/Nunito-ExtraBold.ttf")
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/fonts_embedded.cpp"
    COMMAND "${CMAKE_COMMAND}" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/fonts_embedded.cpp" "-DNAMES=${SIMS2WIDE_FONT_NAMES}"
        "-DFILES=${SIMS2WIDE_FONT_FILES}" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed.cmake"
    DEPENDS cmake/embed.cmake assets/fonts/Nunito-Bold.ttf assets/fonts/Nunito-ExtraBold.ttf
    COMMENT "Embedding the UI's font" VERBATIM)
add_library(uikit STATIC src/ui/kit.cpp src/ui/font.cpp "${CMAKE_CURRENT_BINARY_DIR}/fonts_embedded.cpp")
target_include_directories(uikit PUBLIC melonDS/src src PRIVATE src/thirdparty)

add_executable(sims2wide
    src/main.cpp src/platform.cpp
    src/activities.cpp src/audio.cpp src/backup.cpp src/clock.cpp src/compose.cpp src/console.cpp src/controls.cpp src/gamestate.cpp src/hotspots.cpp src/input.cpp src/layout.cpp
    src/looks.cpp src/menu.cpp src/mic.cpp src/navigator.cpp src/picker.cpp src/port/dialogue.cpp src/port/icons.cpp src/port/gift.cpp src/port/interact.cpp src/port/menuview.cpp src/port/panelmenu.cpp src/port/pockets.cpp src/port/goals.cpp src/port/relations.cpp src/port/options.cpp src/port/popup.cpp src/port/phone.cpp src/port/note.cpp src/port/catalogue.cpp src/port/build.cpp src/port/checkin.cpp src/port/roominfo.cpp src/port/statusbar.cpp src/recorder.cpp src/romdata.cpp src/romdata_nds.cpp src/scene.cpp src/screens.cpp src/script.cpp
    src/settings.cpp src/shots.cpp src/state.cpp src/touchscript.cpp src/ui.cpp src/video.cpp
    src/thirdparty/tinyfiledialogs.c)
target_include_directories(sims2wide PRIVATE melonDS/src src src/thirdparty)
target_link_libraries(sims2wide PRIVATE core uikit SDL2::SDL2-static)
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
add_executable(layout_test EXCLUDE_FROM_ALL tests/layout_test.cpp src/layout.cpp src/activities.cpp src/compose.cpp src/console.cpp src/hotspots.cpp src/screens.cpp
    src/touchscript.cpp src/ui.cpp src/port/dialogue.cpp src/port/icons.cpp src/port/gift.cpp src/port/interact.cpp src/port/menuview.cpp src/port/panelmenu.cpp src/port/pockets.cpp src/port/goals.cpp src/port/relations.cpp src/port/options.cpp src/port/popup.cpp src/port/phone.cpp src/port/note.cpp src/port/catalogue.cpp src/port/build.cpp src/port/checkin.cpp src/port/roominfo.cpp
    src/port/statusbar.cpp src/romdata.cpp)
target_include_directories(layout_test PRIVATE melonDS/src src)
target_link_libraries(layout_test PRIVATE uikit)

# Checks of the synthesised touches (not built by default): ninja -C build touchscript_test && ./build/touchscript_test
add_executable(touchscript_test EXCLUDE_FROM_ALL tests/touchscript_test.cpp src/touchscript.cpp)
target_include_directories(touchscript_test PRIVATE melonDS/src src)

# Checks of the touch targets and focus moves (not built by default): ninja -C build hotspots_test && ./build/hotspots_test
add_executable(hotspots_test EXCLUDE_FROM_ALL tests/hotspots_test.cpp src/hotspots.cpp src/screens.cpp)
target_include_directories(hotspots_test PRIVATE melonDS/src src)

# Checks of the screen rules on synthetic screens (not built by default): ninja -C build screens_test && ./build/screens_test
add_executable(screens_test EXCLUDE_FROM_ALL tests/screens_test.cpp src/screens.cpp)
target_include_directories(screens_test PRIVATE melonDS/src src)

# Checks of CONSOLE's navigator (not built by default): ninja -C build navigator_test && ./build/navigator_test
add_executable(navigator_test EXCLUDE_FROM_ALL tests/navigator_test.cpp src/activities.cpp src/navigator.cpp src/hotspots.cpp src/screens.cpp src/port/menuview.cpp
    src/touchscript.cpp)
target_include_directories(navigator_test PRIVATE melonDS/src src)

# Checks of the bindings and the settings file's loading (not built by default): ninja -C build controls_test && ./build/controls_test
add_executable(controls_test EXCLUDE_FROM_ALL tests/controls_test.cpp src/controls.cpp src/settings.cpp src/layout.cpp)
target_include_directories(controls_test PRIVATE melonDS/src src)
target_link_libraries(controls_test PRIVATE SDL2::SDL2-static)

# Checks of the screen manager reader on a synthetic RAM (not built by default): ninja -C build gamestate_test && ./build/gamestate_test
add_executable(gamestate_test EXCLUDE_FROM_ALL tests/gamestate_test.cpp src/gamestate.cpp src/screens.cpp)
target_include_directories(gamestate_test PRIVATE melonDS/src src)

# Checks of the ROM's archive, string tables, compression, sprites and the icon cache on synthetic data (not built by default): ninja -C build romdata_test && ./build/romdata_test
add_executable(romdata_test EXCLUDE_FROM_ALL tests/romdata_test.cpp src/romdata.cpp src/port/icons.cpp src/port/statusbar.cpp)
target_include_directories(romdata_test PRIVATE melonDS/src src)
target_link_libraries(romdata_test PRIVATE uikit)

# Checks of the UI kit (wrap, the raster's pixels and determinism; not built by default): ninja -C build uikit_test && ./build/uikit_test
add_executable(uikit_test EXCLUDE_FROM_ALL tests/uikit_test.cpp)
target_link_libraries(uikit_test PRIVATE uikit)
