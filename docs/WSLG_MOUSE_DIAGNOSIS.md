# WSLg mouse-look diagnosis

## Observations from the affected machine

The title menu's absolute cursor-position path worked, but the gameplay path used the system Raylib build's GLFW cursor-disabled delta behavior. Logs showed virtual cursor coordinates increasing into the millions with huge, predominantly positive deltas. A separate XInput2 connection successfully selected raw motion but received no events (`raw=0`) from the WSLg/XWayland session. These signals explain the constant yaw drift; changing camera sensitivity could not repair invalid or missing motion input.

## Current fix direction

### Relative-mode fallback for WSLg/XWayland

The previous build forced SDL's warp-based relative fallback on the `x11`
driver. The live trace showed deltas approaching half the window width/height,
consistent with cursor-position-sized jumps, so that fallback is no longer the
default. The patched Raylib SDL backend now requests native/raw relative input
by default on X11 and leaves other drivers on their native path. It reports the
selected mode at runtime.

Set `GRIDFALL_SDL_RELATIVE_MOUSE_WARP=1` to opt into the legacy warp fallback
for comparison, or `GRIDFALL_SDL_RELATIVE_MOUSE_WARP=0` to explicitly request
native/raw input. The default is native/raw. This is a platform backend choice,
not a sensitivity multiplier or arbitrary per-frame clamp.

The input adapter discards one stale sample at the capture/re-capture boundary,
then retains the first subsequent relative sample.



The current project fetches Raylib 6.0 and compiles it with `PLATFORM=SDL`, using SDL2 relative mouse mode. Raylib's SDL backend calls `SDL_SetRelativeMouseMode(SDL_TRUE)` when `DisableCursor()` is called, and in locked mode it assigns mouse motion from the SDL event's signed `xrel/yrel`, rather than deriving it from the virtual cursor's position. This follows the workaround noted by the Raylib maintainer for the reported Linux `DisableCursor()` delta issue.

The project removes custom X11/XInput2 connections and GLFW virtual-cursor polling from its gameplay path. The `MouseInput` wrapper deliberately uses only Raylib's `DisableCursor`, `EnableCursor`, and `GetMouseDelta` API so platform details stay inside the selected Raylib SDL build.

## Verification boundary

Headless tests (including a mocked production adapter test and camera-math tests) pass in both Release and ASan/UBSan builds. A live physical WSLg mouse has not been tested by the artifact builder. The user must build the SDL-backed Raylib target and run `./tools/run-mouse-diagnostic.sh` to confirm actual event delivery on the machine.


## Gridfall's Raylib SDL relative-motion patch

CMake applies `cmake/PatchRaylibSdlRelativeMouse.cmake` to the fetched Raylib 6.0 SDL backend. It makes two corrections: reset both current and previous mouse positions to zero at the beginning of every captured frame, and add each SDL `xrel/yrel` motion event instead of overwriting earlier events in the same frame. The first prevents a stale absolute cursor coordinate from becoming a false, huge relative delta when no motion event arrives; the second preserves the complete movement accumulated during a frame. The CMake patch fails loudly if the expected upstream code changes, rather than silently building an unpatched backend.

Mouse direction/sensitivity/pitch bounds now live together in `MouseLookSettings` in `include/gridfall/core/Controls.hpp`; per-frame mouse deltas are not clipped or multiplied by frame time. The defaults use conventional FPS axis signs; horizontal and vertical inversion are explicit options in the settings and are printed at launch. This avoids flipping axes based only on behavior observed while the input stream was corrupted.

When the game first captures the mouse in active gameplay, it automatically writes terminal reports for 10 seconds of active input. Every report aggregates approximately 0.25 seconds and includes the last raw delta, summed X/Y deltas, largest per-frame delta, yaw, pitch, and facing vector. A final line reports total movement and the largest observed raw delta. This is intentionally low-rate logging to avoid flooding the terminal or heavily disturbing frame pacing.
