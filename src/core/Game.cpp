#include "gridfall/core/Game.hpp"
#include <raylib.h>
#include <algorithm>
#include <cmath>
#include <utility>
#include <cstdlib>
#include <cstdio>
#include <string_view>
#include <chrono>

namespace gridfall {
namespace {
constexpr int kWindowWidth = 1360;
constexpr int kWindowHeight = 780;
constexpr float kMaxFrameDelta = 0.10f;
constexpr int kTargetFramesPerSecond = 144;

bool environmentFlagEnabled(const char* name) {
    const char* value = std::getenv(name);
    return value != nullptr && std::string_view(value) == "1";
}
}

int Game::run() {
    // Use one frame-pacing mechanism at a time. The default is a 144 FPS cap,
    // which matches a 144 Hz display without stacking Raylib's software limiter
    // on top of a VSync wait. Set GRIDFALL_VSYNC=1 to let the display/compositor
    // pace frames instead (useful if VSync is smoother on the current WSLg setup).
    const bool useVsync = environmentFlagEnabled("GRIDFALL_VSYNC");
    unsigned int windowFlags = FLAG_WINDOW_RESIZABLE;
    // Multisampling can cost frame time on the WSLg D3D12 path. Keep it off for
    // a throughput-first default; opt in with GRIDFALL_MSAA=1 if edges need smoothing.
    if (environmentFlagEnabled("GRIDFALL_MSAA")) windowFlags |= FLAG_MSAA_4X_HINT;
    if (useVsync) windowFlags |= FLAG_VSYNC_HINT;
    SetConfigFlags(windowFlags);
    InitWindow(kWindowWidth, kWindowHeight, "GRIDFALL // BELOW THE LINE");
    if (!IsWindowReady()) return 1;
    renderer_.initialize();
    (void)mouseInput_.initialize();
    TraceLog(LOG_INFO, "Gridfall mouse input backend: %s", mouseInput_.backendName());
    const MouseLookSettings& mouseSettings = mouseLook_.settings();
    std::fprintf(stderr,
                 "[MOUSE_CONFIG] horizontal=%.4f rad/pixel vertical=%.4f rad/pixel "
                 "invertX=%s invertY=%s pitch=[%.2f,%.2f] rad\n",
                 static_cast<double>(mouseSettings.horizontalRadiansPerPixel),
                 static_cast<double>(mouseSettings.verticalRadiansPerPixel),
                 mouseSettings.invertHorizontal ? "true" : "false",
                 mouseSettings.invertVertical ? "true" : "false",
                 static_cast<double>(mouseSettings.minimumPitchRadians),
                 static_cast<double>(mouseSettings.maximumPitchRadians));
    std::fprintf(stderr,
                 "[MOUSE_DIAG] Armed: start New Game to collect the first 10 seconds of active gameplay input.\n");
    std::fflush(stderr);
    SetExitKey(KEY_NULL); // Escape is a game/menu key; it should not close the app.
    SetTargetFPS(useVsync ? 0 : kTargetFramesPerSecond);
    resetRun();
    hasSave_ = SaveSystem::exists();
    menuSelection_ = hasSave_ ? 0 : 1;
    saveNotice_ = hasSave_ ? "SAVE PROFILE FOUND" : "NO SAVE FILE YET";

    while (!WindowShouldClose() && !exitRequested_) {
        const float frameDt = GetFrameTime();

        // Capture the cursor only for first-person gameplay. Menu and pause states
        // use a visible pointer. Avoid repeatedly toggling the OS cursor every frame.
        const bool windowFocusedNow = IsWindowFocused();
        const bool shouldCaptureCursor = state_ == GameState::Playing && !skillTreeOpen_ && windowFocusedNow;
        if (shouldCaptureCursor != cursorCaptured_) {
            // MouseInput owns cursor capture. The SDL-built Raylib backend uses
            // SDL relative mouse mode and signed xrel/yrel event deltas. No X11
            // subscription, GLFW virtual cursor polling, or per-frame cursor warping.
            mouseInput_.setCaptured(shouldCaptureCursor);
            cursorCaptured_ = shouldCaptureCursor;
        }

        if (IsKeyPressed(KEY_F10)) renderer_.togglePerformanceOverlay();
        if (IsKeyPressed(KEY_F11)) renderer_.exportPerformanceCsv("build/performance/gridfall-frame-samples.csv");
        if (state_ == GameState::Title) {
            const Vector2 mouse = GetMousePosition();
            renderer_.updateTitleAim(mouse.x, mouse.y, frameDt);
        }

        const auto updateStart = std::chrono::steady_clock::now();
        updateFrame(frameDt);
        const auto updateEnd = std::chrono::steady_clock::now();
        lastUpdateMs_ = std::chrono::duration<float, std::milli>(updateEnd - updateStart).count();

        draw();
        renderer_.recordPerformanceSample({frameDt * 1000.0f, lastUpdateMs_, lastRenderCpuMs_,
                                           lastPresentMs_, lastInputToSubmitMs_});
    }

    // Always write a final rolling window on clean exit, even if F11 was not pressed.
    renderer_.exportPerformanceCsv("build/performance/gridfall-frame-samples.csv");
    mouseInput_.setCaptured(false);
    mouseInput_.shutdown();
    renderer_.shutdown();
    CloseWindow();
    return 0;
}

void Game::resetRun() {
    player_ = Player{};
    combat_.clear();
    enemySystem_.clearProjectiles();
    vfx_ = VfxSystem{};
    currentLevel_ = 0;
    unlockedLevel_ = 1;
    accumulator_ = 0.0f;
    mouseLook_.reset();
    pendingEdges_ = InputFrame{};
    state_ = GameState::Title;
    skillTreeOpen_ = false;
    setToast("CLEAR THE ROOM. FIND THE EXTRACTION GRID.", 4.0f);
    loadLevel(0);
}

void Game::startNewGame() {
    resetRun();
    state_ = GameState::Playing;
    setToast("NEW RUN // CLEAR HOSTILES, LOOT CACHES, REACH EXTRACTION", 3.2f);
}

bool Game::saveGame() {
    SaveData data{};
    // Autosaves made on the clearance screen resume at the next sector rather
    // than asking the player to replay a sector they already completed.
    data.currentLevel = (state_ == GameState::LevelComplete && currentLevel_ < kMaxLevels - 1)
                            ? currentLevel_ + 1 : currentLevel_;
    data.unlockedLevel = unlockedLevel_;
    data.player = player_;
    std::string error;
    if (!SaveSystem::save(data, &error)) {
        saveNotice_ = error;
        setToast("SAVE FAILED // " + error, 2.8f);
        return false;
    }
    hasSave_ = true;
    const int savedSector = data.currentLevel + 1;
    saveNotice_ = "SAVED // SECTOR " + std::to_string(savedSector);
    setToast("RUN SAVED // SECTOR " + std::to_string(savedSector), 2.0f);
    return true;
}

bool Game::loadSavedGame() {
    SaveData data{};
    std::string error;
    if (!SaveSystem::load(data, &error)) {
        hasSave_ = SaveSystem::exists();
        saveNotice_ = error;
        setToast("LOAD FAILED // " + error, 2.6f);
        return false;
    }
    player_ = data.player;
    currentLevel_ = data.currentLevel;
    unlockedLevel_ = data.unlockedLevel;
    loadLevel(currentLevel_);
    state_ = GameState::Playing;
    skillTreeOpen_ = false;
    accumulator_ = 0.0f;
    pendingEdges_ = InputFrame{};
    setToast("SAVE LOADED // SECTOR " + std::to_string(currentLevel_ + 1), 2.5f);
    return true;
}

void Game::loadLevel(int index) {
    currentLevel_ = std::clamp(index, 0, kMaxLevels - 1);
    level_.load(currentLevel_);
    vfx_ = VfxSystem{};
    player_.position = level_.playerSpawn();
    player_.previousPosition = player_.position;
    player_.velocity = {};
    player_.facing = {1.0f, 0.0f};
    mouseLook_.reset();
    player_.reloading = false;
    player_.reloadTimer = 0.0f;
    player_.poisoned = false;
    player_.poisonTickTimer = 0.0f;
    player_.hazardCooldown = 0.0f;
    combat_.clear();
    enemySystem_.clearProjectiles();

    enemies_.clear();
    std::uint32_t id = static_cast<std::uint32_t>(currentLevel_ * 100 + 1);
    for (const EnemySpawn& spawn : level_.enemySpawns()) {
        Enemy enemy{};
        enemy.id = id++;
        enemy.position = spawn.position;
        enemy.previousPosition = enemy.position;
        // The enemy system owns the sector difficulty curve; Game only composes the world.
        EnemySystem::configureForSector(enemy, currentLevel_, spawn.healthScale);
        enemies_.push_back(enemy);
    }
}

Game::InputFrame Game::pollInput() {
    InputFrame input{};
    input.moveX = (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT) ? 1.0f : 0.0f) -
                  (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT) ? 1.0f : 0.0f);
    input.moveY = (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP) ? 1.0f : 0.0f) -
                  (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN) ? 1.0f : 0.0f);
    const Vector2 mouse = GetMousePosition();
    // Camera movement consumes SDL's signed relative xrel/yrel deltas via MouseInput.
    // Do not reconstruct motion from absolute cursor positions or multiply deltas by dt.
    if (cursorCaptured_ && IsWindowFocused()) {
        const MouseDelta delta = mouseInput_.consumeDelta();
        input.lookDeltaX = delta.x;
        input.lookDeltaY = delta.y;
    } else {
        input.lookDeltaX = 0.0f;
        input.lookDeltaY = 0.0f;
    }
    input.fireHeld = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    input.firePressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    input.reloadPressed = IsKeyPressed(KEY_R);
    input.interactPressed = IsKeyPressed(KEY_E);
    input.healthPotionPressed = IsKeyPressed(KEY_Q);
    input.regenPotionPressed = IsKeyPressed(KEY_T);
    input.switchFast = IsKeyPressed(KEY_ONE);
    input.switchPiercing = IsKeyPressed(KEY_TWO);
    input.switchSteady = IsKeyPressed(KEY_THREE);
    input.skillTreePressed = IsKeyPressed(KEY_K);
    input.confirmPressed = IsKeyPressed(KEY_ENTER);
    input.pausePressed = IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE);
    input.menuUp = IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W);
    input.menuDown = IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S);
    input.menuMousePressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    input.mouseX = static_cast<int>(mouse.x);
    input.mouseY = static_cast<int>(mouse.y);
    if (skillTreeOpen_ && input.menuMousePressed) {
        const int clickedSkill = renderer_.skillTreeItemAt(input.mouseX, input.mouseY);
        if (clickedSkill >= 0 && clickedSkill < static_cast<int>(kSkillCount))
            input.purchaseSkill[static_cast<std::size_t>(clickedSkill)] = true;
    }
    input.savePressed = IsKeyPressed(KEY_F5);
    return input;
}

void Game::updateFrame(float frameDt) {
    const float dt = clampf(frameDt, 0.0f, kMaxFrameDelta);
    if (toastTimer_ > 0.0f) toastTimer_ = std::max(0.0f, toastTimer_ - dt);
    vfx_.update(dt);
    inputPollTime_ = std::chrono::steady_clock::now();
    InputFrame input = pollInput();

    // MouseInput suppresses stale/transition samples during cursor capture changes;
    // only valid relative deltas reach the camera controller below.
    if (state_ == GameState::Playing && !skillTreeOpen_ && IsWindowFocused()) {
        mouseLook_.update(input.lookDeltaX, input.lookDeltaY);
        player_.facing = mouseLook_.facing();
        mouseInput_.reportGameplaySample({input.lookDeltaX, input.lookDeltaY}, mouseLook_, frameDt);
    }

    if (state_ == GameState::Title) {
        if (input.menuUp) menuSelection_ = (menuSelection_ + 2) % 3;
        if (input.menuDown) menuSelection_ = (menuSelection_ + 1) % 3;
        if (input.menuMousePressed) {
            const int hovered = renderer_.titleMenuItemAt(input.mouseX, input.mouseY);
            if (hovered >= 0) menuSelection_ = hovered;
            else return;
        } else if (!input.confirmPressed) {
            return;
        }

        switch (menuSelection_) {
            case 0:
                if (hasSave_) loadSavedGame();
                else saveNotice_ = "NO SAVE TO CONTINUE // CHOOSE NEW GAME";
                break;
            case 1:
                startNewGame();
                break;
            case 2:
                exitRequested_ = true;
                break;
            default: break;
        }
        return;
    }
    if (state_ == GameState::Victory || state_ == GameState::GameOver) {
        if (input.pausePressed) {
            state_ = GameState::Title;
            hasSave_ = SaveSystem::exists();
            menuSelection_ = hasSave_ ? 0 : 1;
            saveNotice_ = hasSave_ ? "SAVE PROFILE FOUND" : "NO SAVE FILE YET";
        } else if (input.confirmPressed) startNewGame();
        return;
    }
    if (state_ == GameState::LevelComplete) {
        if (input.confirmPressed) {
            if (currentLevel_ >= kMaxLevels - 1) { state_ = GameState::Victory; return; }
            loadLevel(currentLevel_ + 1);
            state_ = GameState::Playing;
            setToast("SECTOR UNLOCKED // PREPARE FOR CONTACT", 3.0f);
        }
        return;
    }

    // The skill tree is an in-game overlay. Simulation is frozen while it is open,
    // and purchases are applied immediately to the Player's live combat stats.
    if (state_ == GameState::Playing && input.skillTreePressed) {
        skillTreeOpen_ = !skillTreeOpen_;
        pendingEdges_ = InputFrame{};
        setToast(skillTreeOpen_ ? "SKILL TREE // CLICK AN AVAILABLE NODE" : "RETURN TO COMBAT", 1.4f);
        return;
    }
    if (skillTreeOpen_) {
        if (input.pausePressed) {
            skillTreeOpen_ = false;
            setToast("RETURN TO COMBAT", 1.0f);
            return;
        }
        for (std::size_t i = 0; i < input.purchaseSkill.size(); ++i) {
            if (!input.purchaseSkill[i]) continue;
            const SkillId id = static_cast<SkillId>(i);
            if (ProgressionSystem::purchaseSkill(player_, id)) {
                setToast(std::string("SKILL UNLOCKED // ") + ProgressionSystem::skillDefinition(id).name, 1.7f);
            } else if (!ProgressionSystem::hasSkill(player_, id)) {
                setToast("SKILL LOCKED // NEED POINTS OR PREREQUISITE", 1.4f);
            }
        }
        return;
    }

    // F5 saves explicit, persistent progress during play or pause. Skill-tree
    // purchases are handled by clicking the visible node cards.
    if ((state_ == GameState::Playing || state_ == GameState::Paused) && input.savePressed) {
        saveGame();
    }

    if (input.pausePressed) {
        state_ = state_ == GameState::Paused ? GameState::Playing : GameState::Paused;
        return;
    }
    if (state_ == GameState::Paused) return;

    // Latch one-shot controls across render frames. At 144 FPS the renderer can
    // run 2-3 times between 60 Hz simulation ticks; without this buffer a press
    // could occur on a frame which performs no tick and disappear before use.
    pendingEdges_.firePressed |= input.firePressed;
    pendingEdges_.reloadPressed |= input.reloadPressed;
    pendingEdges_.interactPressed |= input.interactPressed;
    pendingEdges_.healthPotionPressed |= input.healthPotionPressed;
    pendingEdges_.regenPotionPressed |= input.regenPotionPressed;
    pendingEdges_.switchFast |= input.switchFast;
    pendingEdges_.switchPiercing |= input.switchPiercing;
    pendingEdges_.switchSteady |= input.switchSteady;

    accumulator_ += dt;
    int ticks = 0;
    while (accumulator_ >= kFixedStep && ticks < 8) {
        InputFrame tickInput = input;
        tickInput.firePressed = pendingEdges_.firePressed;
        tickInput.reloadPressed = pendingEdges_.reloadPressed;
        tickInput.interactPressed = pendingEdges_.interactPressed;
        tickInput.healthPotionPressed = pendingEdges_.healthPotionPressed;
        tickInput.regenPotionPressed = pendingEdges_.regenPotionPressed;
        tickInput.switchFast = pendingEdges_.switchFast;
        tickInput.switchPiercing = pendingEdges_.switchPiercing;
        tickInput.switchSteady = pendingEdges_.switchSteady;
        tick(kFixedStep, tickInput);

        // A buffered press is consumed by exactly one simulation tick.
        pendingEdges_.firePressed = false;
        pendingEdges_.reloadPressed = false;
        pendingEdges_.interactPressed = false;
        pendingEdges_.healthPotionPressed = false;
        pendingEdges_.regenPotionPressed = false;
        pendingEdges_.switchFast = false;
        pendingEdges_.switchPiercing = false;
        pendingEdges_.switchSteady = false;

        accumulator_ -= kFixedStep;
        ++ticks;
        if (state_ != GameState::Playing) break;
    }
    if (ticks >= 8) accumulator_ = 0.0f; // Avoid a spiral of death after a long frame stall.
}

void Game::tick(float dt, const InputFrame& input) {
    // Preserve the last simulation position before updating the world. Position
    // rendering is interpolated at high refresh rates; raw mouse-look updates at poll rate.
    player_.previousPosition = player_.position;
    for (Enemy& enemy : enemies_) enemy.previousPosition = enemy.position;

    updatePlayer(dt, input);
    enemySystem_.update(enemies_, player_, level_, dt);
    const int killedXp = combat_.updateProjectiles(level_, enemies_, vfx_, dt);
    if (killedXp > 0) {
        ProgressionSystem::awardXp(player_, killedXp);
        setToast("HOSTILE DOWN  //  +" + std::to_string(killedXp) + " XP", 1.5f);
    }
    combat_.updateReload(player_, dt);

    if (player_.health <= 0.0f) {
        player_.health = 0.0f;
        state_ = GameState::GameOver;
        setToast("SIGNAL LOST", 3.0f);
        return;
    }

    if (level_.tileAtWorld(player_.position) == Tile::Exit) {
        if (EnemySystem::aliveCount(enemies_) == 0) {
            ProgressionSystem::awardXp(player_, 40);
            unlockedLevel_ = std::max(unlockedLevel_, std::min(kMaxLevels, currentLevel_ + 2));
            state_ = GameState::LevelComplete;
            // Sector-clear rewards are granted before autosaving, so Continue
            // resumes the next sector with the replenished field kit.
            player_.health = std::min(player_.maxHealth, player_.health + 28.0f);
            player_.inventory.addHealthPotion(1);
            if ((currentLevel_ % 2) == 0) player_.inventory.addRegenPotion(1);
            saveGame();
            setToast("SECTOR CLEARED // PROGRESS AUTO-SAVED", 4.0f);
        } else if (!interactionWasBlocked_) {
            setToast("EXTRACTION LOCKED // CLEAR ALL HOSTILES", 2.0f);
            interactionWasBlocked_ = true;
        }
    } else {
        interactionWasBlocked_ = false;
    }
}

void Game::updatePlayer(float dt, const InputFrame& input) {
    player_.animationTime += dt;
    player_.damageFlash = std::max(0.0f, player_.damageFlash - dt);
    player_.hazardCooldown = std::max(0.0f, player_.hazardCooldown - dt);

    // WASD is relative to the current first-person camera facing. The helper normalizes
    // diagonals, so W+D is not faster than W alone.
    const Vec2 movement = cameraRelativeMovement(player_.facing, input.moveX, input.moveY);
    const float movementScale = player_.reloading ? 0.72f : 1.0f;
    player_.velocity = movement * (player_.moveSpeed * movementScale);

    // Axis-separated collision allows the player to slide along walls instead of snagging on corners.
    Vec2 candidate = player_.position;
    candidate.x += player_.velocity.x * dt;
    if (!level_.circleHitsSolid(candidate, player_.radius)) player_.position.x = candidate.x;
    candidate = player_.position;
    candidate.y += player_.velocity.y * dt;
    if (!level_.circleHitsSolid(candidate, player_.radius)) player_.position.y = candidate.y;

    if (input.interactPressed) {
        const InteractionResult result = level_.interactNear(player_.position);
        if (result.changed) {
            if (result.secret) {
                ProgressionSystem::awardXp(player_, 30);
                setToast("SECRET ROUTE DISCOVERED  //  +30 XP", 2.8f);
                vfx_.hitBurst(player_.position, level_.palette().accent, 15);
            } else if (result.cache) {
                player_.inventory.addHealthPotion();
                player_.inventory.addRegenPotion();
                ProgressionSystem::awardXp(player_, 45);
                setToast("CACHE OPENED  // +45 XP // MEDS SECURED", 2.3f);
                vfx_.hitBurst(player_.position, level_.palette().accent, 18);
            } else if (result.ammoCache) {
                player_.weapons[0].reserve += 60;
                player_.weapons[1].reserve += 30;
                player_.weapons[2].reserve += 18;
                player_.inventory.addHealthPotion();
                ProgressionSystem::awardXp(player_, 45);
                setToast("AMMO CACHE OPENED  // ALL WEAPONS RESUPPLIED", 2.6f);
                vfx_.hitBurst(player_.position, {255, 208, 105, 255}, 22);
            } else {
                setToast("SIDE DOOR OPEN // ROOM ACCESS GRANTED", 1.8f);
            }
        } else if (level_.tileAtWorld(player_.position) == Tile::Exit) {
            setToast("EXTRACTION GRID // REMOVE ALL HOSTILES", 2.2f);
        }
    }

    if (input.switchFast || input.switchPiercing || input.switchSteady) {
        if (!player_.reloading) {
            player_.weaponIndex = input.switchFast ? 0 : (input.switchPiercing ? 1 : 2);
            setToast(std::string("EQUIPPED // ") + CombatSystem::spec(player_.ammoType()).name, 1.15f);
        }
    }


    if (input.healthPotionPressed) {
        if (player_.inventory.healthPotions > 0 && player_.health < player_.maxHealth) {
            player_.inventory.consumeHealthPotion();
            player_.health = std::min(player_.maxHealth, player_.health + 38.0f);
            setToast("HEALTH INJECTOR // +38 HP", 1.5f);
            vfx_.hitBurst(player_.position, {100, 255, 155, 255}, 10);
        } else setToast("NO HEALTH INJECTOR AVAILABLE", 1.2f);
    }
    if (input.regenPotionPressed) {
        if (player_.inventory.consumeRegenPotion()) {
            player_.regenRemaining = 8.0f;
            player_.regenTickTimer = 0.05f;
            setToast("REGEN ACTIVE // 3.5% MAX HP PER TICK", 1.8f);
        } else setToast("NO REGEN DOSE AVAILABLE", 1.2f);
    }

    if (input.reloadPressed) {
        if (combat_.requestReload(player_, vfx_)) setToast("RELOADING // MAGAZINE EJECTED", 1.3f);
        else if (!player_.reloading && player_.activeWeapon().magazine == CombatSystem::spec(player_.ammoType()).magazineSize)
            setToast("MAGAZINE ALREADY FULL", 1.0f);
        else if (player_.activeWeapon().reserve <= 0) setToast("RESERVE AMMO EMPTY", 1.2f);
    }

    const Tile standingTile = level_.tileAtWorld(player_.position);
    player_.poisoned = standingTile == Tile::Poison;
    if (player_.poisoned) {
        player_.poisonTickTimer -= dt;
        if (player_.poisonTickTimer <= 0.0f) {
            player_.health = std::max(0.0f, player_.health - 4.0f * (1.0f - clampf(player_.damageResistance, 0.0f, 0.75f)));
            player_.poisonTickTimer = 1.0f;
        }
    } else player_.poisonTickTimer = 0.0f;

    if (standingTile == Tile::Spikes && player_.hazardCooldown <= 0.0f) {
        player_.health = std::max(0.0f, player_.health - 12.0f * (1.0f - clampf(player_.damageResistance, 0.0f, 0.75f)));
        player_.hazardCooldown = 0.78f;
        player_.damageFlash = 0.16f;
        vfx_.hitBurst(player_.position, {255, 96, 105, 255}, 5);
        setToast("HAZARD CONTACT // 12 DAMAGE", 1.0f);
    }

    if (player_.regenRemaining > 0.0f) {
        player_.regenTickTimer -= dt;
        if (player_.regenTickTimer <= 0.0f) {
            player_.health = std::min(player_.maxHealth, player_.health + player_.maxHealth * player_.regenPercentPerTick);
            player_.regenTickTimer += 1.0f;
        }
        player_.regenRemaining = std::max(0.0f, player_.regenRemaining - dt);
    }

    collectTileUnderPlayer();
    const FireResult fire = combat_.tryFire(player_, player_.facing, input.fireHeld || input.firePressed, vfx_);
    if (fire == FireResult::Empty && toastTimer_ < 0.1f) setToast("MAG EMPTY // PRESS R TO RELOAD", 1.5f);
}

void Game::collectTileUnderPlayer() {
    const int x = static_cast<int>(player_.position.x / kTileSize);
    const int y = static_cast<int>(player_.position.y / kTileSize);
    const Tile tile = level_.tileAt(x, y);
    switch (tile) {
        case Tile::HealthPotion:
            player_.inventory.addHealthPotion();
            ProgressionSystem::awardXp(player_, 15);
            level_.setTile(x, y, Tile::Floor);
            setToast("HEALTH INJECTOR SECURED  //  +15 XP", 1.8f);
            vfx_.hitBurst(player_.position, {101, 255, 160, 255}, 10);
            break;
        case Tile::RegenPotion:
            player_.inventory.addRegenPotion();
            ProgressionSystem::awardXp(player_, 15);
            level_.setTile(x, y, Tile::Floor);
            setToast("REGEN DOSE SECURED  //  +15 XP", 1.8f);
            vfx_.hitBurst(player_.position, {95, 213, 255, 255}, 10);
            break;
        case Tile::AmmoLight:
            player_.weapons[0].reserve += 40;
            ProgressionSystem::awardXp(player_, 8);
            level_.setTile(x, y, Tile::Floor);
            setToast("LIGHT AMMO +40  // +8 XP", 1.6f);
            vfx_.hitBurst(player_.position, {255, 203, 119, 255}, 8);
            break;
        case Tile::AmmoPiercing:
            player_.weapons[1].reserve += 20;
            ProgressionSystem::awardXp(player_, 10);
            level_.setTile(x, y, Tile::Floor);
            setToast("PIERCING AMMO +20  // +10 XP", 1.6f);
            vfx_.hitBurst(player_.position, {132, 187, 223, 255}, 8);
            break;
        case Tile::AmmoHeavy:
            player_.weapons[2].reserve += 10;
            ProgressionSystem::awardXp(player_, 12);
            level_.setTile(x, y, Tile::Floor);
            setToast("HEAVY AMMO +10  // +12 XP", 1.6f);
            vfx_.hitBurst(player_.position, {231, 99, 91, 255}, 8);
            break;
        case Tile::Cache:
        case Tile::AmmoCache:
            // Crates are opened deliberately with E, never silently auto-looted.
            break;
        default: break;
    }
}

void Game::setToast(std::string message, float seconds) {
    toast_ = std::move(message);
    toastTimer_ = seconds;
}

void Game::draw() {
    BeginDrawing();
    const auto renderStart = std::chrono::steady_clock::now();
    renderer_.draw(level_, player_, enemies_, enemySystem_.projectiles(), combat_, vfx_, state_, currentLevel_ + 1,
                   unlockedLevel_, toast_, toastTimer_, clampf(accumulator_ / kFixedStep, 0.0f, 1.0f),
                   mouseLook_.pitch(), skillTreeOpen_, hasSave_, menuSelection_, saveNotice_);
    const auto renderEnd = std::chrono::steady_clock::now();
    EndDrawing();
    const auto presentEnd = std::chrono::steady_clock::now();
    lastRenderCpuMs_ = std::chrono::duration<float, std::milli>(renderEnd - renderStart).count();
    lastPresentMs_ = std::chrono::duration<float, std::milli>(presentEnd - renderEnd).count();
    lastInputToSubmitMs_ = inputPollTime_.time_since_epoch().count() == 0
        ? 0.0f
        : std::chrono::duration<float, std::milli>(renderEnd - inputPollTime_).count();
}

} // namespace gridfall
