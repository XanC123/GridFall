# Mouse-input acceptance test plan

## Automated tests

1. `gridfall_mouse_input_tests` checks opposite signs for each axis, diagonals, independent axes, yaw wrapping, pitch bounds, configurable inversion and sample-rate-independent math.
2. `gridfall_mouse_relative_input_tests` mocks Raylib's SDL-relative API and checks signed deltas, all eight directions, one-time consumption, capture transitions and fail-closed behavior when relative capture is rejected.
3. Headless tests do not exercise the real SDL backend.

## Physical WSLg test

Launch `./tools/run-mouse-diagnostic.sh`; press Space and move right/left/up/down and diagonally. Deltas must be signed, match direction, and return to zero when stationary. Release and recapture twice.

Then launch `./run-gridfall-wsl.sh`, start a New Game, and inspect the ten-second `[MOUSE_DIAG]` trace. Movement should not produce repeated window-sized spikes or one-sign-only X values; yaw/pitch should not reset at sector transitions. When native Wayland is available, capture logs should report `driver=wayland` and `relative-mode=active`.

## Release criterion

All headless tests pass and the physical WSLg mouse confirms directional relative deltas without giant spikes, stationary drift or camera resets.
