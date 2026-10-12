#include "gridfall/systems/CombatSystem.hpp"
#include <algorithm>
#include <cmath>

namespace gridfall {
namespace {
constexpr std::array<WeaponSpec, 3> kWeaponSpecs{{
    {"VECTOR-9", 24, 9.0f, 10.0f, 710.0f, 2.0f, 0, {255, 205, 112, 255}, 1.05f},
    {"RAIL PIERCER", 12, 19.0f, 3.1f, 650.0f, 2.8f, 2, {105, 226, 255, 255}, 1.35f},
    {"LONGSHOT", 6, 34.0f, 1.75f, 900.0f, 2.4f, 0, {255, 126, 103, 255}, 1.6f}
}};

float pointSegmentDistanceSquared(Vec2 point, Vec2 a, Vec2 b) {
    const Vec2 ab = b - a;
    const float denom = lengthSquared(ab);
    if (denom <= 0.0001f) return distanceSquared(point, a);
    const float t = clampf(((point.x - a.x) * ab.x + (point.y - a.y) * ab.y) / denom, 0.0f, 1.0f);
    const Vec2 closest = a + ab * t;
    return distanceSquared(point, closest);
}

bool alreadyHit(const Projectile& projectile, std::uint32_t id) {
    for (int i = 0; i < projectile.hitCount; ++i)
        if (projectile.hitEnemyIds[static_cast<std::size_t>(i)] == id) return true;
    return false;
}
} // namespace

CombatSystem::CombatSystem() {
    // Cover expected gameplay concurrency without reallocating during bursts.
    projectiles_.reserve(96);
    survivors_.reserve(96);
}

const WeaponSpec& CombatSystem::spec(AmmoType type) noexcept {
    return kWeaponSpecs[static_cast<std::size_t>(type)];
}

FireResult CombatSystem::tryFire(Player& player, Vec2 aimDirection, bool fireHeld, VfxSystem& vfx) {
    if (!fireHeld) return FireResult::NoInput;
    if (player.reloading) return FireResult::Reloading;
    WeaponState& state = player.activeWeapon();
    if (state.cooldown > 0.0f) return FireResult::CoolingDown;
    if (state.magazine <= 0) return FireResult::Empty;

    const AmmoType type = player.ammoType();
    const WeaponSpec& weapon = spec(type);
    const Vec2 direction = normalized(aimDirection);
    const Vec2 muzzle = player.position + direction * (player.radius + 8.0f);
    Projectile projectile{};
    projectile.position = muzzle;
    projectile.previous = muzzle;
    projectile.velocity = direction * weapon.projectileSpeed;
    projectile.radius = weapon.projectileRadius;
    projectile.damage = (weapon.damage + player.damageBonus) * player.damageMultiplier;
    projectile.life = 1.25f;
    projectile.pierceRemaining = weapon.pierceCount + (type == AmmoType::Piercing ? player.piercingBonus : 0);
    projectile.type = type;
    projectile.color = weapon.color;
    projectiles_.push_back(projectile);

    --state.magazine;
    state.cooldown = 1.0f / (weapon.shotsPerSecond * std::max(0.1f, player.fireRateMultiplier));
    vfx.muzzleBurst(muzzle, direction, type);
    return FireResult::Fired;
}

bool CombatSystem::requestReload(Player& player, VfxSystem& vfx) {
    if (player.reloading) return false;
    WeaponState& weaponState = player.activeWeapon();
    const WeaponSpec& weapon = spec(player.ammoType());
    if (weaponState.magazine >= weapon.magazineSize || weaponState.reserve <= 0) return false;
    player.reloading = true;
    player.reloadDuration = weapon.reloadSeconds * std::max(0.35f, player.reloadDurationMultiplier);
    player.reloadTimer = player.reloadDuration;
    vfx.dropMagazine(player.position + player.facing * 5.0f, player.facing);
    return true;
}

void CombatSystem::updateReload(Player& player, float dt) {
    for (WeaponState& weapon : player.weapons) weapon.cooldown = std::max(0.0f, weapon.cooldown - dt);
    if (!player.reloading) return;
    player.reloadTimer -= dt;
    if (player.reloadTimer > 0.0f) return;

    WeaponState& current = player.activeWeapon();
    const int capacity = spec(player.ammoType()).magazineSize;
    const int missing = std::max(0, capacity - current.magazine);
    const int transfer = std::min(missing, current.reserve);
    current.magazine += transfer;
    current.reserve -= transfer;
    player.reloading = false;
    player.reloadTimer = 0.0f;
}

int CombatSystem::updateProjectiles(Level& level, std::vector<Enemy>& enemies, VfxSystem& vfx, float dt) {
    int xpEarned = 0;
    survivors_.clear();
    if (survivors_.capacity() < projectiles_.size()) survivors_.reserve(projectiles_.size());

    for (Projectile projectile : projectiles_) {
        projectile.life -= dt;
        if (projectile.life <= 0.0f) continue;
        projectile.previous = projectile.position;
        const Vec2 next = projectile.position + projectile.velocity * dt;

        // Sweep in short pieces so fast bullets cannot tunnel through a wall.
        bool hitWall = false;
        const float segmentLength = length(next - projectile.position);
        const int samples = std::max(1, static_cast<int>(std::ceil(segmentLength / 6.0f)));
        for (int s = 1; s <= samples; ++s) {
            const float t = static_cast<float>(s) / static_cast<float>(samples);
            const Vec2 probe = projectile.position + (next - projectile.position) * t;
            if (level.circleHitsSolid(probe, projectile.radius)) { hitWall = true; break; }
        }
        if (hitWall) {
            vfx.hitBurst(projectile.position, {160, 180, 195, 255}, 3);
            continue;
        }
        projectile.position = next;
        bool stillAlive = true;
        for (Enemy& enemy : enemies) {
            if (!enemy.alive || alreadyHit(projectile, enemy.id)) continue;
            const float combinedRadius = enemy.radius + projectile.radius;
            if (pointSegmentDistanceSquared(enemy.position, projectile.previous, projectile.position) > combinedRadius * combinedRadius)
                continue;

            enemy.health -= projectile.damage;
            enemy.hitFlash = 0.12f;
            vfx.hitBurst(enemy.position, projectile.color, projectile.type == AmmoType::Piercing ? 5 : 8);
            if (enemy.health <= 0.0f) {
                enemy.alive = false;
                xpEarned += 35;
                vfx.hitBurst(enemy.position, {255, 100, 93, 255}, 18);
            }

            if (projectile.type != AmmoType::Piercing || projectile.pierceRemaining <= 0 ||
                projectile.hitCount >= static_cast<int>(projectile.hitEnemyIds.size())) {
                stillAlive = false;
                break;
            }
            projectile.hitEnemyIds[static_cast<std::size_t>(projectile.hitCount++)] = enemy.id;
            --projectile.pierceRemaining;
        }
        if (stillAlive) survivors_.push_back(projectile);
    }
    projectiles_.swap(survivors_);
    return xpEarned;
}

} // namespace gridfall
