#pragma once

#include "gridfall/core/Types.hpp"

#include <cmath>

namespace gridfall {

struct MouseDelta {
    float x{};
    float y{};
};

// Mouse-look settings are kept in one place rather than hidden in update math.
// The default signs follow conventional FPS behavior; flip either setting only
// if the physical input trace proves that axis is delivered with the opposite sign.
struct MouseLookSettings {
    float horizontalRadiansPerPixel{0.0035f};
    float verticalRadiansPerPixel{0.0035f};
    bool invertHorizontal{false};
    bool invertVertical{false};
    float minimumPitchRadians{-1.20f};
    float maximumPitchRadians{1.20f};
    float startingPitchRadians{-0.08f};
};

class MouseLookController {
public:
    explicit MouseLookController(MouseLookSettings settings = {}) noexcept
        : settings_(settings) {
        reset();
    }

    void reset() noexcept {
        reset(0.0f, settings_.startingPitchRadians);
    }

    void reset(float yaw, float pitch) noexcept {
        yaw_ = wrapRadians(yaw);
        pitch_ = clampf(pitch, settings_.minimumPitchRadians, settings_.maximumPitchRadians);
    }

    // Relative deltas already represent movement since the previous poll. They
    // must not be multiplied by frame time or silently clipped, because either
    // operation makes aim depend on render rate or discards real mouse movement.
    void update(float deltaX, float deltaY) noexcept {
        if (!std::isfinite(deltaX)) deltaX = 0.0f;
        if (!std::isfinite(deltaY)) deltaY = 0.0f;

        constexpr float kPositiveDirection = 1.0f;
        constexpr float kNegativeDirection = -1.0f;
        const float horizontalDirection = settings_.invertHorizontal
            ? kNegativeDirection : kPositiveDirection;
        const float verticalDirection = settings_.invertVertical
            ? kPositiveDirection : kNegativeDirection;

        yaw_ = wrapRadians(yaw_ + deltaX * settings_.horizontalRadiansPerPixel * horizontalDirection);
        // SDL screen Y grows downward. The configurable sign controls whether
        // a downward physical movement raises or lowers the camera pitch.
        pitch_ = clampf(pitch_ + deltaY * settings_.verticalRadiansPerPixel * verticalDirection,
                        settings_.minimumPitchRadians, settings_.maximumPitchRadians);
    }

    [[nodiscard]] float yaw() const noexcept { return yaw_; }
    [[nodiscard]] float pitch() const noexcept { return pitch_; }
    [[nodiscard]] Vec2 facing() const noexcept { return {std::cos(yaw_), std::sin(yaw_)}; }
    [[nodiscard]] const MouseLookSettings& settings() const noexcept { return settings_; }

    [[nodiscard]] static float wrapRadians(float angle) noexcept {
        constexpr float pi = 3.14159265358979323846f;
        constexpr float twoPi = 2.0f * pi;
        angle = std::fmod(angle + pi, twoPi);
        if (angle < 0.0f) angle += twoPi;
        return angle - pi;
    }

private:
    MouseLookSettings settings_{};
    float yaw_{};
    float pitch_{};
};

// Inputs use +1 for right and forward. Output is normalized for diagonals and
// remains camera-relative, regardless of which direction the player is facing.
[[nodiscard]] inline Vec2 cameraRelativeMovement(Vec2 facing, float strafe, float forward) noexcept {
    facing = normalized(facing);
    const Vec2 right{-facing.y, facing.x};
    Vec2 movement = right * strafe + facing * forward;
    const float length2 = lengthSquared(movement);
    if (length2 > 1.0f) movement = movement * (1.0f / std::sqrt(length2));
    return movement;
}

} // namespace gridfall
