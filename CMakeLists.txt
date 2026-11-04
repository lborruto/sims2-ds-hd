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

add_executable(sims2wide src/main.cpp src/platform.cpp)
target_include_directories(sims2wide PRIVATE melonDS/src src src/thirdparty)
target_link_libraries(sims2wide PRIVATE core SDL2::SDL2-static)
target_compile_definitions(sims2wide PRIVATE SIMS2WIDE_VERSION="${PROJECT_VERSION}")
