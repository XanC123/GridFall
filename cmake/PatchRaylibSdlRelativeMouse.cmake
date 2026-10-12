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


# The forced warp fallback produced window-sized deltas in the WSLg/XWayland
# trace. Keep SDL's native relative input enabled by default; retain the warp
# mode only as an explicit diagnostic override for comparison.
# GRIDFALL_SDL_RELATIVE_MOUSE_WARP=1 opts into warp fallback; =0 opts into raw.
set(_gridfall_disable_cursor_before [=[void DisableCursor(void)
{
    SDL_SetRelativeMouseMode(SDL_TRUE);]=])
set(_gridfall_disable_cursor_after [=[void DisableCursor(void)
{
    const char *videoDriver = SDL_GetCurrentVideoDriver();
    const char *warpFallbackOverride = SDL_getenv("GRIDFALL_SDL_RELATIVE_MOUSE_WARP");
    const bool useWarpFallback = (warpFallbackOverride != NULL)
        && (SDL_strcmp(warpFallbackOverride, "1") == 0);
    const bool isX11Driver = (videoDriver != NULL)
        && (SDL_strcmp(videoDriver, "x11") == 0);

    if (warpFallbackOverride != NULL)
    {
        SDL_SetHintWithPriority(SDL_HINT_MOUSE_RELATIVE_MODE_WARP,
                                useWarpFallback ? "1" : "0", SDL_HINT_OVERRIDE);
    }
    else if (isX11Driver)
    {
        // Override inherited SDL environment settings on X11. Raw relative input
        // is the known-good starting point; warp mode remains opt-in for diagnosis.
        SDL_SetHintWithPriority(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "0", SDL_HINT_OVERRIDE);
    }

    TRACELOG(LOG_INFO, "GRIDFALL mouse input: SDL driver=%s relative-input=%s",
             (videoDriver != NULL) ? videoDriver : "unknown",
             useWarpFallback ? "warp-fallback" : "native-raw");
    SDL_SetRelativeMouseMode(SDL_TRUE);]=])

set(_gridfall_disable_cursor_previous [=[void DisableCursor(void)
{
    const char *videoDriver = SDL_GetCurrentVideoDriver();
    const char *warpFallbackOverride = SDL_getenv("GRIDFALL_SDL_RELATIVE_MOUSE_WARP");
    const bool useWarpFallback = (warpFallbackOverride != NULL)
        ? (SDL_strcmp(warpFallbackOverride, "1") == 0)
        : ((videoDriver != NULL) && (SDL_strcmp(videoDriver, "x11") == 0));

    if (warpFallbackOverride != NULL)
    {
        SDL_SetHintWithPriority(SDL_HINT_MOUSE_RELATIVE_MODE_WARP,
                                useWarpFallback ? "1" : "0", SDL_HINT_OVERRIDE);
    }
    else if (useWarpFallback)
    {
        SDL_SetHintWithPriority(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "1", SDL_HINT_OVERRIDE);
    }

    TRACELOG(LOG_INFO, "GRIDFALL mouse input: SDL driver=%s warp-fallback=%s",
             (videoDriver != NULL) ? videoDriver : "unknown",
             useWarpFallback ? "enabled" : "disabled");
    SDL_SetRelativeMouseMode(SDL_TRUE);]=])

string(FIND "${_gridfall_raylib_sdl_source}" "${_gridfall_disable_cursor_before}" _gridfall_disable_cursor_old_index)
string(FIND "${_gridfall_raylib_sdl_source}" "${_gridfall_disable_cursor_after}" _gridfall_disable_cursor_new_index)
string(FIND "${_gridfall_raylib_sdl_source}" "${_gridfall_disable_cursor_previous}" _gridfall_disable_cursor_previous_index)
if (_gridfall_disable_cursor_old_index GREATER_EQUAL 0)
    string(REPLACE "${_gridfall_disable_cursor_before}" "${_gridfall_disable_cursor_after}" _gridfall_raylib_sdl_source "${_gridfall_raylib_sdl_source}")
    set(_gridfall_raylib_source_changed TRUE)
elseif (_gridfall_disable_cursor_previous_index GREATER_EQUAL 0)
    # Upgrade the earlier Gridfall patch, which forced warp mode on X11.
    string(REPLACE "${_gridfall_disable_cursor_previous}" "${_gridfall_disable_cursor_after}" _gridfall_raylib_sdl_source "${_gridfall_raylib_sdl_source}")
    set(_gridfall_raylib_source_changed TRUE)
elseif (_gridfall_disable_cursor_new_index LESS 0)
    message(FATAL_ERROR "Could not locate Raylib SDL DisableCursor() for the WSLg relative-mouse fix. Review cmake/PatchRaylibSdlRelativeMouse.cmake.")
endif()

if (_gridfall_raylib_source_changed)
    file(WRITE "${_gridfall_raylib_sdl_path}" "${_gridfall_raylib_sdl_source}")
endif()

# Raylib's bundled jar_mod reader ignores fread's result, triggering
# -Wunused-result on toolchains that annotate stdio reads. Check the read and
# release partial state if the file changed or could not be read completely.
set(_gridfall_jar_mod_path "${raylib_SOURCE_DIR}/src/external/jar_mod.h")
if (NOT EXISTS "${_gridfall_jar_mod_path}")
    message(FATAL_ERROR "Gridfall expected Raylib's bundled src/external/jar_mod.h")
endif()
file(READ "${_gridfall_jar_mod_path}" _gridfall_jar_mod_source)
set(_gridfall_fread_before [=[            fread(modctx->modfile, fsize, 1, f);]=])
set(_gridfall_fread_after [=[            size_t bytesRead = fread(modctx->modfile, fsize, 1, f);
            if (bytesRead != 1)
            {
                JARMOD_FREE(modctx->modfile);
                modctx->modfile = 0;
                modctx->modfilesize = 0;
                fsize = 0;
            }]=])
string(FIND "${_gridfall_jar_mod_source}" "${_gridfall_fread_before}" _gridfall_fread_old_index)
string(FIND "${_gridfall_jar_mod_source}" "${_gridfall_fread_after}" _gridfall_fread_new_index)
set(_gridfall_jar_mod_changed FALSE)
if (_gridfall_fread_old_index GREATER_EQUAL 0)
    string(REPLACE "${_gridfall_fread_before}" "${_gridfall_fread_after}" _gridfall_jar_mod_source "${_gridfall_jar_mod_source}")
    set(_gridfall_jar_mod_changed TRUE)
elseif (_gridfall_fread_new_index LESS 0)
    message(FATAL_ERROR "Could not locate Raylib jar_mod fread call to check. Raylib source changed; review the dependency patch.")
endif()
set(_gridfall_jar_mod_load_before [=[            if(!jar_mod_load(modctx, (void *)modctx->modfile, fsize)) fsize = 0;]=])
set(_gridfall_jar_mod_load_after [=[            if (fsize != 0 && !jar_mod_load(modctx, (void *)modctx->modfile, fsize)) fsize = 0;]=])
string(FIND "${_gridfall_jar_mod_source}" "${_gridfall_jar_mod_load_before}" _gridfall_jar_load_old_index)
string(FIND "${_gridfall_jar_mod_source}" "${_gridfall_jar_mod_load_after}" _gridfall_jar_load_new_index)
if (_gridfall_jar_load_old_index GREATER_EQUAL 0)
    string(REPLACE "${_gridfall_jar_mod_load_before}" "${_gridfall_jar_mod_load_after}" _gridfall_jar_mod_source "${_gridfall_jar_mod_source}")
    set(_gridfall_jar_mod_changed TRUE)
elseif (_gridfall_jar_load_new_index LESS 0)
    message(FATAL_ERROR "Could not locate Raylib jar_mod load call to guard after a failed read.")
endif()
if (_gridfall_jar_mod_changed)
    file(WRITE "${_gridfall_jar_mod_path}" "${_gridfall_jar_mod_source}")
endif()

if (_gridfall_raylib_source_changed)
    message(STATUS "Gridfall patched Raylib SDL mouse input: frame reset, signed accumulation, native-relative default")
else()
    message(STATUS "Gridfall Raylib SDL mouse-delta patch already present")
endif()
message(STATUS "Gridfall checked Raylib jar_mod fread result")
