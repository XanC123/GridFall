#pragma once

#include "gridfall/core/Types.hpp"
#include <array>
#include <vector>

namespace gridfall {

struct EnemySpawn { Vec2 position{}; float healthScale{1.0f}; };
struct InteractionResult {
    bool changed{};
    bool secret{};
    bool cache{};
    bool ammoCache{};
};

// Color values belong to the world/theme layer, not raylib, so core tests stay headless.
struct LevelPalette {
    Rgba floor{};
    Rgba grid{};
    Rgba wall{};
    Rgba wallCap{};
    Rgba wallDetail{};
    Rgba secret{};
    Rgba door{};
    Rgba accent{};
    Rgba hazard{};
    Rgba poison{};
};

class Level {
public:
    void load(int zeroBasedIndex);

    [[nodiscard]] int index() const noexcept { return index_; }
    [[nodiscard]] const LevelPalette& palette() const noexcept { return palette_; }
    [[nodiscard]] Tile tileAt(int x, int y) const noexcept;
    void setTile(int x, int y, Tile tile) noexcept;
    [[nodiscard]] Tile tileAtWorld(Vec2 position) const noexcept;
    [[nodiscard]] bool isSolid(Tile tile) const noexcept;
    [[nodiscard]] bool circleHitsSolid(Vec2 center, float radius) const noexcept;
    [[nodiscard]] bool isInside(int x, int y) const noexcept;
    [[nodiscard]] Vec2 playerSpawn() const noexcept { return playerSpawn_; }
    [[nodiscard]] Vec2 exitPosition() const noexcept { return exitPosition_; }
    [[nodiscard]] const std::vector<EnemySpawn>& enemySpawns() const noexcept { return enemySpawns_; }
    [[nodiscard]] int widthPixels() const noexcept { return kMapWidth * kTileSize; }
    [[nodiscard]] int heightPixels() const noexcept { return kMapHeight * kTileSize; }

    InteractionResult interactNear(Vec2 worldPosition);
    void clearToFloor(int x, int y) noexcept;

private:
    std::array<Tile, kMapWidth * kMapHeight> tiles_{};
    int index_{};
    LevelPalette palette_{};
    Vec2 playerSpawn_{};
    Vec2 exitPosition_{};
    std::vector<EnemySpawn> enemySpawns_;

    [[nodiscard]] static int cellIndex(int x, int y) noexcept { return y * kMapWidth + x; }
    void addHorizontalWall(int y, int x0, int x1, const std::vector<int>& gaps);
    void addVerticalWall(int x, int y0, int y1, const std::vector<int>& gaps);
};

} // namespace gridfall
