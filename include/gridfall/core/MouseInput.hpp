#pragma once

#include "gridfall/core/Controls.hpp"

#include <cstdint>

namespace gridfall {

// Thin adapter over Raylib's SDL relative-mouse mode. The Raylib SDL backend
// is patched by Gridfall's CMake configuration to accumulate every xrel/yrel
// event and reset both relative-position values at the beginning of each frame.
class MouseInput {
public:
    MouseInput() = default;
    ~MouseInput();

    MouseInput(const MouseInput&) = delete;
    MouseInput& operator=(const MouseInput&) = delete;

    [[nodiscard]] bool initialize() noexcept;
    void setCaptured(bool captured) noexcept;
    [[nodiscard]] MouseDelta consumeDelta() noexcept;
    void reportGameplaySample(MouseDelta delta, const MouseLookController& camera,
                              float frameSeconds) noexcept;
    [[nodiscard]] const char* backendName() const noexcept;
    void shutdown() noexcept;

private:
    bool initialized_{};
    bool captured_{};
    bool diagnosticsStarted_{};
    bool diagnosticsFinished_{};
    double diagnosticElapsedSeconds_{};
    double diagnosticReportElapsedSeconds_{};
    double diagnosticTotalX_{};
    double diagnosticTotalY_{};
    double diagnosticWindowX_{};
    double diagnosticWindowY_{};
    float diagnosticLastX_{};
    float diagnosticLastY_{};
    float diagnosticWindowPeakX_{};
    float diagnosticWindowPeakY_{};
    float diagnosticWindowMinimumX_{};
    float diagnosticWindowMaximumX_{};
    float diagnosticWindowMinimumY_{};
    float diagnosticWindowMaximumY_{};
    float diagnosticWindowStartYaw_{};
    float diagnosticWindowStartPitch_{};
    float diagnosticOverallPeakX_{};
    float diagnosticOverallPeakY_{};
    std::uint64_t diagnosticTotalFrames_{};
    std::uint64_t diagnosticTotalMovingFrames_{};
    std::uint64_t diagnosticWindowFrames_{};
    std::uint64_t diagnosticWindowMovingFrames_{};
};

} // namespace gridfall
