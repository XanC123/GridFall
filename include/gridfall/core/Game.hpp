#pragma once
#include "gridfall/core/Controls.hpp"
#include "gridfall/core/MouseInput.hpp"
#include "gridfall/entities/Enemy.hpp"
#include "gridfall/entities/Player.hpp"
#include "gridfall/render/Renderer.hpp"
#include "gridfall/systems/CombatSystem.hpp"
#include "gridfall/systems/EnemySystem.hpp"
#include "gridfall/systems/ProgressionSystem.hpp"
#include "gridfall/systems/SaveSystem.hpp"
#include "gridfall/systems/VfxSystem.hpp"
#include "gridfall/world/Level.hpp"
#include <array>
#include <chrono>
#include <string>
#include <vector>

namespace gridfall {

class Game {
public:
    int run();

private:
    struct InputFrame {
        float moveX{};
        float moveY{};
        float lookDeltaX{};
        float lookDeltaY{};
        bool fireHeld{};
        bool firePressed{};
        bool reloadPressed{};
        bool interactPressed{};
        bool healthPotionPressed{};
        bool regenPotionPressed{};
        bool switchFast{};
        bool switchPiercing{};
        bool switchSteady{};
        bool skillTreePressed{};
        std::array<bool, kSkillCount> purchaseSkill{};
        bool confirmPressed{};
        bool pausePressed{};
        bool menuUp{};
        bool menuDown{};
        bool menuMousePressed{};
        int mouseX{};
        int mouseY{};
        bool savePressed{};
    };

    Level level_;
    Player player_;
    std::vector<Enemy> enemies_;
    CombatSystem combat_;
    EnemySystem enemySystem_;
    VfxSystem vfx_;
    Renderer renderer_;
    GameState state_{GameState::Title};
    int currentLevel_{0};
    int unlockedLevel_{1};
    float accumulator_{};
    MouseLookController mouseLook_{};
    MouseInput mouseInput_{};
    bool cursorCaptured_{};
    std::chrono::steady_clock::time_point inputPollTime_{};
    float lastUpdateMs_{};
    float lastRenderCpuMs_{};
    float lastPresentMs_{};
    float lastInputToSubmitMs_{};
    // Edge-triggered controls are latched until a simulation tick consumes them.
    // This prevents short key/mouse clicks being lost while rendering faster than 60 Hz.
    InputFrame pendingEdges_{};
    std::string toast_{"CLEAR THE ROOM. FIND THE EXTRACTION GRID."};
    float toastTimer_{4.0f};
    bool interactionWasBlocked_{};
    bool skillTreeOpen_{};
    bool hasSave_{};
    bool exitRequested_{};
    int menuSelection_{1};
    std::string saveNotice_{};

    void resetRun();
    void startNewGame();
    bool loadSavedGame();
    bool saveGame();
    void loadLevel(int index);
    [[nodiscard]] InputFrame pollInput();
    void updateFrame(float frameDt);
    void tick(float dt, const InputFrame& input);
    void updatePlayer(float dt, const InputFrame& input);
    void collectTileUnderPlayer();
    void setToast(std::string message, float seconds = 2.2f);
    void draw();
};

} // namespace gridfall
