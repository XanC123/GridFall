# Gridfall architecture and implementation milestones

## Module boundaries

```text
raylib platform boundary
    main.cpp -> Game loop and input sampling
                    |
                    +--> Level (tile map, collision queries, interactions)
                    +--> Player + Inventory (domain state)
                    +--> EnemySystem (wall-aware pursuit, contact damage, ranged projectiles)
                    +--> CombatSystem (weapon state/projectile hit tests)
                    +--> ProgressionSystem (XP/rank/skill allocation)
                    +--> VfxSystem (particles/tracers/magazine drops)
                    |
                    +--> Renderer (read-only presentation of current state)
```

`Types.hpp` holds small domain-owned types (`Vec2`, `Tile`, `AmmoType`, and state enums). It deliberately does not include raylib. Domain positions remain 2D pixel coordinates for collision, AI, and combat; `Renderer.cpp` projects them to 3D X/Z coordinates (one tile equals one world unit) and draws the battlefield using raylib 3D primitives. A perspective first-person camera uses mouse delta for yaw/pitch and shares yaw with the player-facing direction. The center reticle fires along the camera-facing direction; the OS cursor is captured during gameplay. A camera-locked weapon viewmodel replaces the third-person player mesh while active. The HUD is drawn as a transparent overlay over the game viewport, not a separate window or side panel. `Game.cpp` is the composition root: it owns the systems, polls platform input, advances fixed simulation ticks, and tells the renderer what to draw.

## Incremental implementation sequence

### Milestone 1 — Window and loop

`src/main.cpp` constructs `Game`. `Game::run()` opens the window and drives the frame loop. `SetExitKey(KEY_NULL)` reserves Escape for in-game pause controls.

### Milestone 2 — Tile world and collision

`Level::load()` initializes a 25 x 18 map, a hard outer border, traversable wall gaps, interactable secret/door tiles, hazards, pickups, spawn points, and extraction. World coordinates are measured in pixels; cell lookup uses integer division by `kTileSize`. `Level::circleHitsSolid()` gives the movement and projectile systems a single collision query. Player movement resolves X and Y separately to support wall sliding.

### Milestone 3 — Player, enemies, and game state

The player uses continuous pixel movement constrained by the tile grid. The game state machine distinguishes title, playing, paused, level-complete, game-over, and victory states. Enemies pursue within a sector-scaled sensing radius, attack on contact, and fire projectiles when line of sight is clear. Enemy speed/aggression, contact damage, detection range, ranged projectile damage/speed/range, and attack cadence scale across 20 sectors.

### Milestone 4 — Weapon and inventory systems

`Player` owns one `WeaponState` per ammunition profile, each with its own magazine, reserve, and cooldown. `CombatSystem` spawns projectiles and sweeps them through the world. Piercing projectiles remember already-hit enemy IDs. `Inventory` owns consumable counts; status timers are updated by the fixed simulation step.

### Milestone 5 — Progression and unlocks

`ProgressionSystem` converts XP into ranks and spendable skill points and owns the nine-node skill tree definition, prerequisite checks, costs, and passive effects. Enemies, pickups, caches, secret discoveries, and completed sectors award XP. The exit activates only after all enemies are defeated; clearing a sector enables progression to the next one, through sector 20.

### Milestone 6 — Feedback and presentation

`VfxSystem` updates tracer lifetimes, particles, and magazine-drop arcs with frame delta. `Renderer` draws a procedural 3D floor plane plus grid lines, raised wall geometry, hazards/pickups, interpolated player/enemy silhouettes and projectiles, VFX, a warm graphite/ivory/amber HUD overlay, state overlays, and an FPS/frame-time panel. This prototype uses generated shapes so it can build without asset packs.

### Milestone 7 — Test without a window

`tests/core_tests.cpp` exercises all 20 unique sector layouts, route connectivity, spawn validity, secret interaction, inventory, XP/rank/skill-tree prerequisite and effect handling, weapon firing/reload, and projectile damage. It does not need to initialize raylib's graphical window.

## Simulation lifecycle

1. Read the real frame delta and clamp long stalls.
2. Poll keyboard/mouse state once per rendered frame.
3. Add frame time to an accumulator.
4. While the accumulator has at least 1/60 second, update player movement/interactions, enemy pursuit, projectile collision, status effects, reload timers, XP, and extraction checks.
5. Update visual effects using elapsed frame time.
6. Draw the latest simulation state. Drawing never changes health, ammo, XP, or positions.

The accumulator keeps movement/combat behavior stable across ordinary frame-rate variation, while render interpolation avoids 60 Hz pose stepping on higher-refresh displays. The default frame limiter targets 144 FPS with VSync off to avoid double pacing; `GRIDFALL_VSYNC=1` opts into compositor/monitor-paced VSync. This is not a guarantee that WSLg itself exposes a 144 Hz presentation path. It is not a lockstep/network-determinism system: no replay state, fixed-point arithmetic, or cross-platform bitwise determinism is promised.

## Extension points

- Replace procedural player/enemy geometry with 3D models/animation resources inside `Renderer` without moving drawing code into `Player` or `Enemy`.
- Add audio and sound cues as another presentation system called from combat events.
- Extend the versioned save format if more campaign state (room-open flags or tactical enemy state) needs to persist; never serialize raw class memory.
- Add navigation/pathfinding in `EnemySystem`, keeping collision authority inside `Level`.
- Expand the skill tree with unlockable active abilities and persistent build/save data.
- Move large enemy/projectile counts to packed pools/spatial partitioning if profiling shows the simple vectors and pairwise checks are insufficient.

## Skill-tree interaction

Press `K` during play to open the integrated skill-tree overlay. The world simulation pauses while the tree is open and the cursor becomes visible. Skill cards are clickable; hovering highlights a card, and clicking an available node attempts a purchase. The three columns are Survival, Assault, and Mobility. Tier two requires tier one in its branch; tier three requires tier two and costs two points. Purchases are validated by `ProgressionSystem::canPurchaseSkill()` and applied once by `purchaseSkill()`. The rendering layer displays the node state and exposes a matching hit-test so click geometry stays aligned with visible cards.

## Twenty-sector generation and enemy pressure

`Level::load()` cycles five base room grammars through four structural-density phases. It carves the required spawn-to-extraction lane, flood-fills reachable cells, and scores reachable ordinary floor tiles for enemy spawn placement. The enemy count grows from three to eighteen and is capped to bound presentation/AI cost; health, speed, aggression, detection radius, contact damage, and attack cooldown keep increasing after that cap.


## Recent systems added

- `SaveSystem` owns a versioned plain-text save format. It validates imported values, writes to a sibling temporary file and atomically renames it into place. Saves include sector progress, core Player stats, inventory, skill tree flags and per-weapon ammunition. Runtime positions, active projectiles, enemy states and map tile mutations are intentionally reset when resuming a sector.
- `EnemySystem` owns enemy projectiles. Hostiles with clear line of sight use sector-scaled ranged shots. Projectiles are swept against walls and the player on fixed ticks; the Renderer only draws them.
- `Level::palette()` exposes a headless `LevelPalette` per sector. Theme values are `Rgba`, keeping Raylib types out of the world/domain layer.
- Each sector has a sealed tile side chamber with one interactable `SideDoor`. The north corridor always contains light, piercing, and heavy ammo pickups, while the chamber contains a medical cache, an ammo cache, and bonus supplies. Press `E` close to the door to open it, then press `E` beside a cache; `Level::interactNear()` removes the opened tile and returns a reward type. The Game applies rewards and the Renderer displays the remaining world tiles.
- The title menu is part of the normal Renderer overlay. It displays the operator in the live 3D world, while its item index and save/load decisions remain owned by `Game`. `Continue / Load Save`, `New Game`, and `Exit` are selectable with keyboard or mouse. `F5` writes a versioned save during play; sector clearance autosaves progression.
- Tests use `CHECK` rather than `assert`, so release builds keep executing validations when `NDEBUG` is enabled.
