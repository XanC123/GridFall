#include "gridfall/core/Controls.hpp"
#include "gridfall/entities/Inventory.hpp"
#include "gridfall/entities/Player.hpp"
#include "gridfall/systems/CombatSystem.hpp"
#include "gridfall/systems/EnemySystem.hpp"
#include "gridfall/systems/ProgressionSystem.hpp"
#include "gridfall/systems/SaveSystem.hpp"
#include "gridfall/world/Level.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <cmath>
#include <iostream>
#include <queue>
#include <utility>
#include <vector>

using namespace gridfall;

// Unlike assert(), CHECK remains enabled in Release builds so tests both execute
// their validations and use the values under test when NDEBUG is defined.
#define CHECK(condition) do { \
    if (!(condition)) { \
        std::cerr << "CHECK failed at " << __FILE__ << ':' << __LINE__ \
                  << ": " #condition << '\n'; \
        return EXIT_FAILURE; \
    } \
} while (false)

static bool sameColor(Rgba a, Rgba b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static bool hasPathToExit(const Level& level) {
    const Vec2 start = level.playerSpawn();
    const Vec2 goal = level.exitPosition();
    const int sx = static_cast<int>(start.x / kTileSize), sy = static_cast<int>(start.y / kTileSize);
    const int gx = static_cast<int>(goal.x / kTileSize), gy = static_cast<int>(goal.y / kTileSize);
    std::array<bool, kMapWidth * kMapHeight> visited{};
    std::queue<std::pair<int, int>> pending;
    pending.push({sx, sy});
    visited[static_cast<std::size_t>(sy * kMapWidth + sx)] = true;
    constexpr int dx[4]{1, -1, 0, 0};
    constexpr int dy[4]{0, 0, 1, -1};
    while (!pending.empty()) {
        const auto [x, y] = pending.front();
        pending.pop();
        if (x == gx && y == gy) return true;
        for (int d = 0; d < 4; ++d) {
            const int nx = x + dx[d], ny = y + dy[d];
            if (!level.isInside(nx, ny) || level.isSolid(level.tileAt(nx, ny))) continue;
            const std::size_t idx = static_cast<std::size_t>(ny * kMapWidth + nx);
            if (visited[idx]) continue;
            visited[idx] = true;
            pending.push({nx, ny});
        }
    }
    return false;
}

static bool hasPathToPosition(const Level& level, Vec2 start, Vec2 goal) {
    const int sx = static_cast<int>(start.x / kTileSize), sy = static_cast<int>(start.y / kTileSize);
    const int gx = static_cast<int>(goal.x / kTileSize), gy = static_cast<int>(goal.y / kTileSize);
    std::array<bool, kMapWidth * kMapHeight> visited{};
    std::queue<std::pair<int, int>> pending;
    pending.push({sx, sy});
    visited[static_cast<std::size_t>(sy * kMapWidth + sx)] = true;
    constexpr int dx[4]{1, -1, 0, 0};
    constexpr int dy[4]{0, 0, 1, -1};
    while (!pending.empty()) {
        const auto [x, y] = pending.front();
        pending.pop();
        if (x == gx && y == gy) return true;
        for (int direction = 0; direction < 4; ++direction) {
            const int nx = x + dx[direction], ny = y + dy[direction];
            if (!level.isInside(nx, ny) || level.isSolid(level.tileAt(nx, ny))) continue;
            const std::size_t index = static_cast<std::size_t>(ny * kMapWidth + nx);
            if (visited[index]) continue;
            visited[index] = true;
            pending.push({nx, ny});
        }
    }
    return false;
}

int main() {
    // Mouse backend integration and directional cases live in dedicated mouse tests.

    // Camera-relative movement tests ensure WASD follows view direction and diagonals
    // don't gain speed. These are the player-world movement vector checks.
    Vec2 movement = cameraRelativeMovement({1.0f, 0.0f}, 0.0f, 1.0f);
    CHECK(std::fabs(movement.x - 1.0f) < 0.001f && std::fabs(movement.y) < 0.001f);
    movement = cameraRelativeMovement({1.0f, 0.0f}, 1.0f, 0.0f);
    CHECK(std::fabs(movement.x) < 0.001f && std::fabs(movement.y - 1.0f) < 0.001f);
    movement = cameraRelativeMovement({0.0f, 1.0f}, 0.0f, 1.0f);
    CHECK(std::fabs(movement.x) < 0.001f && std::fabs(movement.y - 1.0f) < 0.001f);
    movement = cameraRelativeMovement({0.0f, 1.0f}, 1.0f, 0.0f);
    CHECK(std::fabs(movement.x + 1.0f) < 0.001f && std::fabs(movement.y) < 0.001f);
    movement = cameraRelativeMovement({1.0f, 0.0f}, 1.0f, 1.0f);
    CHECK(std::fabs(length(movement) - 1.0f) < 0.001f);
    movement = cameraRelativeMovement({1.0f, 0.0f}, 0.0f, 0.0f);
    CHECK(std::fabs(length(movement)) < 0.001f);

    Level level;
    std::vector<std::array<Tile, kMapWidth * kMapHeight>> uniqueLayouts;
    std::vector<Rgba> uniqueAccents;
    for (int i = 0; i < kMaxLevels; ++i) {
        level.load(i);
        CHECK(level.index() == i);
        CHECK(level.tileAtWorld(level.exitPosition()) == Tile::Exit);
        CHECK(std::none_of(uniqueAccents.begin(), uniqueAccents.end(), [&](Rgba color) {
            return sameColor(color, level.palette().accent);
        }));
        uniqueAccents.push_back(level.palette().accent);
        if (i > 0) {
            Level previous;
            previous.load(i - 1);
            CHECK(!sameColor(level.palette().accent, previous.palette().accent));
            CHECK(!sameColor(level.palette().floor, previous.palette().floor));
        }
        CHECK(level.tileAt(0, 0) == Tile::Wall);
        CHECK(level.enemySpawns().size() == static_cast<std::size_t>(std::min(30, 8 + i)));
        CHECK(hasPathToExit(level));
        CHECK(level.tileAt(6, 2) == Tile::AmmoLight);
        CHECK(level.tileAt(12, 2) == Tile::AmmoPiercing);
        CHECK(level.tileAt(17, 2) == Tile::AmmoHeavy);
        CHECK(level.tileAt(18, 10) == Tile::SideDoor);
        CHECK(level.tileAt(18, 13) == Tile::Cache);
        CHECK(level.tileAt(17, 13) == Tile::AmmoCache);
        std::array<Tile, kMapWidth * kMapHeight> layout{};
        for (int y = 0; y < kMapHeight; ++y)
            for (int x = 0; x < kMapWidth; ++x)
                layout[static_cast<std::size_t>(y * kMapWidth + x)] = level.tileAt(x, y);
        CHECK(std::find(uniqueLayouts.begin(), uniqueLayouts.end(), layout) == uniqueLayouts.end());
        uniqueLayouts.push_back(layout);
        for (const EnemySpawn& spawn : level.enemySpawns()) {
            CHECK(!level.isSolid(level.tileAtWorld(spawn.position)));
            CHECK(level.tileAtWorld(spawn.position) == Tile::Floor);
        }
    }

    level.load(0);
    CHECK(level.tileAt(11, 4) == Tile::SecretWall);
    const InteractionResult opened = level.interactNear({10.5f * kTileSize, 4.5f * kTileSize});
    CHECK(opened.changed && opened.secret);
    CHECK(level.tileAt(11, 4) == Tile::Floor);

    // The side chamber is enclosed and reachable through a real side door.
    level.load(0);
    CHECK(level.tileAt(18, 10) == Tile::SideDoor);
    CHECK(level.tileAt(18, 13) == Tile::Cache);
    CHECK(level.tileAt(17, 13) == Tile::AmmoCache);
    CHECK(!hasPathToPosition(level, level.playerSpawn(), {18.5f * kTileSize, 13.5f * kTileSize}));
    const InteractionResult roomDoor = level.interactNear({18.5f * kTileSize, 9.5f * kTileSize});
    CHECK(roomDoor.changed && !roomDoor.secret && !roomDoor.cache && !roomDoor.ammoCache);
    CHECK(level.tileAt(18, 10) == Tile::Floor);
    CHECK(hasPathToPosition(level, level.playerSpawn(), {18.5f * kTileSize, 13.5f * kTileSize}));
    const InteractionResult openChest = level.interactNear({18.5f * kTileSize, 12.5f * kTileSize});
    CHECK(openChest.changed && openChest.cache);
    CHECK(level.tileAt(18, 13) == Tile::Floor);
    const InteractionResult openAmmo = level.interactNear({16.5f * kTileSize, 13.5f * kTileSize});
    CHECK(openAmmo.changed && openAmmo.ammoCache);
    CHECK(level.tileAt(17, 13) == Tile::Floor);

    Player player{};
    ProgressionSystem::awardXp(player, 100);
    CHECK(player.rank == 2 && player.skillPoints == 1 && player.xp == 0);
    CHECK(ProgressionSystem::spendDamagePoint(player));
    CHECK(player.skillPoints == 0 && std::fabs(player.damageBonus - 4.0f) < 0.001f);
    CHECK(!ProgressionSystem::spendSpeedPoint(player));

    // Enemy pressure grows through all 20 sectors even after enemy count caps out.
    Enemy earlyEnemy{};
    Enemy lateEnemy{};
    EnemySystem::configureForSector(earlyEnemy, 0);
    EnemySystem::configureForSector(lateEnemy, kMaxLevels - 1, 1.2f);
    CHECK(lateEnemy.maxHealth > earlyEnemy.maxHealth);
    CHECK(lateEnemy.speed > earlyEnemy.speed);
    CHECK(lateEnemy.aggression > earlyEnemy.aggression);
    CHECK(lateEnemy.detectionRange > earlyEnemy.detectionRange);
    CHECK(lateEnemy.contactDamage > earlyEnemy.contactDamage);
    CHECK(lateEnemy.attackCooldownDuration < earlyEnemy.attackCooldownDuration);

    // Full skill-tree nodes have costs/prerequisites and immediately alter live stats.
    Player skilled{};
    ProgressionSystem::awardXp(skilled, 100);
    CHECK(ProgressionSystem::canPurchaseSkill(skilled, SkillId::Bulwark));
    CHECK(!ProgressionSystem::canPurchaseSkill(skilled, SkillId::DamageMitigation));
    CHECK(ProgressionSystem::purchaseSkill(skilled, SkillId::Bulwark));
    CHECK(skilled.maxHealth == 125.0f && skilled.health == 125.0f);
    ProgressionSystem::awardXp(skilled, 135);
    CHECK(ProgressionSystem::purchaseSkill(skilled, SkillId::DamageMitigation));
    CHECK(std::fabs(skilled.damageResistance - 0.08f) < 0.001f);
    ProgressionSystem::awardXp(skilled, 170);
    CHECK(!ProgressionSystem::canPurchaseSkill(skilled, SkillId::VitalFlux)); // costs two points
    ProgressionSystem::awardXp(skilled, 205);
    CHECK(ProgressionSystem::purchaseSkill(skilled, SkillId::VitalFlux));
    CHECK(std::fabs(skilled.regenPercentPerTick - 0.05f) < 0.001f);
    CHECK(ProgressionSystem::hasSkill(skilled, SkillId::VitalFlux));

    VfxSystem vfx;
    Player assault{};
    assault.skillPoints = 4;
    CHECK(ProgressionSystem::purchaseSkill(assault, SkillId::WeaponTuning));
    CHECK(ProgressionSystem::purchaseSkill(assault, SkillId::Overcharge));
    CHECK(ProgressionSystem::purchaseSkill(assault, SkillId::PenetrationCore));
    CHECK(std::fabs(assault.damageBonus - 4.0f) < 0.001f);
    CHECK(std::fabs(assault.damageMultiplier - 1.15f) < 0.001f);
    CHECK(assault.piercingBonus == 1 && assault.skillPoints == 0);
    assault.weaponIndex = 1;
    CombatSystem upgradedCombat;
    CHECK(upgradedCombat.tryFire(assault, {1.0f, 0.0f}, true, vfx) == FireResult::Fired);
    CHECK(upgradedCombat.projectiles().front().pierceRemaining == 3);
    CHECK(std::fabs(upgradedCombat.projectiles().front().damage - 26.45f) < 0.01f);

    Player mobility{};
    mobility.skillPoints = 4;
    const float baseSpeed = mobility.moveSpeed;
    CHECK(ProgressionSystem::purchaseSkill(mobility, SkillId::Lightweight));
    CHECK(ProgressionSystem::purchaseSkill(mobility, SkillId::CombatDrills));
    CHECK(ProgressionSystem::purchaseSkill(mobility, SkillId::TriggerDiscipline));
    CHECK(std::fabs(mobility.moveSpeed - (baseSpeed + 15.0f)) < 0.001f);
    CHECK(std::fabs(mobility.reloadDurationMultiplier - 0.80f) < 0.001f);
    CHECK(std::fabs(mobility.fireRateMultiplier - 1.18f) < 0.001f);
    mobility.weapons[0].magazine = 0;
    CombatSystem mobilityCombat;
    CHECK(mobilityCombat.requestReload(mobility, vfx));
    CHECK(std::fabs(mobility.reloadDuration - CombatSystem::spec(AmmoType::FastFire).reloadSeconds * 0.80f) < 0.001f);
    mobility.reloading = false;
    mobility.weapons[0].magazine = 24;
    CHECK(mobilityCombat.tryFire(mobility, {1.0f, 0.0f}, true, vfx) == FireResult::Fired);
    CHECK(std::fabs(mobility.weapons[0].cooldown - 1.0f / (10.0f * 1.18f)) < 0.001f);

    // Damage mitigation affects live enemy contact damage, not just the UI description.
    Level arena;
    arena.load(0);
    Player unarmored{};
    Player armored{};
    unarmored.position = armored.position = {200.0f, 200.0f};
    armored.damageResistance = 0.08f;
    Enemy hitter{};
    hitter.position = unarmored.position;
    hitter.speed = 0.0f;
    hitter.contactDamage = 10.0f;
    hitter.attackCooldownDuration = 1.0f;
    std::vector<Enemy> hitterA{hitter};
    std::vector<Enemy> hitterB{hitter};
    EnemySystem enemySystem;
    enemySystem.update(hitterA, unarmored, arena, 1.0f / 60.0f);
    enemySystem.update(hitterB, armored, arena, 1.0f / 60.0f);
    CHECK(std::fabs(unarmored.health - 90.0f) < 0.001f);
    CHECK(std::fabs(armored.health - 90.8f) < 0.001f);

    // A ranged enemy fires and its projectile can damage the player across a clear lane.
    Level firingLane;
    firingLane.load(0);
    Player shooterTarget{};
    shooterTarget.position = {80.0f, 80.0f};
    Enemy ranged{};
    ranged.id = 445;
    ranged.position = {200.0f, 80.0f};
    ranged.previousPosition = ranged.position;
    EnemySystem rangedSystem;
    EnemySystem::configureForSector(ranged, 0);
    ranged.rangedCooldown = 0.0f;
    ranged.speed = 0.0f;
    ranged.rangedAttackInterval = 100.0f;
    std::vector<Enemy> rangedEnemies{ranged};
    const float startingHealth = shooterTarget.health;
    rangedSystem.update(rangedEnemies, shooterTarget, firingLane, 1.0f / 60.0f);
    CHECK(!rangedSystem.projectiles().empty());
    rangedEnemies[0].rangedCooldown = 100.0f;
    for (int i = 0; i < 80 && shooterTarget.health == startingHealth; ++i)
        rangedSystem.update(rangedEnemies, shooterTarget, firingLane, 1.0f / 60.0f);
    CHECK(shooterTarget.health < startingHealth);

    Inventory inventory{};
    CHECK(inventory.consumeHealthPotion());
    CHECK(inventory.healthPotions == 1);
    inventory.addRegenPotion(2);
    CHECK(inventory.consumeRegenPotion());
    CHECK(inventory.regenPotions == 2);

    CombatSystem combat;
    player.position = {80.0f, 80.0f};
    player.facing = {1.0f, 0.0f};
    const auto result = combat.tryFire(player, player.facing, true, vfx);
    CHECK(result == FireResult::Fired);
    CHECK(player.activeWeapon().magazine == 23);
    CHECK(combat.projectiles().size() == 1);
    CHECK(combat.requestReload(player, vfx));
    CHECK(player.reloading && !vfx.magazines().empty());
    for (int i = 0; i < 100; ++i) combat.updateReload(player, 1.0f / 60.0f);
    CHECK(!player.reloading);
    CHECK(player.activeWeapon().magazine == 24);

    // A real projectile path should damage and eventually eliminate a nearby target.
    combat.clear();
    level.load(0);
    player.position = {80.0f, 80.0f};
    player.weaponIndex = 0;
    player.weapons[0] = {24, 144, 0.0f};
    Enemy target{};
    target.id = 9001;
    target.position = {155.0f, 80.0f};
    target.health = target.maxHealth = 18.0f;
    std::vector<Enemy> targets{target};
    int earned = 0;
    for (int i = 0; i < 120 && targets[0].alive; ++i) {
        combat.tryFire(player, {1.0f, 0.0f}, true, vfx);
        earned += combat.updateProjectiles(level, targets, vfx, 1.0f / 60.0f);
        combat.updateReload(player, 1.0f / 60.0f);
    }
    CHECK(!targets[0].alive && earned == 35);

    // Save/load round-trip preserves campaign progression, skills, inventory and ammo.
    SaveData saveData{};
    saveData.currentLevel = 7;
    saveData.unlockedLevel = 9;
    saveData.player = skilled;
    saveData.player.weaponIndex = 2;
    saveData.player.weapons[2].magazine = 4;
    saveData.player.weapons[2].reserve = 37;
    saveData.player.inventory.healthPotions = 6;
    const std::filesystem::path savePath = std::filesystem::temp_directory_path() / "gridfall-core-test.save";
    std::string saveError;
    CHECK(SaveSystem::save(saveData, &saveError, savePath.string()));
    CHECK(SaveSystem::exists(savePath.string()));
    SaveData loaded{};
    CHECK(SaveSystem::load(loaded, &saveError, savePath.string()));
    CHECK(loaded.currentLevel == 7 && loaded.unlockedLevel == 9);
    CHECK(loaded.player.rank == saveData.player.rank);
    CHECK(loaded.player.unlockedSkills == saveData.player.unlockedSkills);
    CHECK(loaded.player.inventory.healthPotions == 6);
    CHECK(loaded.player.weapons[2].magazine == 4 && loaded.player.weapons[2].reserve == 37);
    std::remove(savePath.string().c_str());

    std::cout << "Gridfall core tests passed\n";
    return 0;
}
