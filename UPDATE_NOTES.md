# Update Notes — WSLg Mouse Input V15

V14's hidden absolute pointer + center warp made the problem worse: up to 792 X and 322 Y pixels per frame, X stuck negative, and total X delta `-195323`. The compositor did not reliably warp the physical pointer, so absolute offsets were repeatedly interpreted as new movement. The mock tests incorrectly assumed warp success.

The full path audit traces SDL → Raylib → `MouseInput` → `MouseLookController` → `player_.facing` → `Renderer::makeFirstPersonCamera`. Camera math consistently applies the supplied deltas; the giant turns originate before the controller. The sector-transition reset remains removed from `loadLevel()`.

- Return to signed SDL relative events; remove absolute pointer recentering from gameplay.
- Prefer native SDL2 Wayland before `InitWindow` when Wayland is available unless `SDL_VIDEODRIVER` was explicitly set.
- Accumulate all Raylib SDL relative events per poll instead of overwriting multiple events within one frame.
- Disable SDL warp emulation on Wayland; default to the warp fallback on X11, with `GRIDFALL_SDL_RELATIVE_MOUSE_WARP=0/1` diagnostic overrides.
- Report the active SDL driver and whether relative mode activated. Fail closed if capture is rejected.
- Track capture request separately from actual capture in game/probe; remove stale claims that mouse-look is smoothed.

Release/sanitizer tests pass, but this environment has no SDL2 development headers or physical WSLg session. Real Wayland relative motion and the live ten-second trace still need verification on the target machine.
