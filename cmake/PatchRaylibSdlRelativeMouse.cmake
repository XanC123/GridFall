if (NOT DEFINED raylib_SOURCE_DIR OR NOT EXISTS "${raylib_SOURCE_DIR}/src/platforms/rcore_desktop_sdl.c")
    message(FATAL_ERROR "Gridfall expected Raylib SDL backend source.")
endif()
set(_path "${raylib_SOURCE_DIR}/src/platforms/rcore_desktop_sdl.c")
file(READ "${_path}" _src)
set(_changed FALSE)
string(FIND "${_src}" "Vector2 GridfallConsumeRelativeMouseDelta(void)" _consume)
string(FIND "${_src}" "void GridfallResetRelativeMouseDelta(void)" _reset)
string(FIND "${_src}" "int GridfallRelativeMouseModeActive(void)" _mode)
if (_consume LESS 0 AND _reset LESS 0)
 set(_anchor [=[#ifndef MAX_CLIPBOARD_BUFFER_LENGTH]=])
 set(_api [=[
/* GRIDFALL PATCH: dedicated signed relative-motion accumulator. */
static Vector2 gridfallRelativeMouseDelta = { 0.0f, 0.0f };
Vector2 GridfallConsumeRelativeMouseDelta(void) {
 Vector2 delta = gridfallRelativeMouseDelta;
 gridfallRelativeMouseDelta = (Vector2){ 0.0f, 0.0f };
 return delta;
}
void GridfallResetRelativeMouseDelta(void) {
 gridfallRelativeMouseDelta = (Vector2){ 0.0f, 0.0f };
}
int GridfallRelativeMouseModeActive(void) {
 return SDL_GetRelativeMouseMode() == SDL_TRUE;
}
#ifndef MAX_CLIPBOARD_BUFFER_LENGTH]=])
 string(FIND "${_src}" "${_anchor}" _ai)
 if (_ai LESS 0)
  message(FATAL_ERROR "Raylib SDL API insertion point not found.")
 endif()
 string(REPLACE "${_anchor}" "${_api}" _src "${_src}")
 set(_changed TRUE)
elseif (_consume LESS 0 OR _reset LESS 0)
 message(FATAL_ERROR "Incomplete Gridfall relative-mouse API.")
elseif (_mode LESS 0)
 set(_oldapi [=[void GridfallResetRelativeMouseDelta(void)
{
    gridfallRelativeMouseDelta = (Vector2){ 0.0f, 0.0f };
}

#ifndef MAX_CLIPBOARD_BUFFER_LENGTH]=])
 set(_newapi [=[void GridfallResetRelativeMouseDelta(void)
{
    gridfallRelativeMouseDelta = (Vector2){ 0.0f, 0.0f };
}

int GridfallRelativeMouseModeActive(void)
{
    return SDL_GetRelativeMouseMode() == SDL_TRUE;
}

#ifndef MAX_CLIPBOARD_BUFFER_LENGTH]=])
 string(FIND "${_src}" "${_oldapi}" _oi)
 if (_oi LESS 0)
  message(FATAL_ERROR "Could not upgrade existing relative-mouse API.")
 endif()
 string(REPLACE "${_oldapi}" "${_newapi}" _src "${_src}")
 set(_changed TRUE)
endif()

set(_reset_old [=[    // Register previous mouse position
    if (CORE.Input.Mouse.cursorLocked) CORE.Input.Mouse.currentPosition = (Vector2){ 0.0f, 0.0f };
    else CORE.Input.Mouse.previousPosition = CORE.Input.Mouse.currentPosition;]=])
set(_reset_new [=[    // Register previous mouse position
    // GRIDFALL PATCH: do not retain stale relative coordinates between frames.
    if (CORE.Input.Mouse.cursorLocked) {
        CORE.Input.Mouse.currentPosition = (Vector2){ 0.0f, 0.0f };
        CORE.Input.Mouse.previousPosition = (Vector2){ 0.0f, 0.0f };
    } else CORE.Input.Mouse.previousPosition = CORE.Input.Mouse.currentPosition;]=])
string(FIND "${_src}" "${_reset_old}" _ro)
string(FIND "${_src}" "${_reset_new}" _rn)
if (_ro GREATER_EQUAL 0)
 string(REPLACE "${_reset_old}" "${_reset_new}" _src "${_src}")
 set(_changed TRUE)
elseif (_rn LESS 0)
 string(FIND "${_src}" "CORE.Input.Mouse.currentPosition = (Vector2){ 0.0f, 0.0f };" _relative_current_zero)
 string(FIND "${_src}" "CORE.Input.Mouse.previousPosition = (Vector2){ 0.0f, 0.0f };" _relative_previous_zero)
 if (_relative_current_zero LESS 0 OR _relative_previous_zero LESS 0)
  message(FATAL_ERROR "Raylib relative mouse reset block was not found.")
 endif()
endif()

set(_motion_old [=[                    CORE.Input.Mouse.currentPosition.x = (float)event.motion.xrel;
                    CORE.Input.Mouse.currentPosition.y = (float)event.motion.yrel;
                    CORE.Input.Mouse.previousPosition = (Vector2){ 0.0f, 0.0f };]=])
set(_motion_new [=[                    // GRIDFALL PATCH: retain every event arriving between polls.
                    CORE.Input.Mouse.currentPosition.x += (float)event.motion.xrel;
                    CORE.Input.Mouse.currentPosition.y += (float)event.motion.yrel;
                    gridfallRelativeMouseDelta.x += (float)event.motion.xrel;
                    gridfallRelativeMouseDelta.y += (float)event.motion.yrel;]=])
string(FIND "${_src}" "${_motion_old}" _mo)
string(FIND "${_src}" "${_motion_new}" _mn)
if (_mo GREATER_EQUAL 0)
 string(REPLACE "${_motion_old}" "${_motion_new}" _src "${_src}")
 set(_changed TRUE)
elseif (_mn LESS 0)
 string(FIND "${_src}" "gridfallRelativeMouseDelta.x += (float)event.motion.xrel;" _relative_accumulator_found)
 if (_relative_accumulator_found LESS 0)
  message(FATAL_ERROR "Raylib relative-motion handler was not found.")
 endif()
endif()

# Find the definition, not the prototype; replace through cursorLocked=true.
string(FIND "${_src}" "void DisableCursor(void)
{" _ds)
set(_end [=[
    CORE.Input.Mouse.cursorLocked = true;
}]=])
string(FIND "${_src}" "${_end}" _de)
if (_ds LESS 0 OR _de LESS 0 OR _de LESS _ds)
 message(FATAL_ERROR "Raylib DisableCursor function was not found.")
endif()
string(LENGTH "${_end}" _elen)
math(EXPR _after "${_de} + ${_elen}")
string(SUBSTRING "${_src}" 0 ${_ds} _prefix)
string(SUBSTRING "${_src}" ${_after} -1 _suffix)
set(_fn [=[void DisableCursor(void)
{
 const char *driver = SDL_GetCurrentVideoDriver();
 const char *warpOverride = SDL_getenv("GRIDFALL_SDL_RELATIVE_MOUSE_WARP");
 const bool isX11 = (driver != NULL) && (SDL_strcmp(driver, "x11") == 0);
 const bool useWarp = (warpOverride != NULL) ? (SDL_strcmp(warpOverride, "1") == 0) : isX11;
 SDL_SetHintWithPriority(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, useWarp ? "1" : "0", SDL_HINT_OVERRIDE);
 const int modeResult = SDL_SetRelativeMouseMode(SDL_TRUE);
 const bool active = (modeResult == 0) && (SDL_GetRelativeMouseMode() == SDL_TRUE);
 TRACELOG(LOG_INFO, "GRIDFALL mouse input: SDL driver=%s path=%s relative-mode=%s",
          (driver != NULL) ? driver : "unknown", useWarp ? "warp-fallback" : "native-relative",
          active ? "active" : "FAILED");
 if (!active) TRACELOG(LOG_WARNING, "GRIDFALL could not activate SDL relative mode: %s", SDL_GetError());
 HideCursor();
 CORE.Input.Mouse.cursorLocked = true;
}]=])
set(_replaced "${_prefix}${_fn}${_suffix}")
if (NOT _replaced STREQUAL _src)
 set(_src "${_replaced}")
 set(_changed TRUE)
endif()
if (_changed)
 file(WRITE "${_path}" "${_src}")
 message(STATUS "Gridfall patched Raylib SDL relative motion and capture verification")
else()
 message(STATUS "Gridfall Raylib relative-input patch already present")
endif()

set(_jar "${raylib_SOURCE_DIR}/src/external/jar_mod.h")
if (NOT EXISTS "${_jar}")
 message(FATAL_ERROR "Raylib jar_mod header not found.")
endif()
file(READ "${_jar}" _jar_src)
set(_fread_old [=[            fread(modctx->modfile, fsize, 1, f);]=])
set(_fread_new [=[            size_t bytesRead = fread(modctx->modfile, fsize, 1, f);
            if (bytesRead != 1) {
                JARMOD_FREE(modctx->modfile);
                modctx->modfile = 0;
                modctx->modfilesize = 0;
                fsize = 0;
            }]=])
string(FIND "${_jar_src}" "${_fread_old}" _fo)
string(FIND "${_jar_src}" "${_fread_new}" _fnidx)
set(_jar_changed FALSE)
if (_fo GREATER_EQUAL 0)
 string(REPLACE "${_fread_old}" "${_fread_new}" _jar_src "${_jar_src}")
 set(_jar_changed TRUE)
elseif (_fnidx LESS 0)
 message(FATAL_ERROR "Raylib jar_mod fread call not found.")
endif()
set(_load_old [=[            if(!jar_mod_load(modctx, (void *)modctx->modfile, fsize)) fsize = 0;]=])
set(_load_new [=[            if (fsize != 0 && !jar_mod_load(modctx, (void *)modctx->modfile, fsize)) fsize = 0;]=])
string(FIND "${_jar_src}" "${_load_old}" _lo)
string(FIND "${_jar_src}" "${_load_new}" _ln)
if (_lo GREATER_EQUAL 0)
 string(REPLACE "${_load_old}" "${_load_new}" _jar_src "${_jar_src}")
 set(_jar_changed TRUE)
elseif (_ln LESS 0)
 message(FATAL_ERROR "Raylib jar_mod load call not found.")
endif()
if (_jar_changed)
 file(WRITE "${_jar}" "${_jar_src}")
endif()
