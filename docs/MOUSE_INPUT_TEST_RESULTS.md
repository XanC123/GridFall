# Mouse-input test results

## Automated checks

The adapter tests now exercise the absolute-pointer center-recenter path rather than SDL relative-mode events. They verify signed X/Y deltas, all eight direction mappings, one-time sample consumption, stationary zero, capture lifecycle, and that DisableCursor() (which enables SDL relative mode) is never called. Camera math tests remain independent of the platform backend.

The headless Release suite and AddressSanitizer/UndefinedBehaviorSanitizer suite both pass all three CTest targets for this revision.

## Physical-device status

This build environment cannot operate the user's physical WSLg mouse or observe its compositor. The physical test still needs to verify the visual probe and the automatic 10-second gameplay trace after rebuilding. No claim of verified live-device delivery is made before that check.
