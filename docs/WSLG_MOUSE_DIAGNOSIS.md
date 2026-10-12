# WSLg mouse-input stability diagnosis

## What the live trace showed

The gameplay trace reported per-sample peaks as large as 463 horizontal and 527 vertical pixels, mixed with zero samples. Those are not ordinary per-frame relative deltas for a mouse moving at game render rate. The trace also showed yaw snapping from about +99.5 degrees to 0 degrees while the accumulated X input was only -10 pixels; that reset matched mouseLook_.reset() being called inside loadLevel() during sector transitions.

SDL has documented cases where relative-mode input on Linux, Wayland/XWayland, or virtualized devices can mistake absolute valuator coordinates for relative deltas. See https://github.com/libsdl-org/SDL/issues/1836. Further sensitivity tuning alone cannot correct such samples.

## Current input path

Gridfall now deliberately avoids DisableCursor() during gameplay. It calls HideCursor() without enabling SDL relative mode, places the pointer at the client-area center, reads the ordinary absolute cursor position, converts its offset from center into a signed X/Y displacement, and calls SetMousePosition() to recenter only after movement. The menu and skill-tree use the same known-working absolute pointer path while the cursor is visible.

No low-pass smoothing or frame-time multiplication is applied; those would add lag to the physical mouse. Default radians-per-pixel sensitivity is reduced from 0.0035 to 0.0018 on both axes. The mouse adapter uses a single capture-transition warm-up sample, and the visual probe no longer discards a second sample independently.

Camera yaw/pitch are reset for a new run, but no longer reset inside loadLevel(). Camera facing is kept in sync when a new sector is loaded, so sector transitions should not unexpectedly turn the view back to zero.

## Acceptance checks

- Moving right/left changes yaw in opposite directions; up/down changes pitch in opposite directions.
- A horizontal sample never changes pitch, and a vertical sample never changes yaw.
- A stationary pointer produces zero; after any non-zero sample the pointer returns to the center before the next read.
- Moving between sectors does not reset yaw or pitch.
- The 10-second live trace has no desktop-coordinate-sized spikes and no unexpected yaw/pitch reset.

Headless tests validate adapter bookkeeping and camera math only. The final check still needs the physical WSLg mouse because event delivery and pointer warping are platform-dependent.
