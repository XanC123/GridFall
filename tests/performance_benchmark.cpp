#include "gridfall/entities/Enemy.hpp"
#include "gridfall/entities/Player.hpp"
#include "gridfall/systems/CombatSystem.hpp"
#include "gridfall/systems/EnemySystem.hpp"
#include "gridfall/systems/ProgressionSystem.hpp"
#include "gridfall/systems/VfxSystem.hpp"
#include "gridfall/world/Level.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

using namespace gridfall;
using Clock = std::chrono::steady_clock;

int main(int argc, char** argv) {
    constexpr int warmupFrames = 1200;
    constexpr int measuredFrames = 20000;
    constexpr float fixedDt = 1.0f / 60.0f;
    const std::filesystem::path csvPath = argc > 1
        ? std::filesystem::path(argv[1])
        : std::filesystem::path("build/performance/simulation-benchmark.csv");
    std::filesystem::create_directories(csvPath.parent_path().empty() ? "." : csvPath.parent_path());

    Level level;
    level.load(kMaxLevels - 1); // use the toughest sector's enemy tuning
    // Build a clear, bounded benchmark arena. This deliberately removes map
    // occlusion so a worst-case number of enemies can maintain line-of-sight
    // and exercise ranged AI and projectile collision rather than all firing
    // into walls in the procedural level.
    for (int y = 1; y < kMapHeight - 1; ++y)
        for (int x = 1; x < kMapWidth - 1; ++x)
            level.clearToFloor(x, y);

    Player player{};
    player.position = {static_cast<float>(kMapWidth) * 0.5f * kTileSize,
                       static_cast<float>(kMapHeight) * 0.5f * kTileSize};
    player.previousPosition = player.position;
    player.facing = {1.0f, 0.0f};
    // Keep enemies alive so the benchmark sustains the intended worst-case AI
    // count for the entire sample window while still moving player bullets.
    player.damageMultiplier = 0.0f;
    player.weapons[0].reserve = 50000;
    player.weapons[1].reserve = 50000;
    player.weapons[2].reserve = 50000;

    std::vector<Enemy> enemies;
    std::vector<Vec2> benchmarkPositions;
    enemies.reserve(level.enemySpawns().size());
    benchmarkPositions.reserve(level.enemySpawns().size());
    std::uint32_t id = 1;
    const std::size_t enemyCount = level.enemySpawns().size();
    constexpr float pi = 3.14159265358979323846f;
    for (std::size_t i = 0; i < enemyCount; ++i) {
        Enemy enemy{};
        enemy.id = id++;
        const float angle = 2.0f * pi * static_cast<float>(i) /
                            static_cast<float>(std::max<std::size_t>(1, enemyCount));
        const float radius = 255.0f;
        const Vec2 position = player.position + Vec2{std::cos(angle) * radius, std::sin(angle) * radius};
        benchmarkPositions.push_back(position);
        enemy.position = position;
        enemy.previousPosition = enemy.position;
        EnemySystem::configureForSector(enemy, kMaxLevels - 1, 1.0f);
        enemies.push_back(enemy);
    }

    CombatSystem combat;
    EnemySystem enemySystem;
    VfxSystem vfx;
    std::vector<float> simulationMs;
    simulationMs.reserve(measuredFrames);

    std::ofstream csv(csvPath);
    if (!csv) {
        std::cerr << "cannot write benchmark CSV: " << csvPath << '\n';
        return 2;
    }
    csv << "frame,simulation_ms,alive_enemies,player_projectiles,enemy_projectiles,particles,tracers\n";
    csv << std::fixed << std::setprecision(5);

    const int totalFrames = warmupFrames + measuredFrames;
    for (int frame = 0; frame < totalFrames; ++frame) {
        // Vary aim slightly to exercise firing, wall sweeps, enemy hit tests,
        // projectile lifetimes, AI steering, enemy return fire, and VFX churn.
        const float angle = static_cast<float>(frame) * 0.0028f;
        player.facing = {std::cos(angle), std::sin(angle)};
        const auto begin = Clock::now();

        (void)combat.tryFire(player, player.facing, true, vfx);
        // Reset positions to their ring after each measured tick. AI movement
        // calculations still run, but enemies do not collapse into melee range,
        // keeping the repeatable stress workload focused on 27 active actors.
        for (std::size_t i = 0; i < enemies.size(); ++i) {
            enemies[i].position = benchmarkPositions[i];
            enemies[i].previousPosition = benchmarkPositions[i];
        }
        enemySystem.update(enemies, player, level, fixedDt);
        (void)combat.updateProjectiles(level, enemies, vfx, fixedDt);
        combat.updateReload(player, fixedDt);
        if (player.activeWeapon().magazine == 0 && !player.reloading)
            (void)combat.requestReload(player, vfx);
        vfx.update(fixedDt);

        const auto end = Clock::now();
        const float elapsed = std::chrono::duration<float, std::milli>(end - begin).count();
        if (frame >= warmupFrames) {
            simulationMs.push_back(elapsed);
            csv << (frame - warmupFrames) << ',' << elapsed << ','
                << EnemySystem::aliveCount(enemies) << ',' << combat.projectiles().size() << ','
                << enemySystem.projectiles().size() << ',' << vfx.particles().size() << ','
                << vfx.tracers().size() << '\n';
        }

        // Recover from a simulated player death so the benchmark remains in a
        // stable, repeatable workload throughout the entire measurement window.
        if (player.health < 25.0f) player.health = player.maxHealth;
        if (player.activeWeapon().magazine == 0 && player.activeWeapon().reserve == 0)
            player.activeWeapon().reserve = 50000;
    }
    csv.close();

    if (simulationMs.empty()) {
        std::cerr << "benchmark produced no samples\n";
        return 3;
    }
    std::sort(simulationMs.begin(), simulationMs.end());
    const double sum = std::accumulate(simulationMs.begin(), simulationMs.end(), 0.0);
    const double mean = sum / static_cast<double>(simulationMs.size());
    const auto percentile = [&](double p) {
        const std::size_t index = static_cast<std::size_t>(p * static_cast<double>(simulationMs.size() - 1));
        return simulationMs[index];
    };
    const float maxMs = simulationMs.back();
    const auto countAbove = [&](float threshold) {
        return std::count_if(simulationMs.begin(), simulationMs.end(),
                             [threshold](float v) { return v > threshold; });
    };
    const auto summarize = [&](std::ostream& out) {
        out << "GRIDFALL HEADLESS SIMULATION BENCHMARK\n"
            << "Build mode: optimized compiler flags supplied by benchmark script\n"
            << "Scenario: Sector " << kMaxLevels << " tuning, " << enemies.size()
            << " enemies in a clear ring arena (sustained ranged AI + projectiles + VFX)\n"
            << "Samples: " << simulationMs.size() << " ticks at fixed 60 Hz simulation step\n"
            << std::fixed << std::setprecision(4)
            << "Simulation CPU mean: " << mean << " ms/tick\n"
            << "Simulation CPU p50:  " << percentile(0.50) << " ms/tick\n"
            << "Simulation CPU p95:  " << percentile(0.95) << " ms/tick\n"
            << "Simulation CPU p99:  " << percentile(0.99) << " ms/tick\n"
            << "Simulation CPU max:  " << maxMs << " ms/tick\n"
            << "Ticks > 6.944 ms (144 Hz frame budget): " << countAbove(6.944f) << '\n'
            << "Ticks > 10.42 ms (144 Hz spike threshold): " << countAbove(10.42f) << '\n'
            << "Ticks > 16.667 ms (60 Hz frame budget): " << countAbove(16.667f) << '\n'
            << "CSV: " << csvPath.string() << '\n'
            << "NOTE: headless simulation CPU timing only; excludes Raylib drawing, GPU, WSLg, compositor and presentation latency.\n";
    };
    summarize(std::cout);

    std::ofstream report(csvPath.parent_path() / "simulation-benchmark.txt");
    if (report) summarize(report);
    return 0;
}
