#include "gridfall/core/MouseInput.hpp"
#include <raylib.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {
int gScreenWidth = 1360;
int gScreenHeight = 780;
Vector2 gMousePosition{static_cast<float>(gScreenWidth / 2), static_cast<float>(gScreenHeight / 2)};
int gHideCalls = 0;
int gDisableCalls = 0;
int gEnableCalls = 0;
int gShowCalls = 0;
int gSetPositionCalls = 0;
}

extern "C" Vector2 GetMousePosition(void) { return gMousePosition; }
extern "C" int GetScreenWidth(void) { return gScreenWidth; }
extern "C" int GetScreenHeight(void) { return gScreenHeight; }
extern "C" void SetMousePosition(int x, int y) {
    ++gSetPositionCalls;
    // Raylib's SDL adapter updates its current and previous absolute positions
    // synchronously when a warp is requested.
    gMousePosition = {static_cast<float>(x), static_cast<float>(y)};
}
extern "C" void HideCursor(void) { ++gHideCalls; }
extern "C" void DisableCursor(void) { ++gDisableCalls; }
extern "C" void EnableCursor(void) { ++gEnableCalls; }
extern "C" void ShowCursor(void) { ++gShowCalls; }

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "Absolute-pointer CHECK failed at %s:%d: %s\n", \
                     __FILE__, __LINE__, #condition); \
        return EXIT_FAILURE; \
    } \
} while (false)

int main() {
    using namespace gridfall;
    MouseInput input;
    CHECK(input.initialize());
    CHECK(input.backendName() != nullptr);

    // Capture hides the normal pointer but must not enable SDL relative mode.
    input.setCaptured(true);
    CHECK(gHideCalls == 1);
    CHECK(gDisableCalls == 0);
    CHECK(gSetPositionCalls == 1);

    // The first sample after capture is a transition warm-up, not camera input.
    gMousePosition = {gScreenWidth * 0.75f, gScreenHeight * 0.75f};
    const MouseDelta transition = input.consumeDelta();
    CHECK(transition.x == 0.0f && transition.y == 0.0f);
    CHECK(gMousePosition.x == gScreenWidth / 2.0f && gMousePosition.y == gScreenHeight / 2.0f);

    struct Motion {
        const char* name;
        float dx;
        float dy;
        int yawSign;
        int pitchSign;
    };
    constexpr std::array<Motion, 8> motions{{
        {"right", 24.0f, 0.0f, 1, 0},
        {"left", -24.0f, 0.0f, -1, 0},
        {"up", 0.0f, -24.0f, 0, 1},
        {"down", 0.0f, 24.0f, 0, -1},
        {"up-right", 18.0f, -18.0f, 1, 1},
        {"up-left", -18.0f, -18.0f, -1, 1},
        {"down-right", 18.0f, 18.0f, 1, -1},
        {"down-left", -18.0f, 18.0f, -1, -1},
    }};

    float centerX = gScreenWidth / 2.0f;
    float centerY = gScreenHeight / 2.0f;
    for (const Motion& motion : motions) {
        gMousePosition = {centerX + motion.dx, centerY + motion.dy};
        const int warpsBefore = gSetPositionCalls;
        const MouseDelta delta = input.consumeDelta();
        CHECK(std::fabs(delta.x - motion.dx) < 0.001f);
        CHECK(std::fabs(delta.y - motion.dy) < 0.001f);
        CHECK(gSetPositionCalls == warpsBefore + 1);
        CHECK(gMousePosition.x == centerX && gMousePosition.y == centerY);

        MouseLookController camera;
        camera.reset(0.0f, 0.0f);
        camera.update(delta.x, delta.y);
        const int yawSign = camera.yaw() > 0.0f ? 1 : camera.yaw() < 0.0f ? -1 : 0;
        const int pitchSign = camera.pitch() > 0.0f ? 1 : camera.pitch() < 0.0f ? -1 : 0;
        if (yawSign != motion.yawSign || pitchSign != motion.pitchSign) {
            std::fprintf(stderr, "Direction mapping failed for %s\n", motion.name);
            return EXIT_FAILURE;
        }
    }

    // A centered, stationary pointer yields zero with no redundant warp.
    const int stationaryWarpCount = gSetPositionCalls;
    const MouseDelta stationary = input.consumeDelta();
    CHECK(stationary.x == 0.0f && stationary.y == 0.0f);
    CHECK(gSetPositionCalls == stationaryWarpCount);

    // Resizing changes the neutral center, but must not turn the camera.
    gScreenWidth = 1400;
    gScreenHeight = 800;
    gMousePosition = {centerX, centerY};
    const MouseDelta resizeDelta = input.consumeDelta();
    CHECK(resizeDelta.x == 0.0f && resizeDelta.y == 0.0f);
    centerX = gScreenWidth / 2.0f;
    centerY = gScreenHeight / 2.0f;
    CHECK(gMousePosition.x == centerX && gMousePosition.y == centerY);

    // The adapter must not leak window-space coordinates or repeat one sample.
    gMousePosition = {centerX + 7.0f, centerY - 11.0f};
    const MouseDelta oneMovement = input.consumeDelta();
    CHECK(oneMovement.x == 7.0f && oneMovement.y == -11.0f);
    const MouseDelta afterRecenter = input.consumeDelta();
    CHECK(afterRecenter.x == 0.0f && afterRecenter.y == 0.0f);

    input.setCaptured(false);
    CHECK(gEnableCalls == 1);
    CHECK(gShowCalls == 1);
    gMousePosition = {centerX + 77.0f, centerY + 33.0f};
    const MouseDelta ignoredAfterRelease = input.consumeDelta();
    CHECK(ignoredAfterRelease.x == 0.0f && ignoredAfterRelease.y == 0.0f);

    input.setCaptured(true);
    CHECK(gHideCalls == 2);
    CHECK(gDisableCalls == 0);
    gMousePosition = {centerX + 5.0f, centerY - 3.0f};
    const MouseDelta recaptureWarmup = input.consumeDelta();
    CHECK(recaptureWarmup.x == 0.0f && recaptureWarmup.y == 0.0f);
    gMousePosition = {centerX - 9.0f, centerY + 4.0f};
    const MouseDelta afterRecapture = input.consumeDelta();
    CHECK(afterRecapture.x == -9.0f && afterRecapture.y == 4.0f);

    input.shutdown();
    CHECK(gEnableCalls == 2);
    CHECK(gShowCalls == 2);
    CHECK(gDisableCalls == 0);
    std::puts("WSLg absolute-pointer adapter passed: center-relative signed axes, all 8 directions, resizing, one-time samples, stationary zero, no relative-mode calls, and capture lifecycle.");
    return EXIT_SUCCESS;
}
