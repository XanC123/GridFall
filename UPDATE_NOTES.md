# Update Notes — WSLg Mouse Stability and Camera Continuity

## Diagnosis

The user's live trace showed samples up to 463 horizontal and 527 vertical pixels, mixed with zero samples, and a sudden yaw reset from about +99.5 degrees to zero when the accumulated X delta was only -10 pixels. This indicated two separate problems: unreliable SDL relative-mode deltas on the WSLg/XWayland path, and camera orientation being reset from loadLevel() during a sector transition. SDL has documented cases where absolute-valuator coordinates are mistaken for relative deltas on Linux/virtualized input paths: https://github.com/libsdl-org/SDL/issues/1836.

## Changes

- Stop enabling SDL relative mouse mode for gameplay. Use HideCursor() while leaving the pointer in absolute mode, compute signed displacement from the client-area center, and recenter only after a non-zero movement sample.
- Keep the title menu and skill-tree on the same ordinary absolute-pointer path.
- Reduce default horizontal and vertical sensitivity from 0.0035 to 0.0018 radians per pixel; do not add low-pass smoothing or frame-time scaling.
- Keep camera yaw/pitch across loadLevel() transitions and initialize player facing from the current camera.
- Remove the duplicate warm-up sample discard from the interactive mouse probe.
- Replace the old Raylib SDL relative-motion patch with only the strict-warning fread result check for Raylib's bundled module loader.
- Add adapter tests for center-relative signed axes, single-sample recentering, stationary zero, all eight directions, resizing, and capture lifecycle.

## Verification boundary

Automated tests can validate center-relative bookkeeping and camera math. They cannot verify a physical mouse under WSLg/XWayland. The live probe and gameplay trace must still be run on the target machine to confirm there are no platform-specific event spikes or recenter artifacts.
