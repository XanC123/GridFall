#pragma once
#include "gridfall/entities/Enemy.hpp"
#include "gridfall/entities/Player.hpp"
#include "gridfall/world/Level.hpp"
#include <vector>

namespace gridfall {

class EnemySystem {
public:
    EnemySystem();
    void update(std::vector<Enemy>& enemies, Player& player, const Level& level, float dt);
    static void configureForSector(Enemy& enemy, int zeroBasedSector, float spawnHealthScale = 1.0f) noexcept;
    [[nodiscard]] static int aliveCount(const std::vector<Enemy>& enemies) noexcept;
    [[nodiscard]] const std::vector<EnemyProjectile>& projectiles() const noexcept { return projectiles_; }
    void clearProjectiles() noexcept { projectiles_.clear(); survivors_.clear(); }

private:
    // Persistent scratch buffer prevents per-tick allocate/free cycles.
    std::vector<EnemyProjectile> projectiles_;
    std::vector<EnemyProjectile> survivors_;
};

} // namespace gridfall
