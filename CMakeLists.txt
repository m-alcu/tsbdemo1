cmake_minimum_required(VERSION 3.16)
project(tsbdemo CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# SDL3 on this system is installed in /usr/local; its exported CMake config is
# broken, so locate it through pkg-config instead (same as msmoney).
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)
add_library(SDL3::SDL3 ALIAS PkgConfig::SDL3)

add_executable(tsbdemo
    src/main.cpp
    src/demo.cpp
    src/player669.cpp
)
target_link_libraries(tsbdemo PRIVATE SDL3::SDL3)

# unit tests for the module player; no SDL or audio device involved
enable_testing()
add_executable(player_tests tests/player_tests.cpp src/player669.cpp)
target_include_directories(player_tests PRIVATE src)
add_test(NAME player_tests COMMAND player_tests)
