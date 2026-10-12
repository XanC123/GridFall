#include "gridfall/core/MouseInput.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace gridfall {
namespace {

constexpr double kDiagnosticsDurationSeconds = 10.0;
constexpr double kDiagnosticsReportIntervalSeconds = 0.25;
constexpr float kRadiansToDegrees = 57.29577951308232f;

} // namespace

MouseInput::~MouseInput() { shutdown(); }

bool MouseInput::initialize() noexcept {
    if (initialized_) return true;
    initialized_ = true;
    return true;
}

void MouseInput::setCaptured(bool captured) noexcept {
    if (captured == captured_) return;

    // In a Gridfall SDL-backed Raylib build, DisableCursor() enables SDL relative
    // mouse mode. The CMake patch ensures each frame starts at zero and all
    // SDL_MOUSEMOTION xrel/yrel events are accumulated before GetMouseDelta().
    if (captured) {
        DisableCursor();
    } else {
        EnableCursor();
        ShowCursor();
    }
    captured_ = captured;
}

MouseDelta MouseInput::consumeDelta() noexcept {
    if (!initialized_ || !captured_) return {};

    const Vector2 delta = GetMouseDelta();
    return {
        std::isfinite(delta.x) ? delta.x : 0.0f,
        std::isfinite(delta.y) ? delta.y : 0.0f
    };
}

void MouseInput::reportGameplaySample(MouseDelta delta, const MouseLookController& camera,
                                      float frameSeconds) noexcept {
    if (!initialized_ || !captured_ || diagnosticsFinished_) return;

    if (!diagnosticsStarted_) {
        diagnosticsStarted_ = true;
        diagnosticWindowStartYaw_ = camera.yaw();
        diagnosticWindowStartPitch_ = camera.pitch();
        std::fprintf(stderr,
                     "\n[MOUSE_DIAG] START: capturing gameplay mouse input for %.0f active seconds. "
                     "Each report aggregates approximately %.2f seconds.\n",
                     kDiagnosticsDurationSeconds, kDiagnosticsReportIntervalSeconds);
        std::fflush(stderr);
    }

    if (!std::isfinite(frameSeconds) || frameSeconds < 0.0f) frameSeconds = 0.0f;
    diagnosticElapsedSeconds_ += static_cast<double>(frameSeconds);
    diagnosticReportElapsedSeconds_ += static_cast<double>(frameSeconds);

    diagnosticLastX_ = std::isfinite(delta.x) ? delta.x : 0.0f;
    diagnosticLastY_ = std::isfinite(delta.y) ? delta.y : 0.0f;
    if (diagnosticWindowFrames_ == 0) {
        diagnosticWindowMinimumX_ = diagnosticLastX_;
        diagnosticWindowMaximumX_ = diagnosticLastX_;
        diagnosticWindowMinimumY_ = diagnosticLastY_;
        diagnosticWindowMaximumY_ = diagnosticLastY_;
    } else {
        diagnosticWindowMinimumX_ = std::min(diagnosticWindowMinimumX_, diagnosticLastX_);
        diagnosticWindowMaximumX_ = std::max(diagnosticWindowMaximumX_, diagnosticLastX_);
        diagnosticWindowMinimumY_ = std::min(diagnosticWindowMinimumY_, diagnosticLastY_);
        diagnosticWindowMaximumY_ = std::max(diagnosticWindowMaximumY_, diagnosticLastY_);
    }
    diagnosticWindowX_ += static_cast<double>(diagnosticLastX_);
    diagnosticWindowY_ += static_cast<double>(diagnosticLastY_);
    diagnosticTotalX_ += static_cast<double>(diagnosticLastX_);
    diagnosticTotalY_ += static_cast<double>(diagnosticLastY_);

    const float absoluteX = std::fabs(diagnosticLastX_);
    const float absoluteY = std::fabs(diagnosticLastY_);
    diagnosticWindowPeakX_ = std::max(diagnosticWindowPeakX_, absoluteX);
    diagnosticWindowPeakY_ = std::max(diagnosticWindowPeakY_, absoluteY);
    diagnosticOverallPeakX_ = std::max(diagnosticOverallPeakX_, absoluteX);
    diagnosticOverallPeakY_ = std::max(diagnosticOverallPeakY_, absoluteY);

    ++diagnosticTotalFrames_;
    ++diagnosticWindowFrames_;
    if (diagnosticLastX_ != 0.0f || diagnosticLastY_ != 0.0f) {
        ++diagnosticTotalMovingFrames_;
        ++diagnosticWindowMovingFrames_;
    }

    const bool reportIntervalElapsed = diagnosticReportElapsedSeconds_ >= kDiagnosticsReportIntervalSeconds;
    const bool diagnosticTimeElapsed = diagnosticElapsedSeconds_ >= kDiagnosticsDurationSeconds;
    if (reportIntervalElapsed || diagnosticTimeElapsed) {
        const float intervalYawChange = MouseLookController::wrapRadians(
            camera.yaw() - diagnosticWindowStartYaw_);
        const float intervalPitchChange = camera.pitch() - diagnosticWindowStartPitch_;
        std::fprintf(stderr,
                     "[MOUSE_DIAG] t=%5.2f/%2.0fs frames=%3llu moving=%3llu "
                     "last=(%+7.2f,%+7.2f) sum=(%+8.2f,%+8.2f) "
                     "rangeX=[%+6.2f,%+6.2f] rangeY=[%+6.2f,%+6.2f] peak=(%6.2f,%6.2f) "
                     "dAngle=(yaw=%+6.2f,pitch=%+6.2f)deg yaw=%+7.2f pitch=%+6.2f "
                     "facing=(%+.3f,%+.3f)\n",
                     diagnosticElapsedSeconds_, kDiagnosticsDurationSeconds,
                     static_cast<unsigned long long>(diagnosticWindowFrames_),
                     static_cast<unsigned long long>(diagnosticWindowMovingFrames_),
                     static_cast<double>(diagnosticLastX_), static_cast<double>(diagnosticLastY_),
                     diagnosticWindowX_, diagnosticWindowY_,
                     static_cast<double>(diagnosticWindowMinimumX_), static_cast<double>(diagnosticWindowMaximumX_),
                     static_cast<double>(diagnosticWindowMinimumY_), static_cast<double>(diagnosticWindowMaximumY_),
                     static_cast<double>(diagnosticWindowPeakX_), static_cast<double>(diagnosticWindowPeakY_),
                     static_cast<double>(intervalYawChange * kRadiansToDegrees),
                     static_cast<double>(intervalPitchChange * kRadiansToDegrees),
                     static_cast<double>(camera.yaw() * kRadiansToDegrees),
                     static_cast<double>(camera.pitch() * kRadiansToDegrees),
                     static_cast<double>(camera.facing().x), static_cast<double>(camera.facing().y));
        std::fflush(stderr);

        diagnosticReportElapsedSeconds_ = 0.0;
        diagnosticWindowX_ = 0.0;
        diagnosticWindowY_ = 0.0;
        diagnosticWindowPeakX_ = 0.0f;
        diagnosticWindowPeakY_ = 0.0f;
        diagnosticWindowMinimumX_ = 0.0f;
        diagnosticWindowMaximumX_ = 0.0f;
        diagnosticWindowMinimumY_ = 0.0f;
        diagnosticWindowMaximumY_ = 0.0f;
        diagnosticWindowStartYaw_ = camera.yaw();
        diagnosticWindowStartPitch_ = camera.pitch();
        diagnosticWindowFrames_ = 0;
        diagnosticWindowMovingFrames_ = 0;
    }

    if (diagnosticTimeElapsed) {
        std::fprintf(stderr,
                     "[MOUSE_DIAG] COMPLETE: frames=%llu moving=%llu totalDelta=(%+.2f,%+.2f) "
                     "maxAbsFrameDelta=(%.2f,%.2f). No more mouse diagnostics this launch.\n\n",
                     static_cast<unsigned long long>(diagnosticTotalFrames_),
                     static_cast<unsigned long long>(diagnosticTotalMovingFrames_),
                     diagnosticTotalX_, diagnosticTotalY_,
                     static_cast<double>(diagnosticOverallPeakX_),
                     static_cast<double>(diagnosticOverallPeakY_));
        std::fflush(stderr);
        diagnosticsFinished_ = true;
    }
}

const char* MouseInput::backendName() const noexcept {
    return "Gridfall-patched Raylib SDL relative mouse (accumulated xrel/yrel)";
}

void MouseInput::shutdown() noexcept {
    setCaptured(false);
    initialized_ = false;
}

} // namespace gridfall
