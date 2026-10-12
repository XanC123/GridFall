#include "gridfall/systems/ProgressionSystem.hpp"
#include <algorithm>
#include <array>

namespace gridfall {
namespace {
constexpr std::array<SkillDefinition, kSkillCount> kSkills{{
    {SkillId::Bulwark, "BULWARK", "+25 max health and heal for 25.", "SURVIVAL", SkillId::Count, 1},
    {SkillId::DamageMitigation, "HARDENED", "Reduce incoming damage by 8%.", "SURVIVAL", SkillId::Bulwark, 1},
    {SkillId::VitalFlux, "VITAL FLUX", "Regeneration doses heal 5% max HP per tick.", "SURVIVAL", SkillId::DamageMitigation, 2},
    {SkillId::WeaponTuning, "WEAPON TUNING", "+4 base damage to every weapon.", "ASSAULT", SkillId::Count, 1},
    {SkillId::Overcharge, "OVERCHARGE", "Increase all weapon damage by 15%.", "ASSAULT", SkillId::WeaponTuning, 1},
    {SkillId::PenetrationCore, "PENETRATION CORE", "Piercing rounds pass through one extra target.", "ASSAULT", SkillId::Overcharge, 2},
    {SkillId::Lightweight, "LIGHTWEIGHT", "+15 movement speed.", "MOBILITY", SkillId::Count, 1},
    {SkillId::CombatDrills, "COMBAT DRILLS", "Reload 20% faster.", "MOBILITY", SkillId::Lightweight, 1},
    {SkillId::TriggerDiscipline, "TRIGGER DISCIPLINE", "Fire 18% faster with all weapons.", "MOBILITY", SkillId::CombatDrills, 2}
}};

constexpr std::size_t skillIndex(SkillId id) noexcept {
    return static_cast<std::size_t>(id);
}
}

void ProgressionSystem::awardXp(Player& player, int amount) noexcept {
    if (amount <= 0) return;
    player.xp += amount;
    while (player.xp >= player.xpToNext) {
        player.xp -= player.xpToNext;
        ++player.rank;
        ++player.skillPoints;
        player.xpToNext = 100 + (player.rank - 1) * 35;
        player.health = std::min(player.maxHealth, player.health + 20.0f);
    }
}

bool ProgressionSystem::spendHealthPoint(Player& player) noexcept {
    if (player.skillPoints <= 0) return false;
    --player.skillPoints;
    player.maxHealth += 15.0f;
    player.health = std::min(player.maxHealth, player.health + 15.0f);
    return true;
}

bool ProgressionSystem::spendDamagePoint(Player& player) noexcept {
    if (player.skillPoints <= 0) return false;
    --player.skillPoints;
    player.damageBonus += 4.0f;
    return true;
}

bool ProgressionSystem::spendSpeedPoint(Player& player) noexcept {
    if (player.skillPoints <= 0) return false;
    --player.skillPoints;
    player.moveSpeed += 12.0f;
    return true;
}

const SkillDefinition& ProgressionSystem::skillDefinition(SkillId id) noexcept {
    const std::size_t index = skillIndex(id);
    return kSkills[index < kSkills.size() ? index : 0];
}

bool ProgressionSystem::hasSkill(const Player& player, SkillId id) noexcept {
    const std::size_t index = skillIndex(id);
    return index < player.unlockedSkills.size() && player.unlockedSkills[index];
}

bool ProgressionSystem::canPurchaseSkill(const Player& player, SkillId id) noexcept {
    const std::size_t index = skillIndex(id);
    if (index >= kSkills.size() || hasSkill(player, id)) return false;
    const SkillDefinition& definition = kSkills[index];
    if (player.skillPoints < definition.cost) return false;
    return definition.prerequisite == SkillId::Count || hasSkill(player, definition.prerequisite);
}

bool ProgressionSystem::purchaseSkill(Player& player, SkillId id) noexcept {
    if (!canPurchaseSkill(player, id)) return false;

    const SkillDefinition& definition = skillDefinition(id);
    player.skillPoints -= definition.cost;
    player.unlockedSkills[skillIndex(id)] = true;

    switch (id) {
        case SkillId::Bulwark:
            player.maxHealth += 25.0f;
            player.health = std::min(player.maxHealth, player.health + 25.0f);
            break;
        case SkillId::DamageMitigation:
            player.damageResistance = std::min(0.60f, player.damageResistance + 0.08f);
            break;
        case SkillId::VitalFlux:
            player.regenPercentPerTick = 0.05f;
            break;
        case SkillId::WeaponTuning:
            player.damageBonus += 4.0f;
            break;
        case SkillId::Overcharge:
            player.damageMultiplier *= 1.15f;
            break;
        case SkillId::PenetrationCore:
            ++player.piercingBonus;
            break;
        case SkillId::Lightweight:
            player.moveSpeed += 15.0f;
            break;
        case SkillId::CombatDrills:
            player.reloadDurationMultiplier *= 0.80f;
            break;
        case SkillId::TriggerDiscipline:
            player.fireRateMultiplier *= 1.18f;
            break;
        case SkillId::Count:
            return false;
    }
    return true;
}

} // namespace gridfall
