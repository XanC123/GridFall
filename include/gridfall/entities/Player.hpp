#pragma once

#include "gridfall/core/Types.hpp"
#include "gridfall/entities/Inventory.hpp"
#include <array>

namespace gridfall {

struct WeaponState {
    int magazine{};
    int reserve{};
    float cooldown{};
};

struct Player {
    Vec2 position{};
    // Previous fixed-step state used for smooth render interpolation.
    Vec2 previousPosition{};
    Vec2 velocity{};
    Vec2 facing{1.0f, 0.0f};
    float radius{10.5f};
    float health{100.0f};
    float maxHealth{100.0f};
    float moveSpeed{190.0f};
    float damageBonus{0.0f};
    float damageMultiplier{1.0f};
    float damageResistance{0.0f};
    float regenPercentPerTick{0.035f};
    float reloadDurationMultiplier{1.0f};
    float fireRateMultiplier{1.0f};
    int piercingBonus{0};
    std::array<bool, kSkillCount> unlockedSkills{};
    int weaponIndex{0};
    std::array<WeaponState, 3> weapons{{{24, 144, 0.0f}, {12, 72, 0.0f}, {6, 48, 0.0f}}};
    bool reloading{false};
    float reloadTimer{0.0f};
    float reloadDuration{1.15f};
    float animationTime{0.0f};
    Inventory inventory{};
    int xp{0};
    int xpToNext{100};
    int rank{1};
    int skillPoints{0};
    float regenRemaining{0.0f};
    float regenTickTimer{0.0f};
    float poisonTickTimer{0.0f};
    bool poisoned{false};
    float damageFlash{0.0f};
    float hazardCooldown{0.0f};

    [[nodiscard]] AmmoType ammoType() const noexcept { return static_cast<AmmoType>(weaponIndex); }
    [[nodiscard]] WeaponState& activeWeapon() noexcept { return weapons[static_cast<std::size_t>(weaponIndex)]; }
    [[nodiscard]] const WeaponState& activeWeapon() const noexcept { return weapons[static_cast<std::size_t>(weaponIndex)]; }
};

} // namespace gridfall
