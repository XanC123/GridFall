#include "gridfall/core/Controls.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>

using namespace gridfall;

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::cerr << "Mouse-look CHECK failed at " << __FILE__ << ':' << __LINE__ \
                  << ": " #condition << '\n'; \
        return EXIT_FAILURE; \
    } \
} while (false)

int main() {
    struct MotionCase {
        const char* name;
        float dx;
        float dy;
        int yawSign;
        int pitchSign;
    };
    constexpr std::array<MotionCase, 8> motions{{
        {"right",       24.0f,   0.0f,  1,  0},
        {"left",       -24.0f,   0.0f, -1,  0},
        {"up",           0.0f, -24.0f,  0,  1},
        {"down",         0.0f,  24.0f,  0, -1},
        {"up-right",    18.0f, -18.0f,  1,  1},
        {"up-left",    -18.0f, -18.0f, -1,  1},
        {"down-right",  18.0f,  18.0f,  1, -1},
        {"down-left",  -18.0f,  18.0f, -1, -1},
    }};

    for (const MotionCase& motion : motions) {
        MouseLookController camera;
        camera.reset(0.0f, 0.0f);
        camera.update(motion.dx, motion.dy);
        const int actualYawSign = camera.yaw() > 0.0f ? 1 : camera.yaw() < 0.0f ? -1 : 0;
        const int actualPitchSign = camera.pitch() > 0.0f ? 1 : camera.pitch() < 0.0f ? -1 : 0;
        if (actualYawSign != motion.yawSign || actualPitchSign != motion.pitchSign) {
            std::cerr << "Motion failed: " << motion.name << '\n';
            return EXIT_FAILURE;
        }
        CHECK(std::fabs(length(camera.facing()) - 1.0f) < 0.001f);
    }

    // Axis inversion is configuration, not scattered sign changes in the math.
    MouseLookSettings invertedSettings{};
    invertedSettings.invertHorizontal = true;
    invertedSettings.invertVertical = true;
    MouseLookController invertedCamera(invertedSettings);
    invertedCamera.reset(0.0f, 0.0f);
    invertedCamera.update(20.0f, -20.0f);
    CHECK(invertedCamera.yaw() < 0.0f);
    CHECK(invertedCamera.pitch() < 0.0f);

    // Positive horizontal input must turn toward camera-right, not alter pitch.
    MouseLookController rightTurn;
    rightTurn.reset(0.0f, 0.0f);
    const Vec2 initialForward = rightTurn.facing();
    const Vec2 cameraRight{-initialForward.y, initialForward.x};
    rightTurn.update(24.0f, 0.0f);
    const Vec2 positiveTurn = rightTurn.facing();
    CHECK(positiveTurn.x * cameraRight.x + positiveTurn.y * cameraRight.y > 0.0f);
    CHECK(std::fabs(rightTurn.pitch()) < 0.00001f);

    MouseLookController leftTurn;
    leftTurn.reset(0.0f, 0.0f);
    const Vec2 initialForwardForLeft = leftTurn.facing();
    const Vec2 cameraRightForLeft{-initialForwardForLeft.y, initialForwardForLeft.x};
    leftTurn.update(-24.0f, 0.0f);
    const Vec2 negativeTurn = leftTurn.facing();
    CHECK(negativeTurn.x * cameraRightForLeft.x + negativeTurn.y * cameraRightForLeft.y < 0.0f);
    CHECK(std::fabs(leftTurn.pitch()) < 0.00001f);
    MouseLookController stationary;
    stationary.reset(0.25f, -0.1f);
    const float yawBefore = stationary.yaw();
    const float pitchBefore = stationary.pitch();
    for (int frame = 0; frame < 240; ++frame) stationary.update(0.0f, 0.0f);
    CHECK(std::fabs(stationary.yaw() - yawBefore) < 0.00001f);
    CHECK(std::fabs(stationary.pitch() - pitchBefore) < 0.00001f);

    // There is no arbitrary per-frame delta cap: a complete turn is retained
    // and yaw wrapping keeps the direction vector continuous through 360 degrees.
    MouseLookController fullTurn;
    fullTurn.reset(0.0f, 0.0f);
    const float fullTurnPixels = 2.0f * 3.14159265358979323846f /
                                 fullTurn.settings().horizontalRadiansPerPixel;
    fullTurn.update(fullTurnPixels, 0.0f);
    CHECK(std::fabs(fullTurn.yaw()) < 0.001f);
    CHECK(std::fabs(fullTurn.facing().x - 1.0f) < 0.001f);
    CHECK(std::fabs(fullTurn.facing().y) < 0.001f);

    MouseLookController boundary;
    boundary.reset(3.13f, 0.0f);
    const Vec2 facingBeforeWrap = boundary.facing();
    boundary.update(40.0f, 0.0f);
    CHECK(boundary.yaw() < -3.0f);
    const Vec2 facingAfterWrap = boundary.facing();
    const float wrapFacingDot = facingBeforeWrap.x * facingAfterWrap.x +
                                facingBeforeWrap.y * facingAfterWrap.y;
    CHECK(wrapFacingDot > 0.99f);

    // Very large valid relative movement is applied then wrapped, not clipped.
    MouseLookController largeMotion;
    largeMotion.reset(0.0f, 0.0f);
    const float largeDelta = 10000.0f;
    largeMotion.update(largeDelta, 0.0f);
    const float expectedYaw = MouseLookController::wrapRadians(
        largeDelta * largeMotion.settings().horizontalRadiansPerPixel);
    CHECK(std::fabs(largeMotion.yaw() - expectedYaw) < 0.0001f);

    MouseLookController pitchBounds;
    pitchBounds.reset(0.0f, 0.0f);
    pitchBounds.update(0.0f, -10000.0f);
    CHECK(std::fabs(pitchBounds.pitch() - pitchBounds.settings().maximumPitchRadians) < 0.0001f);
    pitchBounds.update(0.0f, 20000.0f);
    CHECK(std::fabs(pitchBounds.pitch() - pitchBounds.settings().minimumPitchRadians) < 0.0001f);

    // The same accumulated movement produces the same angle regardless of how
    // many render-frame samples carried it; delta values are never dt-scaled.
    MouseLookController oneSample;
    MouseLookController twoSamples;
    oneSample.reset(0.0f, 0.0f);
    twoSamples.reset(0.0f, 0.0f);
    oneSample.update(100.0f, -20.0f);
    twoSamples.update(40.0f, -8.0f);
    twoSamples.update(60.0f, -12.0f);
    CHECK(std::fabs(oneSample.yaw() - twoSamples.yaw()) < 0.00001f);
    CHECK(std::fabs(oneSample.pitch() - twoSamples.pitch()) < 0.00001f);

    std::cout << "Mouse-look passed: 8 directions, configurable axes, zero drift, full-turn wrapping, "
                 "uncapped relative deltas, pitch bounds, and sample-rate independence.\n";
    return EXIT_SUCCESS;
}
