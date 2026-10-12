#include "gridfall/world/Level.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <queue>
#include <utility>

namespace gridfall {
namespace {
constexpr Rgba rgba(unsigned char r, unsigned char g, unsigned char b) {
    return {r, g, b, 255};
}
// Twenty distinct, deliberately low-saturation palettes: floor and wall values
// remain subdued while trim/accent colors clearly identify each sector.
constexpr std::array<LevelPalette, kMaxLevels> kSectorPalettes{{
    {rgba(43,45,50), rgba(75,75,78), rgba(78,79,84), rgba(130,119,103), rgba(50,52,56), rgba(104,77,65), rgba(129,84,48), rgba(241,173,86), rgba(221,91,77), rgba(113,151,78)},
    {rgba(35,47,48), rgba(58,83,79), rgba(63,83,80), rgba(105,143,126), rgba(39,59,58), rgba(71,111,99), rgba(89,131,112), rgba(105,211,177), rgba(229,106,79), rgba(137,183,101)},
    {rgba(37,43,56), rgba(67,79,104), rgba(67,78,101), rgba(124,137,162), rgba(43,52,69), rgba(71,91,127), rgba(89,112,151), rgba(134,193,242), rgba(237,105,94), rgba(125,181,134)},
    {rgba(47,49,39), rgba(79,83,62), rgba(81,86,67), rgba(139,139,100), rgba(54,57,44), rgba(105,93,58), rgba(128,103,55), rgba(219,192,103), rgba(230,111,74), rgba(145,181,91)},
    {rgba(50,40,48), rgba(87,65,84), rgba(90,69,86), rgba(151,116,137), rgba(57,43,58), rgba(111,72,104), rgba(139,81,122), rgba(232,145,206), rgba(247,114,111), rgba(143,180,117)},
    {rgba(50,37,40), rgba(91,60,66), rgba(88,60,64), rgba(157,102,94), rgba(58,42,45), rgba(119,62,60), rgba(142,70,65), rgba(242,125,102), rgba(247,98,85), rgba(153,175,91)},
    {rgba(32,48,42), rgba(57,87,69), rgba(61,84,70), rgba(105,147,116), rgba(38,58,46), rgba(62,101,78), rgba(78,126,88), rgba(142,218,153), rgba(233,108,82), rgba(178,211,111)},
    {rgba(35,41,54), rgba(61,78,105), rgba(63,75,98), rgba(112,131,164), rgba(40,49,66), rgba(74,86,133), rgba(86,105,151), rgba(125,171,238), rgba(238,103,117), rgba(115,189,153)},
    {rgba(53,42,35), rgba(91,68,49), rgba(95,72,52), rgba(163,119,82), rgba(63,48,38), rgba(126,81,51), rgba(153,88,47), rgba(246,165,91), rgba(243,99,80), rgba(177,174,92)},
    {rgba(43,38,54), rgba(77,63,100), rgba(79,67,98), rgba(132,114,165), rgba(50,43,66), rgba(99,72,132), rgba(119,81,149), rgba(193,153,248), rgba(247,112,135), rgba(125,185,130)},
    {rgba(38,48,52), rgba(63,86,91), rgba(66,88,94), rgba(112,145,151), rgba(42,57,62), rgba(68,102,111), rgba(82,122,135), rgba(112,213,226), rgba(236,110,92), rgba(126,194,151)},
    {rgba(51,44,36), rgba(86,74,52), rgba(88,76,56), rgba(151,128,87), rgba(58,51,39), rgba(115,90,54), rgba(140,101,48), rgba(237,190,105), rgba(243,107,82), rgba(146,189,97)},
    {rgba(42,46,42), rgba(68,83,69), rgba(70,85,72), rgba(123,145,116), rgba(45,56,46), rgba(83,105,77), rgba(98,129,80), rgba(175,213,132), rgba(236,108,93), rgba(139,205,126)},
    {rgba(39,43,54), rgba(65,73,95), rgba(68,75,97), rgba(114,125,158), rgba(44,50,65), rgba(75,83,126), rgba(90,101,148), rgba(144,174,237), rgba(239,106,110), rgba(117,177,144)},
    {rgba(52,39,40), rgba(86,61,60), rgba(91,64,64), rgba(151,103,97), rgba(59,43,45), rgba(118,66,63), rgba(139,75,65), rgba(242,139,113), rgba(247,93,80), rgba(170,181,91)},
    {rgba(37,49,48), rgba(62,88,84), rgba(65,88,85), rgba(113,146,133), rgba(43,59,57), rgba(70,107,96), rgba(85,128,107), rgba(125,218,187), rgba(233,107,91), rgba(139,196,110)},
    {rgba(48,42,55), rgba(82,68,101), rgba(84,70,100), rgba(139,119,161), rgba(55,47,67), rgba(110,78,132), rgba(130,84,150), rgba(207,158,245), rgba(246,107,133), rgba(142,188,130)},
    {rgba(51,47,37), rgba(88,79,56), rgba(91,80,58), rgba(157,138,91), rgba(59,56,42), rgba(119,99,55), rgba(143,110,51), rgba(239,197,110), rgba(240,105,82), rgba(151,188,95)},
    {rgba(37,42,52), rgba(62,74,96), rgba(66,77,96), rgba(111,126,153), rgba(43,51,63), rgba(74,86,124), rgba(91,105,144), rgba(133,182,226), rgba(235,104,119), rgba(127,189,149)},
    {rgba(42,41,46), rgba(73,71,81), rgba(76,73,83), rgba(129,121,139), rgba(48,47,55), rgba(96,79,113), rgba(116,84,131), rgba(219,169,235), rgba(247,102,97), rgba(160,197,114)}
}};
} // namespace

void Level::addHorizontalWall(int y, int x0, int x1, const std::vector<int>& gaps) {
    for (int x = x0; x <= x1; ++x) {
        bool gap = false;
        for (int g : gaps) gap = gap || x == g;
        if (!gap) setTile(x, y, Tile::Wall);
    }
}

void Level::addVerticalWall(int x, int y0, int y1, const std::vector<int>& gaps) {
    for (int y = y0; y <= y1; ++y) {
        bool gap = false;
        for (int g : gaps) gap = gap || y == g;
        if (!gap) setTile(x, y, Tile::Wall);
    }
}

void Level::load(int zeroBasedIndex) {
    index_ = std::clamp(zeroBasedIndex, 0, kMaxLevels - 1);
    palette_ = kSectorPalettes[static_cast<std::size_t>(index_)];
    const int pattern = index_ % 5;  // Five base room grammars.
    const int cycle = index_ / 5;   // Four structural escalation phases.
    tiles_.fill(Tile::Floor);
    enemySpawns_.clear();

    // A permanent stone border prevents the player and projectiles escaping the map.
    for (int x = 0; x < kMapWidth; ++x) {
        setTile(x, 0, Tile::Wall);
        setTile(x, kMapHeight - 1, Tile::Wall);
    }
    for (int y = 0; y < kMapHeight; ++y) {
        setTile(0, y, Tile::Wall);
        setTile(kMapWidth - 1, y, Tile::Wall);
    }

    // Five readable room grammars recur with increasingly dense optional cover.
    // A guaranteed corridor is carved below, so procedural additions can never
    // accidentally make the extraction objective unreachable.
    const int h1 = 4 + (pattern % 3);
    const int v1 = 13 + (pattern % 3);
    const int h2 = 11 - (pattern % 2);
    const int v2 = 5 + (pattern % 2);
    addHorizontalWall(h1, 3, 12, {5 + pattern % 4, 9});
    addVerticalWall(v1, 2, 9, {3 + pattern % 3, 7});
    addHorizontalWall(h2, 11, 21, {14, 18 - pattern % 2});
    addVerticalWall(v2, 8, 15, {10, 13});

    if (pattern >= 1) {
        addHorizontalWall(8, 8, 11, {10});
        addVerticalWall(19, 3, 6, {5});
    }
    if (pattern >= 2) {
        addHorizontalWall(3, 4, 8, {6});
        addVerticalWall(10, 11, 15, {13});
    }
    if (pattern >= 3) {
        addHorizontalWall(14, 15, 21, {18});
        addVerticalWall(21, 4, 9, {6, 8});
    }
    if (pattern >= 4) {
        addHorizontalWall(7, 17, 22, {19});
        addVerticalWall(9, 2, 5, {4});
    }

    // Five stronger silhouette/flow signatures distinguish the base sector
    // families. The extraction spine is carved after these signatures, so each
    // layout remains beatable while combat spaces feel meaningfully different.
    switch (pattern) {
        case 0: // Bastion: two staggered defensive walls and a central firing lane.
            addVerticalWall(8, 5, 15, {8, 12});
            addVerticalWall(16, 4, 13, {6, 10});
            addHorizontalWall(9, 9, 15, {11, 14});
            break;
        case 1: // Killhouse: long horizontal lanes with offset breaks.
            addHorizontalWall(5, 4, 20, {6, 10, 15});
            addHorizontalWall(13, 4, 20, {8, 12, 17});
            addVerticalWall(12, 6, 12, {8, 10});
            break;
        case 2: // Maze: staggered chambers that encourage flanking.
            addHorizontalWall(6, 6, 18, {9, 14});
            addHorizontalWall(10, 4, 19, {7, 11, 16});
            addVerticalWall(18, 6, 14, {8, 12});
            addVerticalWall(6, 8, 15, {10, 13});
            break;
        case 3: // Ring: a broken central enclosure and two side gates.
            addVerticalWall(7, 4, 14, {6, 10});
            addVerticalWall(17, 4, 14, {7, 12});
            addHorizontalWall(6, 8, 16, {12});
            addHorizontalWall(13, 8, 16, {10, 14});
            break;
        case 4: // Gauntlet: crossing cover islands split into readable stages.
            addHorizontalWall(5, 3, 21, {6, 11, 16});
            addHorizontalWall(12, 3, 21, {8, 14, 18});
            addVerticalWall(12, 6, 11, {8});
            addVerticalWall(19, 7, 14, {10});
            break;
        default: break;
    }

    // Beyond sector five, each five-sector cycle introduces another layer of
    // cover. Short segments always contain gaps to preserve combat lanes.
    if (cycle >= 1) {
        const int row = 5 + ((pattern + cycle) % 5);
        const int startX = 3 + (pattern % 3);
        addHorizontalWall(row, startX, startX + 7, {startX + 2, startX + 5});
        const int column = 15 + (pattern % 3);
        addVerticalWall(column, 5, 13, {7 + (cycle % 3), 11});
    }
    if (cycle >= 2) {
        addHorizontalWall(12, 12, 20, {15, 18});
        addVerticalWall(7 + (pattern % 2), 9, 15, {11, 13});
    }
    if (cycle >= 3) {
        addHorizontalWall(9 + (pattern % 2), 12, 20, {14, 18});
        addVerticalWall(17 + (pattern % 2), 3, 8, {5, 7});
    }

    // Secret wall is separate from the side-door cache room; both are usable routes.
    const int secretX = 11;
    const int secretY = h1;
    if (tileAt(secretX, secretY) == Tile::Wall) setTile(secretX, secretY, Tile::SecretWall);

    playerSpawn_ = {2.5f * kTileSize, 2.5f * kTileSize};
    exitPosition_ = {22.5f * kTileSize, 15.5f * kTileSize};
    for (int y = 1; y <= 3; ++y) for (int x = 1; x <= 3; ++x) clearToFloor(x, y);
    for (int y = 14; y <= 16; ++y) for (int x = 21; x <= 23; ++x) clearToFloor(x, y);

    const auto putIfFloor = [this](int x, int y, Tile tile) {
        if (tileAt(x, y) == Tile::Floor) setTile(x, y, tile);
    };
    putIfFloor(3 + (pattern % 3), 9, Tile::HealthPotion);
    putIfFloor(20, 12 + (pattern % 2), Tile::RegenPotion);
    putIfFloor(9, 6, Tile::AmmoLight);
    putIfFloor(13, 8, Tile::AmmoPiercing);
    putIfFloor(10, 14, Tile::AmmoHeavy);
    putIfFloor(6, 7, Tile::Spikes);
    putIfFloor(8, 13, Tile::Spikes);
    putIfFloor(17, 8, Tile::Spikes);
    putIfFloor(12, 8, Tile::Poison);
    putIfFloor(19, 12, Tile::Poison);
    if (pattern >= 2) putIfFloor(4, 12, Tile::Poison);
    if (cycle >= 1) putIfFloor(14 + (pattern % 4), 13, Tile::Spikes);
    if (cycle >= 2) putIfFloor(5 + (pattern % 3), 5, Tile::Poison);
    if (cycle >= 3) putIfFloor(15 + (pattern % 4), 6, Tile::Spikes);

    // Build an actual side room, sealed by walls with one interactable door.
    // The corridor at y=9 joins the guaranteed main lane at x=22. Opening the
    // door at (18,10) lets the player enter the room and reach its chest/ammo cache.
    constexpr int roomLeft = 15, roomRight = 20, roomTop = 10, roomBottom = 15;
    for (int x = roomLeft; x <= roomRight; ++x) {
        setTile(x, roomTop, Tile::Wall);
        setTile(x, roomBottom, Tile::Wall);
    }
    for (int y = roomTop; y <= roomBottom; ++y) {
        setTile(roomLeft, y, Tile::Wall);
        setTile(roomRight, y, Tile::Wall);
    }
    for (int y = roomTop + 1; y < roomBottom; ++y)
        for (int x = roomLeft + 1; x < roomRight; ++x)
            setTile(x, y, Tile::Floor);
    setTile(18, roomTop, Tile::SideDoor);
    for (int x = 18; x <= 22; ++x) clearToFloor(x, 9);
    setTile(18, 13, Tile::Cache);
    setTile(17, 13, Tile::AmmoCache);
    setTile(16, 13, Tile::AmmoLight);
    setTile(19, 13, Tile::AmmoHeavy);
    setTile(18, 12, Tile::AmmoPiercing);
    setTile(19, 12, Tile::RegenPotion);

    // Carve a reliable route across the north lane and down the extraction lane.
    // This is also a useful tactical flank route on the denser later-sector layouts.
    for (int x = 2; x <= 22; ++x) clearToFloor(x, 2);
    for (int y = 2; y <= 15; ++y) clearToFloor(22, y);
    // Re-clear a safe spawn pocket and extraction pocket after all map additions.
    for (int y = 1; y <= 3; ++y) for (int x = 1; x <= 3; ++x) clearToFloor(x, y);
    for (int y = 14; y <= 16; ++y) for (int x = 21; x <= 23; ++x) clearToFloor(x, y);
    setTile(22, 15, Tile::Exit);

    // Reliable resupply on the guaranteed north corridor: these cells are set
    // after procedural walls are carved, so every sector actually contains all
    // three ammo classes rather than silently losing pickups to a wall segment.
    setTile(6, 2, Tile::AmmoLight);
    setTile(12, 2, Tile::AmmoPiercing);
    setTile(17, 2, Tile::AmmoHeavy);

    // Flood-fill the walkable world from the spawn. Enemy spawn candidates are
    // selected only from reachable floor cells, avoiding walls, pickups, hazards,
    // the player start and the extraction point.
    std::array<bool, kMapWidth * kMapHeight> reachable{};
    std::queue<std::pair<int, int>> pending;
    pending.push({2, 2});
    reachable[static_cast<std::size_t>(2 * kMapWidth + 2)] = true;
    constexpr int dx[4]{1, -1, 0, 0};
    constexpr int dy[4]{0, 0, 1, -1};
    while (!pending.empty()) {
        const auto [x, y] = pending.front();
        pending.pop();
        for (int d = 0; d < 4; ++d) {
            const int nx = x + dx[d];
            const int ny = y + dy[d];
            if (!isInside(nx, ny) || isSolid(tileAt(nx, ny))) continue;
            const std::size_t idx = static_cast<std::size_t>(ny * kMapWidth + nx);
            if (reachable[idx]) continue;
            reachable[idx] = true;
            pending.push({nx, ny});
        }
    }

    struct Candidate { int x; int y; std::uint32_t score; };
    std::vector<Candidate> candidates;
    candidates.reserve(kMapWidth * kMapHeight);
    for (int y = 1; y < kMapHeight - 1; ++y) {
        for (int x = 1; x < kMapWidth - 1; ++x) {
            const std::size_t idx = static_cast<std::size_t>(y * kMapWidth + x);
            if (!reachable[idx] || tileAt(x, y) != Tile::Floor) continue;
            const float fromSpawnX = static_cast<float>(x) + 0.5f - 2.5f;
            const float fromSpawnY = static_cast<float>(y) + 0.5f - 2.5f;
            const float fromExitX = static_cast<float>(x) + 0.5f - 22.5f;
            const float fromExitY = static_cast<float>(y) + 0.5f - 15.5f;
            if (fromSpawnX * fromSpawnX + fromSpawnY * fromSpawnY < 42.0f) continue;
            if (fromExitX * fromExitX + fromExitY * fromExitY < 20.0f) continue;

            std::uint32_t hash = static_cast<std::uint32_t>((x + 1) * 73856093u) ^
                                 static_cast<std::uint32_t>((y + 1) * 19349663u) ^
                                 static_cast<std::uint32_t>((index_ + 1) * 83492791u);
            hash ^= hash >> 16;
            hash *= 0x7feb352du;
            hash ^= hash >> 15;
            candidates.push_back({x, y, hash});
        }
    }
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        return a.score > b.score;
    });

    const int desiredEnemies = std::min(30, 8 + index_);
    std::vector<Vec2> chosen;
    chosen.reserve(static_cast<std::size_t>(desiredEnemies));
    const auto tryAddCandidates = [&](float minimumDistanceSquared) {
        for (const Candidate& candidate : candidates) {
            if (static_cast<int>(enemySpawns_.size()) >= desiredEnemies) break;
            const Vec2 pos{(static_cast<float>(candidate.x) + 0.5f) * kTileSize,
                           (static_cast<float>(candidate.y) + 0.5f) * kTileSize};
            bool tooClose = false;
            for (const Vec2 existing : chosen) {
                if (distanceSquared(existing, pos) < minimumDistanceSquared * kTileSize * kTileSize) {
                    tooClose = true;
                    break;
                }
            }
            if (tooClose) continue;
            chosen.push_back(pos);
            enemySpawns_.push_back({pos, 1.0f + static_cast<float>(index_) * 0.0125f});
        }
    };
    tryAddCandidates(2.0f);
    if (static_cast<int>(enemySpawns_.size()) < desiredEnemies) tryAddCandidates(1.0f);

    setTile(22, 15, Tile::Exit);
}

bool Level::isInside(int x, int y) const noexcept {
    return x >= 0 && y >= 0 && x < kMapWidth && y < kMapHeight;
}

Tile Level::tileAt(int x, int y) const noexcept {
    if (!isInside(x, y)) return Tile::Wall;
    return tiles_[static_cast<std::size_t>(cellIndex(x, y))];
}

void Level::setTile(int x, int y, Tile tile) noexcept {
    if (isInside(x, y)) tiles_[static_cast<std::size_t>(cellIndex(x, y))] = tile;
}

void Level::clearToFloor(int x, int y) noexcept {
    if (isInside(x, y) && x > 0 && y > 0 && x < kMapWidth - 1 && y < kMapHeight - 1)
        setTile(x, y, Tile::Floor);
}

Tile Level::tileAtWorld(Vec2 position) const noexcept {
    return tileAt(static_cast<int>(std::floor(position.x / kTileSize)),
                  static_cast<int>(std::floor(position.y / kTileSize)));
}

bool Level::isSolid(Tile tile) const noexcept {
    return tile == Tile::Wall || tile == Tile::SecretWall || tile == Tile::SideDoor;
}

bool Level::circleHitsSolid(Vec2 center, float radius) const noexcept {
    // Test centre and four radius-offset points. Axis-separated motion in Game keeps sliding smooth.
    const std::array<Vec2, 5> probes{{
        center, {center.x - radius, center.y}, {center.x + radius, center.y},
        {center.x, center.y - radius}, {center.x, center.y + radius}
    }};
    for (const Vec2 p : probes) {
        if (isSolid(tileAtWorld(p))) return true;
    }
    return false;
}

InteractionResult Level::interactNear(Vec2 worldPosition) {
    InteractionResult result{};
    const int cx = static_cast<int>(worldPosition.x / kTileSize);
    const int cy = static_cast<int>(worldPosition.y / kTileSize);
    float best = static_cast<float>(kTileSize * kTileSize) * 1.55f;
    int bestX = -1;
    int bestY = -1;
    Tile bestTile = Tile::Floor;
    for (int y = cy - 1; y <= cy + 1; ++y) {
        for (int x = cx - 1; x <= cx + 1; ++x) {
            const Tile tile = tileAt(x, y);
            const bool interactive = tile == Tile::SecretWall || tile == Tile::SideDoor ||
                                     tile == Tile::Cache || tile == Tile::AmmoCache;
            if (!interactive) continue;
            const Vec2 center{(static_cast<float>(x) + 0.5f) * kTileSize,
                              (static_cast<float>(y) + 0.5f) * kTileSize};
            const float distance = distanceSquared(worldPosition, center);
            if (distance < best) {
                best = distance;
                bestX = x;
                bestY = y;
                bestTile = tile;
            }
        }
    }
    if (bestX < 0) return result;

    result.changed = true;
    switch (bestTile) {
        case Tile::SecretWall:
            setTile(bestX, bestY, Tile::Floor);
            result.secret = true;
            break;
        case Tile::SideDoor:
            setTile(bestX, bestY, Tile::Floor);
            break;
        case Tile::Cache:
            setTile(bestX, bestY, Tile::Floor);
            result.cache = true;
            break;
        case Tile::AmmoCache:
            setTile(bestX, bestY, Tile::Floor);
            result.ammoCache = true;
            break;
        default:
            result.changed = false;
            break;
    }
    return result;
}

} // namespace gridfall
