# WSLg mouse-input stability diagnosis

V14's absolute-pointer recenter experiment made the camera spin faster. X samples repeatedly stayed negative, peaks reached hundreds of pixels per frame, and total X reached `-195323` over ten seconds. The implementation assumed `SetMousePosition()` moved the physical pointer to the center. Wayland does not guarantee that warp, so an unchanged absolute offset could be consumed repeatedly as new motion. Its mock tests incorrectly assumed warp success.

SDL has documented cases where relative-mode input on Linux/virtualized paths confuses absolute valuators and relative deltas: [SDL issue #1836](https://github.com/libsdl-org/SDL/issues/1836). Sensitivity tuning cannot fix source data that is already corrupt.

The input chain is SDL events → Raylib SDL handler → isolated `xrel/yrel` accumulator → `MouseInput::consumeDelta()` → `MouseLookController::update()` → player-facing/yaw/pitch → `Renderer::makeFirstPersonCamera()`. The camera applies the supplied deltas linearly, and the renderer builds a consistent forward vector. The huge turns originate before camera math.

The current code avoids absolute-pointer warping. Before `InitWindow`, it prefers SDL2's Wayland driver when available and `WAYLAND_DISPLAY` is set, unless `SDL_VIDEODRIVER` was explicitly selected. The patched Raylib handler accumulates all relative events since the previous poll. Wayland disables warp emulation; X11 defaults to SDL's warp fallback, controllable with `GRIDFALL_SDL_RELATIVE_MOUSE_WARP=0/1`. Capture success is checked, and failed capture produces zero camera input.

Yaw and pitch are preserved across sector transitions. A new run intentionally resets the camera, and Renderer does not add smoothing.

## Live acceptance checks

- Expect `[SDL_INPUT] WSLg Wayland available...` when the linked SDL build includes Wayland.
- Capture should log `SDL driver=wayland path=native-relative relative-mode=active`.
- Right/left change yaw in opposite directions; up/down change pitch in opposite directions; axes remain independent.
- A stationary pointer yields zero deltas; normal movement has no repeated screen-sized spikes.
- Sector transitions do not reset yaw or pitch.

Headless tests validate the adapter and camera math only. Real WSLg mouse delivery still requires testing on the target system.
