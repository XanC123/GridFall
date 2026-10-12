# Mouse-input test results

## Automated checks completed

- Release headless build: all three CTest targets passed.
- AddressSanitizer + UndefinedBehaviorSanitizer headless build: all three CTest targets passed.
- The CMake patch was run twice against an upstream-shaped Raylib SDL fixture. The first pass patched the source and checked the bundled fread result; the second pass completed idempotently.
- The patched SDL C fixture compiled with C99, -Wall, -Wextra and -Werror.
- The adapter test asserts that GetMouseDelta() is never called, signed X/Y deltas are returned independently, the transition sample is discarded once, later samples are retained, and capture/release clears stale data. The camera test explicitly checks that horizontal input turns toward or away from camera-right without changing pitch.

## Physical-device status

The full graphical Raylib target and the user's physical WSLg mouse cannot be exercised from this build environment. Physical mouse behavior must still be verified using ./tools/run-mouse-diagnostic.sh and the automatic 10-second gameplay trace after rebuilding. The automated tests validate the adapter, math and patch plumbing; they do not claim that XWayland/compositor event delivery has been verified live.
