#include "gridfall/systems/VfxSystem.hpp"
#include <algorithm>
#include <cmath>

namespace gridfall {

VfxSystem::VfxSystem() {
    // Pre-allocate for a busy firefight so bursts normally stay allocation-free.
    particles_.reserve(1536);
    tracers_.reserve(128);
    magazines_.reserve(16);
}

float VfxSystem::random01() noexcept {
    // A compact deterministic generator keeps effects reproducible for a given event sequence.
    rngState_ = rngState_ * 1664525u + 1013904223u;
    return static_cast<float>((rngState_ >> 8) & 0x00ffffffu) / 16777215.0f;
}

void VfxSystem::muzzleBurst(Vec2 position, Vec2 direction, AmmoType type) {
    Rgba color{255, 206, 115, 255};
    int count = 9;
    if (type == AmmoType::Piercing) { color = {105, 230, 255, 255}; count = 13; }
    if (type == AmmoType::SlowSteady) { color = {255, 118, 92, 255}; count = 7; }
    tracers_.push_back({position, position + direction * (type == AmmoType::SlowSteady ? 22.0f : 15.0f), 0.07f, 0.07f, color});
    for (int i = 0; i < count; ++i) {
        const float angle = (random01() - 0.5f) * 1.65f;
        const float speed = 38.0f + random01() * 155.0f;
        const float cs = std::cos(angle), sn = std::sin(angle);
        const Vec2 rotated{direction.x * cs - direction.y * sn, direction.x * sn + direction.y * cs};
        const float life = 0.12f + random01() * 0.28f;
        particles_.push_back({position, rotated * speed, life, life, 1.3f + random01() * 2.4f, color, 12.0f});
    }
}

void VfxSystem::hitBurst(Vec2 position, Rgba color, int count) {
    for (int i = 0; i < count; ++i) {
        const float angle = random01() * 2.0f * kPi;
        const float speed = 30.0f + random01() * 105.0f;
        const float life = 0.16f + random01() * 0.30f;
        particles_.push_back({position, {std::cos(angle) * speed, std::sin(angle) * speed},
                              life, life, 1.4f + random01() * 2.0f, color, 10.0f});
    }
}

void VfxSystem::dropMagazine(Vec2 position, Vec2 facing) {
    const Vec2 side{-facing.y, facing.x};
    magazines_.push_back({position, side * (36.0f + random01() * 25.0f) - facing * 12.0f,
                          15.0f, 90.0f + random01() * 35.0f,
                          random01() * 2.0f * kPi, (random01() - 0.5f) * 12.0f, 0.0f});
}

void VfxSystem::update(float dt) {
    for (Particle& p : particles_) {
        p.life -= dt;
        p.velocity.y += p.gravity * dt;
        p.position += p.velocity * dt;
        p.velocity = p.velocity * std::max(0.0f, 1.0f - 1.8f * dt);
    }
    std::erase_if(particles_, [](const Particle& p) { return p.life <= 0.0f; });

    for (Tracer& tracer : tracers_) tracer.life -= dt;
    std::erase_if(tracers_, [](const Tracer& t) { return t.life <= 0.0f; });

    for (MagazineDrop& mag : magazines_) {
        mag.age += dt;
        mag.position += mag.velocity * dt;
        mag.velocity = mag.velocity * std::max(0.0f, 1.0f - 1.15f * dt);
        mag.verticalVelocity -= 310.0f * dt;
        mag.height += mag.verticalVelocity * dt;
        if (mag.height < 0.0f) { mag.height = 0.0f; mag.verticalVelocity = 0.0f; }
        mag.rotation += mag.rotationSpeed * dt;
    }
    std::erase_if(magazines_, [](const MagazineDrop& m) { return m.age > 2.35f; });
}

} // namespace gridfall
