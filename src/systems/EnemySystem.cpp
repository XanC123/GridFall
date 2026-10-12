#include "gridfall/systems/EnemySystem.hpp"

#include <algorithm>
#include <cmath>

namespace gridfall {
namespace {
float pointSegmentDistanceSquared(Vec2 point, Vec2 a, Vec2 b) noexcept {
    const Vec2 ab = b - a;
    const float denominator = lengthSquared(ab);
    if (denominator <= 0.0001f) return distanceSquared(point, a);
    const Vec2 ap = point - a;
    const float t = clampf((ap.x * ab.x + ap.y * ab.y) / denominator, 0.0f, 1.0f);
    return distanceSquared(point, a + ab * t);
}

bool hasLineOfSight(const Level& level, Vec2 from, Vec2 to) {
    const Vec2 delta = to - from;
    const float distance = length(delta);
    const int samples = std::max(1, static_cast<int>(std::ceil(distance / (kTileSize * 0.35f))));
    for (int i = 1; i < samples; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(samples);
        const Vec2 point = from + delta * t;
        if (level.isSolid(level.tileAtWorld(point))) return false;
    }
    return true;
}

bool projectileHitsWall(const Level& level, const EnemyProjectile& projectile, Vec2 next) {
    const Vec2 segment = next - projectile.position;
    const float distance = length(segment);
    const int samples = std::max(1, static_cast<int>(std::ceil(distance / 5.0f)));
    for (int i = 1; i <= samples; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(samples);
        if (level.circleHitsSolid(projectile.position + segment * t, projectile.radius)) return true;
    }
    return false;
}
} // namespace

EnemySystem::EnemySystem() {
    // Sector 20 has 27 enemies; reserve headroom for their concurrent ranged shots.
    projectiles_.reserve(128);
    survivors_.reserve(128);
}

void EnemySystem::update(std::vector<Enemy>& enemies, Player& player, const Level& level, float dt) {
    for (Enemy& enemy : enemies) {
        if (!enemy.alive) continue;
        enemy.hitFlash = std::max(0.0f, enemy.hitFlash - dt);
        enemy.attackCooldown = std::max(0.0f, enemy.attackCooldown - dt);
        enemy.rangedCooldown = std::max(0.0f, enemy.rangedCooldown - dt);

        const Vec2 delta = player.position - enemy.position;
        const float d2 = lengthSquared(delta);
        const float distance = std::sqrt(std::max(0.0f, d2));
        const float detect = enemy.detectionRange;
        if (d2 > 14.0f * 14.0f && d2 < detect * detect) {
            const Vec2 direction = normalized(delta);
            const float closePressure = d2 < 150.0f * 150.0f ? 1.22f : 1.0f;
            const Vec2 step = direction * (enemy.speed * enemy.aggression * closePressure * dt);
            Vec2 candidate = enemy.position;
            candidate.x += step.x;
            if (!level.circleHitsSolid(candidate, enemy.radius)) enemy.position.x = candidate.x;
            candidate = enemy.position;
            candidate.y += step.y;
            if (!level.circleHitsSolid(candidate, enemy.radius)) enemy.position.y = candidate.y;
        }

        // Hostiles use a ranged attack when they have a clear lane. Every sector
        // makes them quicker and more dangerous, while cooldowns remain staggered.
        const float minRange = enemy.radius + player.radius + 45.0f;
        if (distance >= minRange && distance <= enemy.rangedAttackRange &&
            enemy.rangedCooldown <= 0.0f && hasLineOfSight(level, enemy.position, player.position)) {
            Vec2 direction = normalized(delta);
            const int spreadStep = static_cast<int>(enemy.id % 3u) - 1;
            const float spread = static_cast<float>(spreadStep) * (0.018f + (enemy.aggression - 1.0f) * 0.045f);
            const float cs = std::cos(spread), sn = std::sin(spread);
            direction = {direction.x * cs - direction.y * sn,
                         direction.x * sn + direction.y * cs};
            EnemyProjectile bullet{};
            bullet.sourceId = enemy.id;
            bullet.position = enemy.position + direction * (enemy.radius + 3.0f);
            bullet.previous = bullet.position;
            bullet.velocity = direction * enemy.projectileSpeed;
            bullet.damage = enemy.projectileDamage;
            bullet.radius = 3.2f;
            bullet.life = 2.2f;
            projectiles_.push_back(bullet);
            enemy.rangedCooldown = enemy.rangedAttackInterval;
        }

        const float reach = enemy.radius + player.radius + 4.0f;
        if (distanceSquared(enemy.position, player.position) <= reach * reach && enemy.attackCooldown <= 0.0f) {
            const float mitigation = clampf(player.damageResistance, 0.0f, 0.75f);
            player.health = std::max(0.0f, player.health - enemy.contactDamage * (1.0f - mitigation));
            player.damageFlash = 0.20f;
            enemy.attackCooldown = enemy.attackCooldownDuration;
        }
    }

    // Enemy bolts are advanced on the simulation tick, swept against walls to
    // avoid tunnelling, and swept against the player to avoid frame-rate misses.
    survivors_.clear();
    if (survivors_.capacity() < projectiles_.size()) survivors_.reserve(projectiles_.size());
    for (EnemyProjectile bullet : projectiles_) {
        bullet.life -= dt;
        if (bullet.life <= 0.0f) continue;
        bullet.previous = bullet.position;
        const Vec2 next = bullet.position + bullet.velocity * dt;
        if (projectileHitsWall(level, bullet, next)) continue;
        const float hitRadius = player.radius + bullet.radius;
        if (pointSegmentDistanceSquared(player.position, bullet.position, next) <= hitRadius * hitRadius) {
            const float mitigation = clampf(player.damageResistance, 0.0f, 0.75f);
            player.health = std::max(0.0f, player.health - bullet.damage * (1.0f - mitigation));
            player.damageFlash = 0.22f;
            continue;
        }
        bullet.position = next;
        survivors_.push_back(bullet);
    }
    projectiles_.swap(survivors_);
}

void EnemySystem::configureForSector(Enemy& enemy, int zeroBasedSector, float spawnHealthScale) noexcept {
    const float sector = static_cast<float>(std::clamp(zeroBasedSector, 0, kMaxLevels - 1));
    const float safeHealthScale = std::max(0.5f, spawnHealthScale);
    enemy.maxHealth = (42.0f + sector * 3.2f) * safeHealthScale;
    enemy.health = enemy.maxHealth;
    enemy.speed = 48.0f + sector * 2.0f;
    enemy.aggression = 1.0f + sector * 0.032f;
    enemy.detectionRange = 450.0f + sector * 24.0f;
    enemy.contactDamage = 7.5f + sector * 0.75f;
    enemy.attackCooldownDuration = std::max(0.42f, 0.92f - sector * 0.024f);
    enemy.rangedAttackInterval = std::max(0.95f, 2.05f - sector * 0.052f);
    enemy.rangedCooldown = 0.35f + static_cast<float>(enemy.id % 5u) * 0.21f;
    enemy.projectileSpeed = 215.0f + sector * 5.5f;
    enemy.projectileDamage = 5.0f + sector * 0.42f;
    enemy.rangedAttackRange = 360.0f + sector * 17.0f;
}

int EnemySystem::aliveCount(const std::vector<Enemy>& enemies) noexcept {
    return static_cast<int>(std::count_if(enemies.begin(), enemies.end(), [](const Enemy& enemy) {
        return enemy.alive;
    }));
}

} // namespace gridfall
