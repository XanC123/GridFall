# Mouse-input test results

## Automated checks

The source now tests a dedicated SDL relative-motion accumulator rather than relying on GetMouseDelta() cursor-position subtraction for gameplay look. The adapter test asserts that GetMouseDelta() is never called, signed X/Y deltas are returned independently, the transition sample is discarded once, later samples are retained, and capture/release clears stale data. The camera test explicitly checks that horizontal input turns toward or away from camera-right without changing pitch.

## Physical-device status

This code environment cannot operate the user's physical WSLg mouse or observe the host compositor. Physical mouse behavior must be verified using ./tools/run-mouse-diagnostic.sh and the automatic 10-second gameplay trace after rebuilding. No claim of a verified live-device fix is made until that test passes.
