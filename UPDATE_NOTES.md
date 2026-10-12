# Update Notes — Raylib SDL2 Relative-Mouse Backend

## Why this version changes the platform dependency

The user's WSLg/XWayland session showed large, mostly-positive GLFW virtual cursor deltas and a custom XInput2 root subscription that received zero raw-motion events. Repeated sensitivity tuning could not fix the corrupted/missing signal.

Raylib's maintainer documented a Linux `DisableCursor()` / `GetMouseDelta()` issue and listed using the SDL platform backend as a workaround: https://github.com/raysan5/raylib/issues/3875. Raylib 6.0's SDL platform implementation enters relative mouse mode when `DisableCursor()` is called and uses the signed SDL mouse-motion relative fields while locked. This project therefore builds its own Raylib 6.0 copy with `PLATFORM=SDL` and explicitly selects SDL2. Official implementation: https://github.com/raysan5/raylib/blob/6.0/src/platforms/rcore_desktop_sdl.c

## Changes

- Patch Raylib's SDL relative-mouse backend during CMake configuration: clear both current and previous delta state before each captured frame, and accumulate all `xrel/yrel` events instead of retaining only the last event. The patch includes source-pattern checks and fails loudly if upstream changes the affected code.
- Centralize sensitivity, axis inversion and pitch limits in `MouseLookSettings`, widen the pitch range, increase the named sensitivity setting, and remove the arbitrary 180-pixel per-frame clamp and unused frame-time argument. Keep conventional axis signs by default; inversion is configurable and should only be changed if live raw-delta traces demonstrate a real sign reversal.
- Automatically log the first 10 active seconds of gameplay mouse input to the terminal at a low reporting rate, correlating signed deltas with yaw, pitch, and facing.
- Replace the hand-written GLFW/XInput2 pathways with a small adapter using Raylib's normal relative-mouse API, while requiring the SDL platform backend for the game build.
- Remove stale XInput2 raw-valuator expectations from the main domain tests.
- Add a production-adapter mock test and keep the 8-direction, wraparound, stationary, and frame-rate-independence math tests.
- Make `make test` build all test executables before invoking CTest, so it cannot accidentally run a test binary that has not been built.
- Update WSLg troubleshooting and acceptance documentation to describe the SDL path and explicitly distinguish automated tests from physical-device testing.

## Verified here

- Release headless compilation with strict warnings treated as errors.
- 3/3 CTest targets pass in Release.
- 3/3 CTest targets pass in AddressSanitizer + UndefinedBehaviorSanitizer build.
- The updated graphical Raylib/SDL target and WSLg physical mouse are **not verified** in this artifact environment. Headless tests validate controller math and adapter behavior, not physical input or rendering. Reconfigure/rebuild on the user's WSLg machine and use the automatic 10-second gameplay trace to confirm the fix against actual mouse motion.
