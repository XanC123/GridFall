#pragma once
#include "gridfall/core/Types.hpp"
#include "gridfall/entities/Enemy.hpp"
#include "gridfall/entities/Player.hpp"
#include "gridfall/systems/CombatSystem.hpp"
#include "gridfall/systems/VfxSystem.hpp"
#include "gridfall/world/Level.hpp"
#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace gridfall {

struct PerformanceSample {
    float frameMs{};          // Previous presented frame period (includes frame pacing)
    float updateMs{};         // Game/simulation/input CPU time
    float renderCpuMs{};      // CPU time spent submitting drawing commands
    float presentMs{};        // EndDrawing/present time; may include compositor/VSync waits
    float inputToSubmitMs{};  // Poll-input to renderer-submit CPU proxy, not photon latency
};

// Presentation only: simulation stays in 2D grid coordinates, rendered from a
// first-person camera. Menus use a deliberately separate operator showcase scene.
class Renderer {
public:
    void initialize();
    void shutdown();

    void draw(const Level& level, const Player& player, const std::vector<Enemy>& enemies,
              const std::vector<EnemyProjectile>& enemyProjectiles,
              const CombatSystem& combat, const VfxSystem& vfx, GameState state,
              int levelNumber, int unlockedLevel, const std::string& toast, float toastTimer,
              float interpolationAlpha, float cameraPitch, bool skillTreeOpen,
              bool hasSave, int menuSelection, const std::string& saveNotice) const;

    [[nodiscard]] int titleMenuItemAt(int mouseX, int mouseY) const noexcept;
    [[nodiscard]] int skillTreeItemAt(int mouseX, int mouseY) const noexcept;
    void updateTitleAim(float mouseX, float mouseY, float frameDt) noexcept;
    void recordPerformanceSample(PerformanceSample sample) noexcept;
    void togglePerformanceOverlay() noexcept { performanceExpanded_ = !performanceExpanded_; }
    void exportPerformanceCsv(const char* path) const;

private:
    void drawTitleShowcase(const Player& player) const;
    void drawMap(const Level& level, Vec2 cameraPosition, Vec2 cameraFacing) const;
    void drawPlayer(const Player& player, float weaponPitch = 0.0f) const;
    void drawViewWeapon(const Player& player) const;
    void drawEnemy(const Enemy& enemy) const;
    void drawHud(const Player& player, const std::vector<Enemy>& enemies,
                 GameState state, int levelNumber, int unlockedLevel) const;
    void drawOverlay(GameState state, int levelNumber, const std::string& toast, float toastTimer,
                     bool hasSave, int menuSelection, const std::string& saveNotice) const;
    void drawSkillTree(const Player& player) const;
    void drawVfx(const VfxSystem& vfx) const;
    void drawProjectiles(const CombatSystem& combat, float interpolationAlpha) const;
    void drawEnemyProjectiles(const std::vector<EnemyProjectile>& projectiles, float interpolationAlpha) const;
    void drawPerformanceOverlay() const;

    float titleAimYaw_{};
    float titleAimPitch_{};
    float titleIdleTime_{};
    bool performanceExpanded_{true};
    static constexpr std::size_t kPerfHistory = 4096;
    std::array<PerformanceSample, kPerfHistory> performanceSamples_{};
    std::size_t performanceSampleCount_{};
    std::size_t performanceWriteIndex_{};
};

} // namespace gridfall
