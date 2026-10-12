#include "gridfall/core/Controls.hpp"
#include "gridfall/core/MouseInput.hpp"

#include <raylib.h>
#include <cmath>
#include <string>

using namespace gridfall;

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1040, 700, "GRIDFALL // CENTER-RELATIVE POINTER PROBE");
    if (!IsWindowReady()) return 1;
    SetExitKey(KEY_NULL);
    SetTargetFPS(144);

    MouseInput mouse;
    (void)mouse.initialize();
    MouseLookController look;
    look.reset(0.0f, 0.0f);

    bool captureRequested = false;
    bool cursorCaptured = false;
    MouseDelta latest{};

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_SPACE)) captureRequested = !captureRequested;
        if (IsKeyPressed(KEY_R)) {
            look.reset(0.0f, 0.0f);
            latest = {};
        }
        if (IsKeyPressed(KEY_ESCAPE)) captureRequested = false;

        const bool shouldCapture = captureRequested && IsWindowFocused();
        if (shouldCapture != cursorCaptured) {
            cursorCaptured = shouldCapture;
            mouse.setCaptured(shouldCapture);
        }

        latest = cursorCaptured ? mouse.consumeDelta() : MouseDelta{};
        look.update(latest.x, latest.y);
        const Vec2 facing = look.facing();

        BeginDrawing();
        ClearBackground(Color{19, 23, 29, 255});
        DrawText("GRIDFALL / CENTER-RELATIVE MOUSE DIAGNOSTIC", 28, 24, 25, RAYWHITE);
        DrawText(TextFormat("Backend: %s", mouse.backendName()), 30, 66, 18, GREEN);
        DrawText("SPACE: capture/release    R: reset yaw/pitch    ESC: release cursor", 30, 98, 16, LIGHTGRAY);
        DrawText(TextFormat("Capture: %s    Window focus: %s", cursorCaptured ? "ON" : "OFF",
                            IsWindowFocused() ? "YES" : "NO"), 30, 130, 17, RAYWHITE);
        DrawText(TextFormat("Input path: %s", mouse.backendName()), 30, 162, 16, Color{195, 211, 228, 255});
        DrawText("SPACE captures. Move the hidden pointer from center; a stationary mouse must not rotate.",
                 30, 188, 15, LIGHTGRAY);
        DrawText("Gameplay has an automatic 10-second terminal input trace.",
                 30, 212, 14, Color{184, 198, 214, 255});

        DrawText("LIVE DELTAS (center-relative pixels per poll)", 30, 232, 18, Color{150, 193, 230, 255});
        DrawText(TextFormat("dx: %8.2f", latest.x), 30, 266, 22, RAYWHITE);
        DrawText(TextFormat("dy: %8.2f", latest.y), 30, 298, 22, RAYWHITE);
        DrawText(TextFormat("yaw:   % .4f rad", look.yaw()), 30, 344, 20, RAYWHITE);
        DrawText(TextFormat("pitch: % .4f rad", look.pitch()), 30, 374, 20, RAYWHITE);
        DrawText(TextFormat("facing: x=% .3f  z=% .3f", facing.x, facing.y), 30, 404, 18, RAYWHITE);

        const int centerX = 760;
        const int centerY = 380;
        DrawCircleLines(centerX, centerY, 128.0f, Color{80, 91, 104, 255});
        DrawLine(centerX - 145, centerY, centerX + 145, centerY, Color{55, 65, 77, 255});
        DrawLine(centerX, centerY - 145, centerX, centerY + 145, Color{55, 65, 77, 255});
        DrawText("FORWARD", centerX - 42, centerY - 178, 17, LIGHTGRAY);
        DrawText("LEFT", centerX - 190, centerY - 8, 16, LIGHTGRAY);
        DrawText("RIGHT", centerX + 150, centerY - 8, 16, LIGHTGRAY);
        const Vector2 origin{static_cast<float>(centerX), static_cast<float>(centerY)};
        const Vector2 tip{centerX + facing.x * 115.0f, centerY - facing.y * 115.0f};
        DrawLineEx(origin, tip, 7.0f, Color{245, 173, 76, 255});
        DrawCircleV(tip, 9.0f, RAYWHITE);
        DrawCircleV(origin, 5.0f, Color{245, 173, 76, 255});
        DrawText("The arrow must rotate both ways and pitch must respond only to vertical motion.",
                 30, 620, 16, LIGHTGRAY);
        DrawText("Test: RIGHT, LEFT, UP, DOWN, then UP-RIGHT, UP-LEFT, DOWN-RIGHT, DOWN-LEFT.",
                 30, 648, 16, RAYWHITE);
        EndDrawing();
    }

    mouse.setCaptured(false);
    mouse.shutdown();
    CloseWindow();
    return 0;
}
