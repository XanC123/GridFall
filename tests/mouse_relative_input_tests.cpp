#include "gridfall/core/MouseInput.hpp"
#include <raylib.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {
float gNextDx = 0.0f;
float gNextDy = 0.0f;
int gGetDeltaCalls = 0;
int gConsumeRelativeCalls = 0;
int gResetRelativeCalls = 0;
int gDisableCalls = 0;
int gEnableCalls = 0;
int gShowCalls = 0;
}

extern "C" Vector2 GetMouseDelta(void) {
    ++gGetDeltaCalls;
    return {};
}
extern "C" Vector2 GridfallConsumeRelativeMouseDelta(void) {
    ++gConsumeRelativeCalls;
    const Vector2 result{gNextDx, gNextDy};
    gNextDx = 0.0f;
    gNextDy = 0.0f;
    return result;
}
extern "C" void GridfallResetRelativeMouseDelta(void) {
    ++gResetRelativeCalls;
    gNextDx = 0.0f;
    gNextDy = 0.0f;
}
extern "C" void DisableCursor(void) { ++gDisableCalls; }
extern "C" void EnableCursor(void) { ++gEnableCalls; }
extern "C" void ShowCursor(void) { ++gShowCalls; }

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "Relative-input CHECK failed at %s:%d: %s\n", \
                     __FILE__, __LINE__, #condition); \
        return EXIT_FAILURE; \
    } \
} while (false)

int main() {
    using namespace gridfall;
    MouseInput input;
    CHECK(input.initialize());
    CHECK(input.backendName() != nullptr);

    // A delta is not consumed while the cursor is uncaptured.
    gNextDx = 17.0f;
    gNextDy = -9.0f;
    const MouseDelta ignored = input.consumeDelta();
    CHECK(ignored.x == 0.0f && ignored.y == 0.0f);
    CHECK(gConsumeRelativeCalls == 0);

    input.setCaptured(true);
    CHECK(gDisableCalls == 1);
    CHECK(gResetRelativeCalls == 1);

    // Discard a stale capture-transition sample without losing the next sample.
    gNextDx = 2400.0f;
    gNextDy = -1800.0f;
    const MouseDelta staleSample = input.consumeDelta();
    CHECK(staleSample.x == 0.0f && staleSample.y == 0.0f);
    gNextDx = 12.0f;
    gNextDy = -6.0f;
    const MouseDelta firstRelativeSample = input.consumeDelta();
    CHECK(firstRelativeSample.x == 12.0f && firstRelativeSample.y == -6.0f);

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

    for (const Motion& motion : motions) {
        gNextDx = motion.dx;
        gNextDy = motion.dy;
        const MouseDelta delta = input.consumeDelta();
        CHECK(std::fabs(delta.x - motion.dx) < 0.001f);
        CHECK(std::fabs(delta.y - motion.dy) < 0.001f);

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

    // The independent accumulator returns stationary zero and is consumed once.
    const MouseDelta stationary = input.consumeDelta();
    CHECK(stationary.x == 0.0f && stationary.y == 0.0f);
    CHECK(gConsumeRelativeCalls == 12);
    CHECK(gGetDeltaCalls == 0);

    input.setCaptured(false);
    CHECK(gEnableCalls == 1);
    CHECK(gShowCalls == 1);
    CHECK(gResetRelativeCalls == 2);
    gNextDx = -999.0f;
    gNextDy = 777.0f;
    const MouseDelta ignoredAfterRelease = input.consumeDelta();
    CHECK(ignoredAfterRelease.x == 0.0f && ignoredAfterRelease.y == 0.0f);
    CHECK(gConsumeRelativeCalls == 12);

    input.setCaptured(true);
    CHECK(gDisableCalls == 2);
    CHECK(gResetRelativeCalls == 3);
    gNextDx = -1600.0f;
    gNextDy = 1200.0f;
    const MouseDelta staleRecaptureSample = input.consumeDelta();
    CHECK(staleRecaptureSample.x == 0.0f && staleRecaptureSample.y == 0.0f);
    gNextDx = 5.0f;
    gNextDy = -3.0f;
    const MouseDelta afterRecapture = input.consumeDelta();
    CHECK(afterRecapture.x == 5.0f && afterRecapture.y == -3.0f);

    input.shutdown();
    CHECK(gEnableCalls == 2);
    CHECK(gShowCalls == 2);
    CHECK(gResetRelativeCalls == 4);
    CHECK(gGetDeltaCalls == 0);
    std::puts("SDL relative-input adapter passed: dedicated signed xrel/yrel path, all 8 directions, axis separation, warm-up, one-time consumption, and capture lifecycle.");
    return EXIT_SUCCESS;
}
