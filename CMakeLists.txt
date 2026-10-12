cmake_minimum_required(VERSION 3.20)
project(Gridfall VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(GRIDFALL_BUILD_GAME "Build the Raylib graphical game" ON)
option(GRIDFALL_FETCH_RAYLIB "Fetch and build the selected Raylib backend" ON)
option(GRIDFALL_BUILD_TESTS "Build headless gameplay/domain tests" ON)
option(GRIDFALL_BUILD_BENCHMARKS "Build headless performance benchmark/report tool" ON)
option(GRIDFALL_BUILD_MOUSE_PROBE "Build an interactive SDL relative-mouse diagnostic window" ON)
option(GRIDFALL_FORCE_FETCH_RAYLIB "Legacy compatibility flag; Gridfall now fetches SDL-backed Raylib for WSLg" ON)
option(GRIDFALL_USE_SDL_RAYLIB "Use Raylib 6.0 SDL2 backend with native relative mouse input" ON)

set(GRIDFALL_DOMAIN_SOURCES
    src/world/Level.cpp
    src/entities/Inventory.cpp
    src/systems/ProgressionSystem.cpp
    src/systems/VfxSystem.cpp
    src/systems/CombatSystem.cpp
    src/systems/EnemySystem.cpp
    src/systems/SaveSystem.cpp
)

if (GRIDFALL_BUILD_GAME)
    if (NOT GRIDFALL_USE_SDL_RAYLIB)
        message(FATAL_ERROR
            "This WSLg mouse-fix build requires GRIDFALL_USE_SDL_RAYLIB=ON. "
            "The graphical target expects the selected SDL2-backed Raylib build.")
    endif()
    if (NOT GRIDFALL_FETCH_RAYLIB)
        message(FATAL_ERROR
            "GRIDFALL_FETCH_RAYLIB must be ON so this build can compile Raylib with PLATFORM=SDL; "
            "the system Raylib installation uses the GLFW backend.")
    endif()

    # Build a private Raylib 6.0 SDL2 copy and prefer native Wayland relative events on WSLg.
    find_package(SDL2 CONFIG REQUIRED)
    set(PLATFORM SDL CACHE STRING "Raylib platform backend for Gridfall" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "Disable Raylib examples" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "Disable Raylib games" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static Raylib" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "Disable Raylib's own tests" FORCE)
    include(FetchContent)
    FetchContent_Declare(
        raylib
        URL https://github.com/raysan5/raylib/archive/refs/tags/6.0.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(raylib)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/PatchRaylibSdlRelativeMouse.cmake")

    if (NOT TARGET raylib)
        message(FATAL_ERROR "Raylib was not built. Check SDL2 development files and network access.")
    endif()
    message(STATUS "Gridfall mouse input: SDL relative-motion accumulator; native Wayland preferred on WSLg (Raylib 6.0 SDL2)")

    add_executable(gridfall
        src/main.cpp
        src/core/Game.cpp
        src/core/MouseInput.cpp
        src/core/SdlInputPlatform.cpp
        src/render/Renderer.cpp
        ${GRIDFALL_DOMAIN_SOURCES}
    )
    target_include_directories(gridfall PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_compile_definitions(gridfall PRIVATE GRIDFALL_RAYLIB_SDL_BACKEND=1)
    target_link_libraries(gridfall PRIVATE raylib)
    if (UNIX AND NOT APPLE)
        target_link_libraries(gridfall PRIVATE m)
    endif()
    if (MSVC)
        target_compile_options(gridfall PRIVATE /W4 /permissive-)
    else()
        target_compile_options(gridfall PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Werror)
    endif()

    if (GRIDFALL_BUILD_MOUSE_PROBE)
        add_executable(gridfall_mouse_probe
            tools/mouse-input-probe.cpp
            src/core/MouseInput.cpp
            src/core/SdlInputPlatform.cpp
        )
        target_include_directories(gridfall_mouse_probe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
        target_compile_definitions(gridfall_mouse_probe PRIVATE GRIDFALL_RAYLIB_SDL_BACKEND=1)
        target_link_libraries(gridfall_mouse_probe PRIVATE raylib)
        if (UNIX AND NOT APPLE)
            target_link_libraries(gridfall_mouse_probe PRIVATE m)
        endif()
        if (MSVC)
            target_compile_options(gridfall_mouse_probe PRIVATE /W4 /permissive-)
        else()
            target_compile_options(gridfall_mouse_probe PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Werror)
        endif()
    endif()
endif()

if (GRIDFALL_BUILD_TESTS)
    enable_testing()
    add_executable(gridfall_core_tests
        tests/core_tests.cpp
        ${GRIDFALL_DOMAIN_SOURCES}
    )
    target_include_directories(gridfall_core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    if (MSVC)
        target_compile_options(gridfall_core_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(gridfall_core_tests PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Werror)
    endif()
    add_test(NAME gridfall_core_tests COMMAND gridfall_core_tests)

    add_executable(gridfall_mouse_input_tests tests/mouse_input_tests.cpp)
    target_include_directories(gridfall_mouse_input_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    if (MSVC)
        target_compile_options(gridfall_mouse_input_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(gridfall_mouse_input_tests PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Werror)
        target_link_libraries(gridfall_mouse_input_tests PRIVATE m)
    endif()
    add_test(NAME gridfall_mouse_input_tests COMMAND gridfall_mouse_input_tests)

    # Build and execute production MouseInput.cpp against a deterministic SDL-relative API mock.
    # This tests signed deltas, eight directions, capture lifecycle and failed capture.
    add_executable(gridfall_mouse_relative_input_tests
        tests/mouse_relative_input_tests.cpp
        src/core/MouseInput.cpp
    )
    target_include_directories(gridfall_mouse_relative_input_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/mocks
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_compile_definitions(gridfall_mouse_relative_input_tests PRIVATE GRIDFALL_RAYLIB_SDL_BACKEND=1)
    if (MSVC)
        target_compile_options(gridfall_mouse_relative_input_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(gridfall_mouse_relative_input_tests PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Werror)
        target_link_libraries(gridfall_mouse_relative_input_tests PRIVATE m)
    endif()
    add_test(NAME gridfall_mouse_relative_input_tests COMMAND gridfall_mouse_relative_input_tests)
endif()

if (GRIDFALL_BUILD_BENCHMARKS)
    add_executable(gridfall_performance_benchmark
        tests/performance_benchmark.cpp
        ${GRIDFALL_DOMAIN_SOURCES}
    )
    target_include_directories(gridfall_performance_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    add_executable(gridfall_frame_report tools/analyze-frame-csv.cpp)
    if (MSVC)
        target_compile_options(gridfall_performance_benchmark PRIVATE /W4 /permissive-)
        target_compile_options(gridfall_frame_report PRIVATE /W4 /permissive-)
    else()
        target_compile_options(gridfall_performance_benchmark PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Werror)
        target_compile_options(gridfall_frame_report PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Werror)
    endif()
endif()
