#include "gridfall/systems/SaveSystem.hpp"

#include "gridfall/core/Types.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <string>

namespace gridfall {
namespace {
constexpr int kSaveVersion = 1;
constexpr const char* kSaveMagic = "GRIDFALL_SAVE";

bool fail(std::string* error, const char* message) {
    if (error != nullptr) *error = message;
    return false;
}

bool finitePlayer(const Player& player) {
    return std::isfinite(player.health) && std::isfinite(player.maxHealth) &&
           std::isfinite(player.moveSpeed) && std::isfinite(player.damageBonus) &&
           std::isfinite(player.damageMultiplier) && std::isfinite(player.damageResistance) &&
           std::isfinite(player.regenPercentPerTick) &&
           std::isfinite(player.reloadDurationMultiplier) &&
           std::isfinite(player.fireRateMultiplier);
}
} // namespace

bool SaveSystem::exists(const std::string& path) noexcept {
    std::ifstream input(path);
    return input.good();
}

bool SaveSystem::save(const SaveData& data, std::string* error, const std::string& path) {
    if (data.currentLevel < 0 || data.currentLevel >= kMaxLevels ||
        data.unlockedLevel < 1 || data.unlockedLevel > kMaxLevels || !finitePlayer(data.player)) {
        return fail(error, "Save data is outside valid ranges.");
    }

    // Write to a temporary file first, then replace the save. A crash during
    // serialization therefore cannot leave a partially written primary save.
    const std::string tempPath = path + ".tmp";
    std::ofstream output(tempPath, std::ios::trunc);
    if (!output) return fail(error, "Could not create the temporary save file.");

    const Player& p = data.player;
    output << kSaveMagic << ' ' << kSaveVersion << '\n';
    output << data.currentLevel << ' ' << data.unlockedLevel << '\n';
    output << std::setprecision(9)
           << p.health << ' ' << p.maxHealth << ' ' << p.moveSpeed << ' '
           << p.damageBonus << ' ' << p.damageMultiplier << ' ' << p.damageResistance << ' '
           << p.regenPercentPerTick << ' ' << p.reloadDurationMultiplier << ' '
           << p.fireRateMultiplier << ' ' << p.piercingBonus << ' ' << p.weaponIndex << '\n';
    output << p.xp << ' ' << p.xpToNext << ' ' << p.rank << ' ' << p.skillPoints << '\n';
    output << p.inventory.healthPotions << ' ' << p.inventory.regenPotions << '\n';
    output << p.regenRemaining << '\n';
    for (const bool unlocked : p.unlockedSkills) output << (unlocked ? 1 : 0) << ' ';
    output << '\n';
    for (const WeaponState& weapon : p.weapons)
        output << weapon.magazine << ' ' << weapon.reserve << '\n';
    output.flush();
    if (!output) {
        output.close();
        return fail(error, "Writing the temporary save file failed.");
    }
    output.close();

    // On POSIX, rename atomically replaces an existing destination file.
    if (std::rename(tempPath.c_str(), path.c_str()) != 0) {
        std::remove(tempPath.c_str());
        return fail(error, "Could not replace the previous save file.");
    }
    if (error != nullptr) error->clear();
    return true;
}

bool SaveSystem::load(SaveData& out, std::string* error, const std::string& path) {
    std::ifstream input(path);
    if (!input) return fail(error, "No save file was found.");

    std::string magic;
    int version = 0;
    if (!(input >> magic >> version) || magic != kSaveMagic || version != kSaveVersion)
        return fail(error, "Save file header or version is unsupported.");

    SaveData candidate{};
    Player& p = candidate.player;
    if (!(input >> candidate.currentLevel >> candidate.unlockedLevel))
        return fail(error, "Save file is missing sector progress.");
    if (!(input >> p.health >> p.maxHealth >> p.moveSpeed >> p.damageBonus
                >> p.damageMultiplier >> p.damageResistance >> p.regenPercentPerTick
                >> p.reloadDurationMultiplier >> p.fireRateMultiplier >> p.piercingBonus
                >> p.weaponIndex))
        return fail(error, "Save file is missing player statistics.");
    if (!(input >> p.xp >> p.xpToNext >> p.rank >> p.skillPoints))
        return fail(error, "Save file is missing rank progress.");
    if (!(input >> p.inventory.healthPotions >> p.inventory.regenPotions >> p.regenRemaining))
        return fail(error, "Save file is missing inventory state.");

    for (bool& unlocked : p.unlockedSkills) {
        int flag = 0;
        if (!(input >> flag) || (flag != 0 && flag != 1))
            return fail(error, "Save file contains invalid skill state.");
        unlocked = flag == 1;
    }
    for (WeaponState& weapon : p.weapons) {
        if (!(input >> weapon.magazine >> weapon.reserve))
            return fail(error, "Save file is missing weapon ammunition.");
        weapon.magazine = std::clamp(weapon.magazine, 0, 999);
        weapon.reserve = std::clamp(weapon.reserve, 0, 9999);
        weapon.cooldown = 0.0f;
    }

    if (candidate.currentLevel < 0 || candidate.currentLevel >= kMaxLevels ||
        candidate.unlockedLevel < 1 || candidate.unlockedLevel > kMaxLevels ||
        !finitePlayer(p) || p.maxHealth < 1.0f || p.maxHealth > 5000.0f ||
        p.health < 0.0f || p.health > p.maxHealth || p.moveSpeed < 1.0f || p.moveSpeed > 2000.0f ||
        p.damageMultiplier < 0.1f || p.damageMultiplier > 50.0f ||
        p.damageResistance < 0.0f || p.damageResistance > 0.75f ||
        p.weaponIndex < 0 || p.weaponIndex >= static_cast<int>(p.weapons.size()) ||
        p.xp < 0 || p.xp > 100000000 || p.xpToNext < 1 || p.rank < 1 || p.skillPoints < 0 ||
        p.inventory.healthPotions < 0 || p.inventory.regenPotions < 0 ||
        p.regenPercentPerTick < 0.0f || p.regenPercentPerTick > 0.5f ||
        p.reloadDurationMultiplier < 0.1f || p.reloadDurationMultiplier > 5.0f ||
        p.fireRateMultiplier < 0.1f || p.fireRateMultiplier > 10.0f || p.piercingBonus < 0 || p.piercingBonus > 20)
        return fail(error, "Save file contains values outside safe ranges.");

    // Transient animation/input state is deliberately reset on load.
    p.position = {};
    p.previousPosition = {};
    p.velocity = {};
    p.facing = {1.0f, 0.0f};
    p.reloading = false;
    p.reloadTimer = 0.0f;
    p.regenTickTimer = 0.0f;
    p.poisoned = false;
    p.poisonTickTimer = 0.0f;
    p.damageFlash = 0.0f;
    p.hazardCooldown = 0.0f;
    p.animationTime = 0.0f;

    out = candidate;
    if (error != nullptr) error->clear();
    return true;
}

} // namespace gridfall
