# Mouse-input acceptance test plan

## Automated tests

1. gridfall_mouse_input_tests checks right/left yaw opposition, up/down pitch opposition, all diagonals, independent axes, yaw wrap, stationary input, pitch bounds, configurable inversion, and frame-rate-independent math.
2. gridfall_mouse_relative_input_tests links production MouseInput.cpp to a deterministic Raylib mock. It verifies the absolute pointer is sampled relative to the center, every non-zero sample recenters once, stationary samples stay zero, capture hides/shows the pointer correctly, and the adapter never enables SDL relative mode.
3. Headless tests are not a substitute for physical WSLg testing.

## Physical WSLg test

Build and launch ./tools/run-mouse-diagnostic.sh. Press Space to capture. Physically move right, left, up, down, and all four diagonals. The displayed dx/dy should have the same signs as the motion, axes must stay independent, and the center-relative arrow must stop when the mouse stops. Release and recapture twice to check for transition jumps.

Then launch ./run-gridfall-wsl.sh, start a New Game, and turn while moving and shooting. Read the automatic [MOUSE_DIAG] trace. Peaks should reflect the actual center-relative physical displacement rather than desktop coordinates, and yaw/pitch should not reset during a sector change. The camera should not receive extra smoothing or time scaling.

## Release criterion

All headless tests pass, the adapter follows the absolute pointer path without SDL relative mode, and the physical WSLg test has no coordinate-sized spikes, stationary drift, or camera resets.
