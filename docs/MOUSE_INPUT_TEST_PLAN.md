# Mouse-input acceptance test plan

## Automated tests

1. gridfall_mouse_input_tests checks right/left yaw opposition, up/down pitch opposition, diagonals, independent axes, consistent turning toward camera-right, yaw wrap, stationary input, pitch bounds, inversion options, and frame-rate-independent math.
2. gridfall_mouse_relative_input_tests links the production MouseInput.cpp to mocked Raylib/SDL-accumulator functions. It verifies Gridfall reads GridfallConsumeRelativeMouseDelta() rather than GetMouseDelta(), preserves signed X/Y values, never swaps the axes, consumes each sample once, ignores input while uncaptured, and clears capture-transition state.
3. The CMake patch must be idempotent and must fail during configure if any expected Raylib 6.0 source pattern changes.

## Physical WSLg test

Reconfigure and rebuild, then run:

    ./tools/run-mouse-diagnostic.sh

Press Space to capture and move the physical mouse deliberately right, left, up, down, then along all four diagonals. Check that dx changes only yaw, dy changes only pitch, opposite directions reverse the corresponding camera change, and a stationary mouse produces zero. Then launch ./run-gridfall-wsl.sh, start a New Game and inspect the automatic [MOUSE_DIAG] lines in the terminal while turning, strafing and shooting.

A stable sign reversal can be corrected through MouseLookSettings; intermittent sign changes or cross-axis movement should be diagnosed in the raw dx/dy trace, not masked by arbitrary clipping or extra smoothing. Do not change the UI or frame pacing during this input test.

## Release criterion

All automated tests pass, the SDL backend reports the dedicated GridFall relative-input path, and all eight physical motion directions work in the live WSLg test. Headless tests alone do not prove physical-device correctness.
