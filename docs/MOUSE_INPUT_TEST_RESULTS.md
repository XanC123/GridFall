# Mouse Input Test Results — SDL Backend

## Automated results executed in the artifact build environment

- Release headless build: passed with `-Wall -Wextra -Wpedantic -Wshadow -Werror`.
- Latest Release CTest run after the mouse-control refactor: 3/3 passed (`gridfall_core_tests`, `gridfall_mouse_input_tests`, `gridfall_mouse_relative_input_tests`).
- ASan + UBSan CTest passed after the current controller and diagnostic changes (3/3 tests).
- The obsolete XInput2 raw-valuator test block was removed from `core_tests.cpp`; it referenced a decoder that no longer exists in the SDL input architecture.
- The production `MouseInput.cpp` was linked against a deterministic mock Raylib surface. It verifies signed deltas, one sample per consume, ignored deltas while uncaptured, and cursor capture/release lifecycle.
- Mouse-look math tests cover left/right, up/down, all four diagonals, configurable axis inversion, stationary no-drift, a complete 360-degree turn, yaw wrap continuity, no arbitrary per-frame delta clipping, pitch limits, and sample-rate independence.
- The CMake Raylib patch was exercised against an upstream-shaped source fixture, including a second configure-time run to verify idempotence.

## Not yet verified on the target machine

The current artifact environment has no interactive WSLg desktop or access to the user's physical mouse. Outbound GitHub access is not available inside the build container, so this environment could not build and launch the full fetched Raylib SDL graphical target. The automated tests validate Gridfall's adapter and camera math, not SDL event delivery on the actual WSLg host.

The updated graphical Raylib/SDL target and WSLg physical mouse still need live verification on the user's WSL setup. After reconfiguring and rebuilding so the CMake patch is applied, run `./run-gridfall-wsl.sh` in the terminal and start a New Game. Review the automatic 10-second raw/camera trace and confirm the first-person camera follows all axes without drift or continuous rotation.
