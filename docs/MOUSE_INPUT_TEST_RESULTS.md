# Mouse-input test results

V14's absolute-pointer center-recenter path was rejected after live logs showed much faster spinning. It relied on a pointer warp that the compositor did not perform reliably, and mock tests incorrectly assumed the warp succeeded. V15 uses the SDL relative-event accumulator, checks capture activation, and returns no camera input if SDL refuses the relative lock. Unit tests cover all eight directions, one-time reads, capture transitions and fail-closed rejection.

Headless Release and ASan/UBSan suites pass. This container lacks SDL2 development headers and a physical WSLg session, so the graphical target and live hardware path remain unverified. The on-device probe and ten-second trace are still required before claiming physical confirmation.
