cmake_minimum_required(VERSION 3.16)
project(intv2convert LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

option(BUILD_GUI "Build with native GUI (downloads ~20 MB of sources on first build)" OFF)

if(BUILD_GUI)
    find_package(OpenGL REQUIRED)

    # GLFW — window + OpenGL context
    FetchContent_Declare(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.4
        GIT_SHALLOW    TRUE)
    set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)

    # Dear ImGui — immediate-mode GUI
    FetchContent_Declare(imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        v1.91.6
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(imgui)

    # nativefiledialog-extended — native OS file/folder pickers
    FetchContent_Declare(nfd
        GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended.git
        GIT_TAG        v1.2.1
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(nfd)

    add_executable(intv2convert WIN32
        src/gui_main.cpp
        src/writer.cpp
        src/rom.cpp
        src/cfg.cpp
        src/lst.cpp
        src/batch.cpp
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(intv2convert PRIVATE
        src
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
    target_link_libraries(intv2convert PRIVATE glfw nfd OpenGL::GL)

    if(MSVC)
        target_compile_options(intv2convert PRIVATE /W4 /wd4996)
        # WIN32 sets /SUBSYSTEM:WINDOWS; change entry point so we can use main()
        target_link_options(intv2convert PRIVATE /ENTRY:mainCRTStartup)
    else()
        target_compile_options(intv2convert PRIVATE -Wall -Wextra)
        # Apple deprecated OpenGL in macOS 10.14 but it still works fine
        if(APPLE)
            target_compile_definitions(intv2convert PRIVATE GL_SILENCE_DEPRECATION)
        endif()
    endif()

    # GCC < 9 / Clang < 9 need an explicit filesystem link flag
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.0")
        target_link_libraries(intv2convert PRIVATE stdc++fs)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.0")
        target_link_libraries(intv2convert PRIVATE c++fs)
    endif()

else()
    # CLI-only build (no GUI dependencies)
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

endif()

install(TARGETS intv2convert RUNTIME DESTINATION bin)
