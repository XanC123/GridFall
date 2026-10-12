# Gridfall-local patch for Raylib's SDL relative-mouse bookkeeping.
# Upstream's relative mode originally overwrote multiple xrel/yrel events in one
# frame and could leave a stale absolute previousPosition when no motion event arrived.
# Patch the fetched dependency at configure time so every fresh build gets the fix.

if (NOT DEFINED raylib_SOURCE_DIR OR NOT EXISTS "${raylib_SOURCE_DIR}/src/platforms/rcore_desktop_sdl.c")
    message(FATAL_ERROR "Gridfall expected Raylib's SDL backend source at raylib_SOURCE_DIR/src/platforms/rcore_desktop_sdl.c")
endif()

set(_gridfall_raylib_sdl_path "${raylib_SOURCE_DIR}/src/platforms/rcore_desktop_sdl.c")
file(READ "${_gridfall_raylib_sdl_path}" _gridfall_raylib_sdl_source)
set(_gridfall_raylib_source_changed FALSE)

set(_gridfall_reset_before [=[    // Register previous mouse position
    if (CORE.Input.Mouse.cursorLocked) CORE.Input.Mouse.currentPosition = (Vector2){ 0.0f, 0.0f };
    else CORE.Input.Mouse.previousPosition = CORE.Input.Mouse.currentPosition;]=])
set(_gridfall_reset_after [=[    // Register previous mouse position
    // GRIDFALL PATCH: relative position is a per-frame delta, not a screen coordinate.
    // Reset both values so a missing SDL motion event cannot reuse stale absolute coordinates.
    if (CORE.Input.Mouse.cursorLocked)
    {
        CORE.Input.Mouse.currentPosition = (Vector2){ 0.0f, 0.0f };
        CORE.Input.Mouse.previousPosition = (Vector2){ 0.0f, 0.0f };
    }
    else CORE.Input.Mouse.previousPosition = CORE.Input.Mouse.currentPosition;]=])

string(FIND "${_gridfall_raylib_sdl_source}" "${_gridfall_reset_before}" _gridfall_reset_old_index)
string(FIND "${_gridfall_raylib_sdl_source}" "${_gridfall_reset_after}" _gridfall_reset_new_index)
if (_gridfall_reset_old_index GREATER_EQUAL 0)
    string(REPLACE "${_gridfall_reset_before}" "${_gridfall_reset_after}" _gridfall_raylib_sdl_source "${_gridfall_raylib_sdl_source}")
    set(_gridfall_raylib_source_changed TRUE)
elseif (_gridfall_reset_new_index LESS 0)
    message(FATAL_ERROR "Could not locate Raylib SDL mouse-position reset to patch. Raylib source changed; review cmake/PatchRaylibSdlRelativeMouse.cmake.")
endif()

set(_gridfall_motion_before [=[                    CORE.Input.Mouse.currentPosition.x = (float)event.motion.xrel;
                    CORE.Input.Mouse.currentPosition.y = (float)event.motion.yrel;
                    CORE.Input.Mouse.previousPosition = (Vector2){ 0.0f, 0.0f };]=])
set(_gridfall_motion_after [=[                    // GRIDFALL PATCH: accumulate every relative SDL motion event for this frame.
                    CORE.Input.Mouse.currentPosition.x += (float)event.motion.xrel;
                    CORE.Input.Mouse.currentPosition.y += (float)event.motion.yrel;]=])

string(FIND "${_gridfall_raylib_sdl_source}" "${_gridfall_motion_before}" _gridfall_motion_old_index)
string(FIND "${_gridfall_raylib_sdl_source}" "${_gridfall_motion_after}" _gridfall_motion_new_index)
if (_gridfall_motion_old_index GREATER_EQUAL 0)
    string(REPLACE "${_gridfall_motion_before}" "${_gridfall_motion_after}" _gridfall_raylib_sdl_source "${_gridfall_raylib_sdl_source}")
    set(_gridfall_raylib_source_changed TRUE)
elseif (_gridfall_motion_new_index LESS 0)
    message(FATAL_ERROR "Could not locate Raylib SDL relative-motion event handler to patch. Raylib source changed; review cmake/PatchRaylibSdlRelativeMouse.cmake.")
endif()

if (_gridfall_raylib_source_changed)
    file(WRITE "${_gridfall_raylib_sdl_path}" "${_gridfall_raylib_sdl_source}")
    message(STATUS "Gridfall patched Raylib SDL mouse deltas: per-frame reset + signed event accumulation")
else()
    message(STATUS "Gridfall Raylib SDL mouse-delta patch already present")
endif()
