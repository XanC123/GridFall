#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <string>

namespace gridfall {

inline constexpr int kTileSize = 32;
inline constexpr int kMapWidth = 25;
inline constexpr int kMapHeight = 18;
inline constexpr int kMaxLevels = 20;
inline constexpr float kFixedStep = 1.0f / 60.0f;
inline constexpr float kPi = 3.14159265358979323846f;

struct Vec2 {
    float x{};
    float y{};

    constexpr Vec2 operator+(Vec2 rhs) const noexcept { return {x + rhs.x, y + rhs.y}; }
    constexpr Vec2 operator-(Vec2 rhs) const noexcept { return {x - rhs.x, y - rhs.y}; }
    constexpr Vec2 operator*(float s) const noexcept { return {x * s, y * s}; }
    Vec2& operator+=(Vec2 rhs) noexcept { x += rhs.x; y += rhs.y; return *this; }
};

inline float lengthSquared(Vec2 v) noexcept { return v.x * v.x + v.y * v.y; }
inline float length(Vec2 v) noexcept { return std::sqrt(lengthSquared(v)); }
inline Vec2 normalized(Vec2 v) noexcept {
    const float len = length(v);
    return len > 0.0001f ? Vec2{v.x / len, v.y / len} : Vec2{1.0f, 0.0f};
}
inline float distanceSquared(Vec2 a, Vec2 b) noexcept { return lengthSquared(a - b); }
inline float clampf(float value, float low, float high) noexcept { return std::max(low, std::min(value, high)); }

struct Rgba {
    std::uint8_t r{255};
    std::uint8_t g{255};
    std::uint8_t b{255};
    std::uint8_t a{255};
};

enum class Tile : std::uint8_t {
    Floor, Wall, SecretWall, SideDoor, Spikes, Poison, Exit, Cache, AmmoCache,
    HealthPotion, RegenPotion, AmmoLight, AmmoPiercing, AmmoHeavy
};

enum class AmmoType : std::uint8_t { FastFire = 0, Piercing = 1, SlowSteady = 2 };
enum class GameState : std::uint8_t { Title, Playing, Paused, LevelComplete, GameOver, Victory };

enum class SkillId : std::uint8_t {
    Bulwark = 0,
    DamageMitigation,
    VitalFlux,
    WeaponTuning,
    Overcharge,
    PenetrationCore,
    Lightweight,
    CombatDrills,
    TriggerDiscipline,
    Count
};

inline constexpr std::size_t kSkillCount = static_cast<std::size_t>(SkillId::Count);

inline const char* ammoName(AmmoType type) noexcept {
    switch (type) {
        case AmmoType::FastFire: return "FAST FIRE";
        case AmmoType::Piercing: return "PIERCING";
        case AmmoType::SlowSteady: return "SLOW / STEADY";
    }
    return "UNKNOWN";
}

} // namespace gridfall
