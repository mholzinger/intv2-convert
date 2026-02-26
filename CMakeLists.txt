cmake_minimum_required(VERSION 3.16)
project(intv2convert LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(intv2convert
    src/main.cpp
    src/writer.cpp
    src/rom.cpp
    src/cfg.cpp
    src/lst.cpp
    src/batch.cpp
)

target_include_directories(intv2convert PRIVATE src)

if(MSVC)
    target_compile_options(intv2convert PRIVATE /W4 /wd4996)
else()
    target_compile_options(intv2convert PRIVATE -Wall -Wextra -Wpedantic)
endif()

# GCC < 9 and Clang < 9 require an explicit filesystem link flag
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.0")
    target_link_libraries(intv2convert PRIVATE stdc++fs)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.0")
    target_link_libraries(intv2convert PRIVATE c++fs)
endif()

install(TARGETS intv2convert RUNTIME DESTINATION bin)
