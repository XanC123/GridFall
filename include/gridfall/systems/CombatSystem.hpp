#pragma once
#include "gridfall/entities/Enemy.hpp"
#include "gridfall/entities/Player.hpp"
#include "gridfall/systems/VfxSystem.hpp"
#include "gridfall/world/Level.hpp"
#include <vector>

namespace gridfall {

struct WeaponSpec {
    const char* name{};
    int magazineSize{};
    float damage{};
    float shotsPerSecond{};
    float projectileSpeed{};
    float projectileRadius{};
    int pierceCount{};
    Rgba color{};
    float reloadSeconds{1.15f};
};

struct Projectile {
    Vec2 position{};
    Vec2 previous{};
    Vec2 velocity{};
    float radius{2.0f};
    float damage{};
    float life{1.0f};
    int pierceRemaining{};
    AmmoType type{AmmoType::FastFire};
    std::array<std::uint32_t, 8> hitEnemyIds{};
    int hitCount{};
    Rgba color{};
};

enum class FireResult { NoInput, Fired, CoolingDown, Empty, Reloading };

class CombatSystem {
public:
    CombatSystem();
    static const WeaponSpec& spec(AmmoType type) noexcept;
    FireResult tryFire(Player& player, Vec2 aimDirection, bool fireHeld, VfxSystem& vfx);
    bool requestReload(Player& player, VfxSystem& vfx);
    void updateReload(Player& player, float dt);
    int updateProjectiles(Level& level, std::vector<Enemy>& enemies, VfxSystem& vfx, float dt);
    [[nodiscard]] const std::vector<Projectile>& projectiles() const noexcept { return projectiles_; }
    void clear() noexcept { projectiles_.clear(); survivors_.clear(); }

private:
    // Reuse scratch storage every simulation tick to avoid allocator churn.
    std::vector<Projectile> projectiles_;
    std::vector<Projectile> survivors_;
};

} // namespace gridfall
