#pragma once

#include "gridfall/core/Types.hpp"
#include <cstdint>

namespace gridfall {

struct Enemy {
    std::uint32_t id{};
    Vec2 position{};
    // Previous fixed-step position; rendering interpolates between these states.
    Vec2 previousPosition{};
    Vec2 velocity{};
    float radius{10.0f};
    float health{42.0f};
    float maxHealth{42.0f};
    float speed{50.0f};
    float aggression{1.0f};
    float detectionRange{480.0f};
    float contactDamage{8.0f};
    float attackCooldownDuration{0.85f};
    float attackCooldown{0.0f};
    // Ranged combat is independently timed from the contact attack.
    float rangedAttackInterval{2.0f};
    float rangedCooldown{0.0f};
    float projectileSpeed{235.0f};
    float projectileDamage{5.0f};
    float rangedAttackRange{380.0f};
    float hitFlash{0.0f};
    bool alive{true};
};

struct EnemyProjectile {
    std::uint32_t sourceId{};
    Vec2 position{};
    Vec2 previous{};
    Vec2 velocity{};
    float radius{3.0f};
    float damage{5.0f};
    float life{2.4f};
};

} // namespace gridfall
