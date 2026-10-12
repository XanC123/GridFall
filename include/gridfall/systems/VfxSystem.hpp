#pragma once
#include "gridfall/core/Types.hpp"
#include <vector>

namespace gridfall {

struct Particle {
    Vec2 position{};
    Vec2 velocity{};
    float life{};
    float maxLife{};
    float size{};
    Rgba color{};
    float gravity{};
};

struct Tracer {
    Vec2 from{};
    Vec2 to{};
    float life{0.08f};
    float maxLife{0.08f};
    Rgba color{255, 225, 145, 255};
};

struct MagazineDrop {
    Vec2 position{};
    Vec2 velocity{};
    float height{18.0f};
    float verticalVelocity{100.0f};
    float rotation{};
    float rotationSpeed{5.0f};
    float age{};
};

class VfxSystem {
public:
    VfxSystem();
    void muzzleBurst(Vec2 position, Vec2 direction, AmmoType type);
    void hitBurst(Vec2 position, Rgba color, int count = 8);
    void dropMagazine(Vec2 position, Vec2 facing);
    void update(float dt);
    [[nodiscard]] const std::vector<Particle>& particles() const noexcept { return particles_; }
    [[nodiscard]] const std::vector<Tracer>& tracers() const noexcept { return tracers_; }
    [[nodiscard]] const std::vector<MagazineDrop>& magazines() const noexcept { return magazines_; }

private:
    std::vector<Particle> particles_;
    std::vector<Tracer> tracers_;
    std::vector<MagazineDrop> magazines_;
    std::uint32_t rngState_{0x9e3779b9u};
    float random01() noexcept;
};

} // namespace gridfall
