# Update Notes — Isolated SDL Relative Mouse Input

## Diagnosis and design

GridFall's WSLg/XWayland input path previously combined SDL relative motion with Raylib's shared current/previous cursor-position state. That API is also used for absolute UI pointer coordinates, and GetMouseDelta() derives its answer from position subtraction and applies Raylib mouse scaling. The project now exposes a private, Gridfall-specific accessor in the patched Raylib SDL source so gameplay reads the signed SDL relative-motion event fields directly.

## Changes

- Keep Raylib's existing mouse-position bookkeeping intact for UI and any other Raylib consumers.
- Add a dedicated gridfallRelativeMouseDelta accumulator. Each captured SDL_MOUSEMOTION event adds xrel only to X and yrel only to Y.
- Add GridfallConsumeRelativeMouseDelta(), which returns and clears the accumulated signed pair, and GridfallResetRelativeMouseDelta(), used during cursor-capture transitions.
- Update MouseInput to consume that accumulator, ignore non-finite components, clear pending motion before changing capture modes, and discard one capture-warmup sample while preserving the next sample.
- Keep the calibrated radians-per-pixel values, default axis signs, pitch bounds, fixed 60 Hz simulation, render-rate mouse look, and rendering/UI unchanged. Do not multiply relative deltas by frame time, smooth them a second time, or hide anomalies with arbitrary clipping.
- Add tests that assert the production adapter does not call GetMouseDelta(), X/Y never cross, capture lifecycle clears stale values, and positive/negative horizontal motion turns toward/away from the camera-right vector.
- Preserve the Raylib SDL dependency patch's explicit source-pattern checks and the checked fread result guard for strict-warning builds.

## Verification boundary

Automated tests validate input adapter plumbing and camera math. They cannot verify a physical mouse under WSLg/XWayland. Run the interactive probe and review the gameplay trace after rebuilding on the target machine before considering the live input issue fully closed.
