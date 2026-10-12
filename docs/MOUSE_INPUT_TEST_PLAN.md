# Mouse Input Acceptance Test Plan — SDL Backend

## Why the backend changed

The affected WSLg session showed two unusable paths: cursor-disabled GLFW polling produced huge, biased virtual-cursor positions, and a second XInput2 subscription received no raw events. The current project builds Raylib 6.0 with its SDL platform backend and enables SDL relative mouse mode through Raylib's `DisableCursor()`. Gridfall patches the fetched SDL backend so locked motion uses accumulated signed event `xrel/yrel` values, resetting both current and previous delta state each frame. See `docs/WSLG_MOUSE_DIAGNOSIS.md` for details.

## Automated checks (CTest)

1. `gridfall_mouse_input_tests`
   - Right and left deltas produce opposite yaw changes.
   - Up and down deltas produce opposite pitch changes.
   - Each diagonal changes both axes with correct signs.
   - Stationary frames do not drift.
   - Wrapping yaw through +/-pi does not cause a discontinuity in facing.
   - Equal accumulated displacements yield equal angular changes regardless of how they are split across frames.
   - A full 360-degree input turn wraps without discontinuity; large movement is not silently clipped.
   - Horizontal/vertical inversion is configurable through named settings.

2. `gridfall_mouse_relative_input_tests`
   - Compiles production `src/core/MouseInput.cpp` against deterministic mock Raylib APIs.
   - Verifies signed `GetMouseDelta()` values pass through exactly once and only when capture is active.
   - Verifies capture/release transitions and that no fabricated motion occurs while stationary.

These tests validate Gridfall's adapter and math, not real device events delivered by WSLg.

## Physical WSLg acceptance test

Install the dependency and build the app as in `README.md`:

```bash
sudo apt install -y libsdl2-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DGRIDFALL_FETCH_RAYLIB=ON \
  -DGRIDFALL_BUILD_TESTS=ON -DGRIDFALL_BUILD_MOUSE_PROBE=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Run the mouse probe:

```bash
./tools/run-mouse-diagnostic.sh
```

Press Space to capture, then physically test each direction:

| Motion | Required result |
|---|---|
| Right | positive `dx`; yaw turns one direction |
| Left | negative `dx`; yaw reverses |
| Up | negative `dy`; pitch changes, yaw stays still |
| Down | positive `dy`; pitch reverses, yaw stays still |
| Up-right | both axes respond |
| Up-left | both axes respond with opposite horizontal sign |
| Down-right | both axes respond with opposite vertical sign |
| Down-left | both axes respond with both opposite signs |
| Stop moving | next sample returns to zero; camera stops turning |
| Capture/release twice | no large first-frame jump |

For gameplay, run `./run-gridfall-wsl.sh` from a terminal and start New Game. The game automatically logs the first 10 active seconds, correlating raw deltas with yaw, pitch and facing. Check that a stationary mouse produces zero summed deltas and no yaw drift; right/left raw deltas must produce opposite yaw changes; up/down raw deltas must produce opposite pitch changes while yaw remains still. If a raw axis sign is correct but the corresponding camera change is not, adjust only the named inversion setting. Do not use the probe's 2D arrow as proof of camera behavior. Include turning while strafing and shooting in the live check.

## Release criterion

The software tests must pass, the SDL backend must be visible in the Raylib startup log, and all eight physical motions must work on the target WSLg machine before the issue is considered resolved.
