# WSLg relative-mouse diagnosis

## Current input path

GridFall builds its own Raylib 6.0 SDL2 backend because the earlier GLFW cursor-disabled path on this WSLg/XWayland session produced large, biased virtual-cursor deltas. A separate direct XInput2 raw-motion subscription also received no events.

The game now consumes a dedicated signed relative-motion accumulator patched into Raylib's SDL event loop. While the cursor is captured, each SDL mouse-motion event contributes xrel to horizontal input and yrel to vertical input. GridfallConsumeRelativeMouseDelta() returns and clears that accumulator directly. The game no longer reconstructs look input by subtracting Raylib's shared absolute cursor positions or applying Raylib cursor scale to those position differences. The absolute cursor path remains available to the title menu and skill-tree UI.

The backend defaults to SDL's native relative mode on X11, with GRIDFALL_SDL_RELATIVE_MOUSE_WARP=1 available only as an explicit comparison override. Capture transitions clear queued deltas, and the adapter consumes a single warm-up sample so a stale transition sample cannot rotate the camera.

## Axis and camera contract

- Positive SDL xrel means physical movement to the right and must change yaw in one consistent direction.
- Negative SDL xrel must reverse yaw; horizontal input must not change pitch.
- Negative SDL yrel means physical movement up and changes pitch; positive yrel reverses it. Vertical input must not change yaw.
- A diagonal motion changes yaw and pitch independently; the two raw components are never swapped.
- The default horizontal sign is retained because the camera's yaw/facing/right-vector convention and automated direction tests agree. Do not change inversion defaults merely to compensate for corrupted or oversized samples.

The automatic first-10-second gameplay trace reports signed raw deltas next to yaw/pitch/facing changes. This can determine whether a remaining direction defect starts in the SDL event stream or after the adapter.

## Verification boundary

Headless tests prove the adapter returns isolated signed components exactly once and that the camera's math maps them to independent yaw/pitch changes. They cannot simulate WSLg's physical mouse, XWayland, or compositor event delivery. The final physical-device acceptance test must still use the interactive probe and gameplay trace on the target WSLg desktop.
