#pragma once
#include "gridfall/entities/Player.hpp"
#include <cstddef>

namespace gridfall {

struct SkillDefinition {
    SkillId id{};
    const char* name{};
    const char* description{};
    const char* branch{};
    SkillId prerequisite{SkillId::Count};
    int cost{1};
};

class ProgressionSystem {
public:
    static void awardXp(Player& player, int amount) noexcept;
    static bool spendHealthPoint(Player& player) noexcept;
    static bool spendDamagePoint(Player& player) noexcept;
    static bool spendSpeedPoint(Player& player) noexcept;

    [[nodiscard]] static const SkillDefinition& skillDefinition(SkillId id) noexcept;
    [[nodiscard]] static bool hasSkill(const Player& player, SkillId id) noexcept;
    [[nodiscard]] static bool canPurchaseSkill(const Player& player, SkillId id) noexcept;
    static bool purchaseSkill(Player& player, SkillId id) noexcept;
};

} // namespace gridfall
