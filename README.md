# GRIDFALL: BELOW THE LINE

A modular C++20 / raylib first-person grid-action prototype. The world is rendered as a three-dimensional X/Z battlefield with mouse-look, a center reticle and a camera-locked weapon viewmodel; the HUD is drawn directly over the game view. All visuals are procedural, so no external art or sound assets are required.

## Current gameplay

- Twenty sequential grid sectors with a distinct low-saturation palette per sector, locked extraction triggers, progressively denser cover, secret wall panels, a proper enclosed side-loot room, healing pickups, ammo pickups, spike traps, and poison pools.
- First-person movement constrained by grid collision, keyboard/game state handling, mouse-look and center-reticle firing, plus wall-aware enemy pursuit. Enemy density rises from 8 to 27; health, speed, aggression, detection range, contact damage, ranged projectile speed/damage/range and attack tempo scale through sector 20.
- Three weapon profiles with independent magazine/reserve counts: fast fire, piercing rounds, and high-damage slow/steady shots. Every sector guarantees light, piercing and heavy ammo in the main route plus additional supplies in the secret chamber; opening the ammo cache resupplies all three weapons.
- Enemies shoot back using line-of-sight ranged attacks and swept projectiles; bullets stop at walls and apply player damage-resistance upgrades correctly.
- Reload timing, a tossed magazine with a short arc/fade, muzzle bursts, projectile tracers, and hit/death particles.
- Health/regen consumables, poison and hazard damage, XP from kills/loot/secrets, ranks, skill points, and a nine-node, three-branch skill tree with prerequisites and live passive effects.
- Integrated in-game HUD overlay for health/status, ammo, weapons, inventory, rank/XP, upgrades, and objectives. No separate editor or side-panel UI.
- A fixed 60 Hz simulation accumulator with render interpolation for player, enemies, and projectiles. First-person mouse-look updates at render rate so it stays responsive while world simulation remains deterministic at the fixed tick rate.
- Default 144 FPS target for high-refresh displays, an in-game FPS/frame-time overlay, and an opt-in VSync mode for comparing frame pacing on WSLg.
- Reduced per-frame rendering work: the ground is a single plane with grid lines instead of hundreds of repeated floor cubes/wireframes.
- Buffered one-shot input so short key/mouse presses are not dropped when a 144 Hz render loop runs between 60 Hz simulation ticks.
- A small headless test executable covering all 20 unique sector layouts, extraction connectivity, spawn validity, secret interaction, skill-tree prerequisites/effects, inventory, firing, and reload behavior.
- A perspective first-person camera with yaw/pitch mouse-look and a camera-locked weapon viewmodel. The center reticle drives shots along the camera-facing direction. On WSLg, gameplay uses a Gridfall-patched Raylib 6.0 SDL2 backend with a dedicated signed `xrel/yrel` accumulator for camera look, separate from Raylib's absolute cursor-position state used by the UI. Native/raw relative input is the default; the previous forced warp fallback produced window-sized mouse deltas in the live trace. The first 10 seconds of active gameplay input are automatically reported to the terminal. Set `GRIDFALL_SDL_RELATIVE_MOUSE_WARP=1` to compare the legacy warp fallback, or `=0` to explicitly request native/raw input.
- Title menu draws over a live 3D operator preview, with mouse/keyboard selection for Continue/Load Save, New Game, and Exit. F5 saves during play/pause; sector-clear progression is autosaved. Save files are versioned, human-readable `gridfall.save` files in the working directory.
- First-person mouse look uses a WSLg-safe absolute-pointer fallback: hide the cursor without locking it, measure its offset from the client-area center, and recenter only when it moved. This avoids the corrupted SDL relative deltas seen in WSLg/XWayland. Mouse tuning is centralized in `MouseLookSettings` in `include/gridfall/core/Controls.hpp`; the default sensitivity is reduced, axis inversion is configurable, and pitch limits are named. No extra smoothing or `dt` multiplication is applied, so normal movement does not trail behind the physical mouse. The skill tree remains mouse-clickable, with hovered node highlighting.
- Soft system-font loading prefers Lato Medium/Semibold/Regular and falls back to DejaVu Sans, Liberation Sans or FreeSans, then Raylib's built-in font. Install Lato on Ubuntu/WSL with `sudo apt install fonts-lato` for the intended typography. A cool pearl foreground, brighter secondary text, subtle shadow, more opaque graphite surfaces and a 13 px minimum improve HUD readability. Sector palettes change floor, grid, wall and accent colors independently.
- Warm graphite, stone, ivory, and amber UI colors replace the previous teal-heavy interface.

This is a playable prototype, not a finished commercial game. The game currently uses procedural placeholder geometry rather than finished sprite/model/animation assets; audio, more advanced pathfinding, and a final balance pass remain future work.

## Directory layout

```text
Gridfall/
├── CMakeLists.txt
├── Makefile
├── README.md
├── include/gridfall/
│   ├── core/Types.hpp
│   ├── core/Controls.hpp
│   ├── core/MouseInput.hpp
│   ├── core/Game.hpp
│   ├── world/Level.hpp
│   ├── entities/Player.hpp
│   ├── entities/Enemy.hpp
│   ├── entities/Inventory.hpp
│   ├── systems/CombatSystem.hpp
│   ├── systems/EnemySystem.hpp
│   ├── systems/ProgressionSystem.hpp
│   ├── systems/VfxSystem.hpp
│   └── render/Renderer.hpp
├── src/
│   ├── main.cpp
│   ├── core/Game.cpp
│   ├── core/MouseInput.cpp
│   ├── world/Level.cpp
│   ├── entities/Inventory.cpp
│   ├── systems/{CombatSystem,EnemySystem,ProgressionSystem,VfxSystem}.cpp
│   └── render/Renderer.cpp
├── tests/core_tests.cpp
├── tests/mouse_input_tests.cpp
├── tests/mouse_relative_input_tests.cpp
├── tests/mocks/raylib.h
└── tools/mouse-input-probe.cpp
```

## Dependencies on Ubuntu / WSL

The source targets C++20 and builds a private copy of raylib 6.0 with `PLATFORM=SDL`. This keeps the platform dependency explicit while on WSLg prefers SDL2's native Wayland driver and consumes isolated SDL relative-motion events. CMake fetches the upstream raylib 6.0 source and links it with SDL2. The first configure needs network access to GitHub.

Install common toolchain and Linux desktop development dependencies:

```bash
sudo apt update
sudo apt install -y build-essential cmake git pkg-config libsdl2-dev \
  libgl1-mesa-dev libasound2-dev fonts-lato
```

On WSL, use WSLg (Windows 11) or a configured X/Wayland display. Run `echo "$DISPLAY"` to check that the shell has a display environment.

## Build and run with CMake

From the directory containing `CMakeLists.txt`:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DGRIDFALL_FETCH_RAYLIB=ON \
  -DGRIDFALL_USE_SDL_RAYLIB=ON \
  -DGRIDFALL_BUILD_TESTS=ON \
  -DGRIDFALL_BUILD_MOUSE_PROBE=ON \
  -DGRIDFALL_BUILD_BENCHMARKS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./tools/run-mouse-diagnostic.sh  # Verify eight physical motion directions first
./run-gridfall-wsl.sh
```

`run-gridfall-wsl.sh` selects WSLg's D3D12 backend and requests the NVIDIA adapter, avoiding the `llvmpipe` CPU renderer on systems where that workaround is needed. For a general launch outside WSLg, `./build/gridfall` also works.

By default Gridfall uses a **144 FPS software cap** without requesting VSync, avoids 4× MSAA, and shows an FPS/frame-time readout at the top-right of the game window. To opt into 4× MSAA for smoother geometry edges (potentially lower FPS), launch with `GRIDFALL_MSAA=1 ./run-gridfall-wsl.sh`. To compare with monitor/compositor-paced VSync, run:

```bash
GRIDFALL_VSYNC=1 ./build/gridfall
```

In this mode VSync paces frames to the refresh rate reported by the display/compositor (so the shown rate may be 60 FPS if WSLg is presenting at 60 Hz). Check that Windows is set to 144 Hz and compare both modes on your system. World positions use render interpolation; mouse-look applies relative deltas immediately to avoid input lag.

## Build and run with Make

```bash
make          # configure and build
make test     # run the headless core test suite
make run      # build then launch the graphical game
make clean    # remove build output
```

## Controls

| Input | Action |
|---|---|
| `W A S D` or arrows | Move |
| Mouse | Smooth first-person look (yaw/pitch); aim with center reticle |
| Hold left mouse | Fire (profile rate limits apply) |
| `1` / `2` / `3` | Fast fire / piercing / slow-steady weapon |
| `R` | Reload (time-locked; magazine is ejected) |
| `E` | Open a nearby side door / secret wall, or open a medical/ammo cache |
| `F5` | Save the current run (Continue/Load Save appears on the title menu) |
| `Q` | Use health injector (flat +38 HP) |
| `T` | Use regeneration dose (3.5% max health per one-second tick for 8 seconds) |
| `K` | Open/close the in-game skill tree (simulation pauses) |
| Mouse click on an available node | Purchase that skill if you have enough points and its prerequisite is unlocked |
| Mouse hover over a node | Highlight the node and inspect its status |
| `Esc` while in skill tree | Close the skill tree |
| `P` or `Esc` | Pause / resume |
| `Enter` | Start, proceed after extraction, or redeploy after failure |

## Architecture notes

- **Domain:** `Level`, `Player`, `Enemy`, `Inventory`, and weapon/projectile data do not use raylib types. Positions use a small engine-owned `Vec2`.
- **Simulation/render timing:** game rules update in fixed 1/60-second ticks using an accumulator. Player/enemy positions and facing plus projectile positions are interpolated for rendering between the previous and current simulation states. VFX lifetimes use the real frame delta. One-shot controls are latched until the next fixed tick so fast render frames cannot lose presses.
- **Presentation:** `Renderer` is the only gameplay module that draws the 3D world and integrated HUD. Domain positions remain 2D pixel coordinates; the renderer maps them to world X/Z coordinates using one tile = one 3D world unit. It consumes current domain state rather than owning game rules.
- **3D camera / aim:** gameplay uses a first-person perspective camera whose yaw is shared with the player-facing direction. Gridfall builds Raylib 6.0 with the SDL2 platform backend, but prefers native Wayland relative-pointer events and patches Raylib to accumulate all signed `xrel/yrel` motion received between polls. The headless mouse tests verify all four cardinal and four diagonal direction/sign combinations, recentering, capture lifecycle, and stationary/no-drift behavior. `gridfall_mouse_probe` visualizes the relative deltas and camera facing for manual WSLg validation. The center reticle fires along camera direction. Domain positions remain in 2D pixel coordinates while the renderer maps them to 3D X/Z coordinates.
- **Weapons:** each weapon has its own magazine, reserve and cooldown. Piercing projectiles track enemy IDs already hit so they don't repeatedly damage the same enemy.
- **Progression:** XP is awarded for enemy kills, pickups/cache loot, discovering secret walls, and clearing a sector. Rank-ups award spendable skill points; spend them in the integrated nine-node Survival/Assault/Mobility skill tree.
- **Level completion:** the extraction tile only completes a sector after every spawned enemy is defeated. Clearing a sector unlocks the next one.

## Known prototype constraints

- Artwork is procedurally drawn 3D placeholder geometry rather than finished models or animated sprite/model assets.
- Enemy AI pursues and fires sector-scaled ranged bolts when it has line of sight, in addition to contact attacks. Movement is wall-aware but uses a compact steering model rather than full pathfinding.
- The simulation is fixed-step but is not designed for network determinism or replay serialization.
- Save files preserve sector unlocks, player stats, skill unlocks, consumables and weapon ammo. Loading resumes at the saved sector entrance; enemies and unopened room loot for that sector are regenerated rather than restoring a full tactical snapshot.
- Collision is a compact prototype circle-vs-grid-probe approach, not a full rigid-body solver.

## First-person controls and frame diagnostics

The active play camera is first-person. `WASD` moves relative to where the camera faces; horizontal mouse motion turns yaw and vertical motion changes pitch; left mouse fires along the center reticle. While focused, the cursor is captured with SDL relative mode; WSLg uses the native Wayland path when available. The per-sample displacement is applied once without additional smoothing or time scaling. Camera orientation is preserved across sector transitions. The player avatar is not drawn in gameplay (it is replaced by a camera-locked weapon viewmodel). The title screen uses a separate operator showcase scene: the whole level map is not rendered behind the menu, and the operator's weapon follows the mouse position.

Sector layouts use five stronger spatial grammars (Bastion, Killhouse, Maze, Ring, Gauntlet) that repeat with additional structural changes as the four escalation cycles advance. Enemy population now grows from 8 to 27, with later-sector enemy stats scaling further.

The in-game performance counter is inexpensive by default. Press `F10` to expand the diagnostics panel; press `F11` to export the rolling frame samples to `build/performance/gridfall-frame-samples.csv`. The rolling buffer stores up to 4096 samples and is exported automatically when the game exits cleanly. The panel reports frame period/FPS, update CPU time, render-command submission CPU time, EndDrawing/present wait, p95/p99/max frame time, frame spikes, and input-poll-to-render-submit CPU proxy. **That input metric is not physical input-to-photon latency.**

### Reproducible benchmarks

The full procedure, latest headless results, thresholds, and metric interpretations are documented in [`docs/PERFORMANCE.md`](docs/PERFORMANCE.md).

- `./tools/run-performance-benchmark.sh` runs 20,000 optimized headless simulation ticks with 27 active enemy actors, sustained enemy return-fire, player projectiles, and VFX. It writes CSV/text output under `build/performance/` and treats compiler warnings as errors.
- `./tools/run-live-benchmark.sh` starts the graphical game on the accelerated WSLg D3D12 path, then summarizes the frame capture after you close the game. Play a repeatable scene for 30–60 seconds. The in-game `F10` overlay expands diagnostics; `F11` exports CSV. Keep F10 closed during the cleanest measurement.

The frame report separates frame period, game-update CPU time, render-submission CPU time, `EndDrawing()`/presentation wait, and an input-poll-to-render-submit CPU proxy. The last one is **not** physical input-to-photon latency. The headless benchmark excludes Raylib, the GPU, WSLg, and presentation latency.

### Allocation-churn reduction

Combat and enemy projectile updates now reuse member-owned survivor buffers rather than allocate a temporary vector from scratch each simulation tick. Both vectors retain capacity across ticks; this removes repeated heap allocation/deallocation from the projectile update path once capacities are established. Core tests still pass with warnings treated as errors, and the source passes AddressSanitizer/UndefinedBehaviorSanitizer checks.


## Mouse input regression testing

`gridfall_mouse_input_tests` validates camera math, all eight directions, configurable inversion, full-turn wrapping, and sample-rate-independent look math. `gridfall_mouse_relative_input_tests` compiles production `MouseInput.cpp` against a deterministic Raylib mock to verify center-relative signed deltas, recentering, one-time consumption, stationary zero, and capture lifecycle. The CMake dependency patch now only checks Raylib's bundled `fread` result for strict-warning builds; it no longer modifies relative mouse handling. After building, launch `./run-gridfall-wsl.sh` from a terminal and start a New Game: the first 10 seconds of active gameplay automatically print sampled deltas/peaks alongside camera yaw, pitch, and facing. The optional `./tools/run-mouse-diagnostic.sh` probe visualizes the SDL relative-motion path. Automated tests validate Gridfall's adapter and camera math, not physical WSLg delivery; use the trace for final live acceptance.


### WSLg mouse backend and troubleshooting

Gridfall now fetches and builds its own Raylib 6.0 using the **SDL2 platform backend**. The system-installed Raylib in this WSL environment was built with GLFW, where `DisableCursor()`/`GetMouseDelta()` can show a relative-motion defect under XWayland. GLFW virtual cursor polling also produced huge, mostly-positive host-scaled positions, while a separate XInput2 subscription returned zero events to the game's client. The current project removes both of those paths. Native/raw SDL relative mode is the default because the forced warp fallback produced window-sized deltas in the live trace; `GRIDFALL_SDL_RELATIVE_MOUSE_WARP=1` opts into that legacy fallback for comparison.

During gameplay, `DisableCursor()` activates SDL relative mouse mode. The CMake patch adds every signed SDL `xrel/yrel` packet to a dedicated Gridfall accumulator. `GridfallConsumeRelativeMouseDelta()` returns and clears the pair directly; the camera no longer reconstructs deltas from Raylib's shared absolute cursor-position bookkeeping or applies cursor scaling to those deltas. Raylib's absolute pointer data remains available for the menu and skill-tree UI. The adapter does not create a separate X11 connection or warp/recenter the cursor each frame.

The SDL build needs `libsdl2-dev` and network access to fetch upstream Raylib 6.0. Launch `./run-gridfall-wsl.sh` from a terminal, start a New Game, and watch automatic `[MOUSE_DIAG]` output for the first 10 active gameplay seconds. Reports every quarter-second contain the latest delta, summed X/Y movement, peak per-frame delta, yaw, pitch and facing; the final line shows total movement and peak values. The optional `./tools/run-mouse-diagnostic.sh` remains a visual input probe. The `gridfall_mouse_relative_input_tests` target verifies that gameplay uses the isolated signed SDL accumulator, not `GetMouseDelta()`, and tests axis separation, frame consumption and capture lifecycle; it does not replace the live WSLg test.
