#include "gridfall/render/Renderer.hpp"
#include "gridfall/systems/EnemySystem.hpp"
#include "gridfall/systems/ProgressionSystem.hpp"

#include <raylib.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>

namespace gridfall {
namespace {
constexpr int kWindowWidth = 1360;
constexpr int kWindowHeight = 780;
constexpr float kTileWorld = static_cast<float>(kTileSize);

// New palette: warm ivory typography, muted graphite stone, amber controls and
// restrained coral warnings. The HUD is drawn as an overlay over the 3D scene.
constexpr Color kInk{13, 18, 26, 246};
constexpr Color kInkSoft{27, 35, 46, 236};
// Soft warm parchment is deliberately distinct from the graphite/navy surfaces.
// Secondary labels use a calm blue-slate, while semantic accent colors are reserved
// for clickable states, ammo types, warnings, and status indicators.
constexpr Color kIvory{239, 229, 208, 255};
constexpr Color kMuted{180, 199, 215, 255};
constexpr Color kAmber{235, 174, 105, 255};
constexpr Color kAmberBright{255, 211, 145, 255};
constexpr Color kCoral{239, 130, 126, 255};
constexpr Color kMint{164, 218, 183, 255};
constexpr Color kBlue{151, 202, 237, 255};

Font gUiFont{};
bool gUiFontLoaded = false;

Color rayColor(Rgba color) { return {color.r, color.g, color.b, color.a}; }
Vec2 lerpVec2(Vec2 from, Vec2 to, float amount) {
    return from + (to - from) * clampf(amount, 0.0f, 1.0f);
}
Vector3 toWorld(Vec2 p, float height = 0.12f) {
    return {p.x / kTileWorld, height, p.y / kTileWorld};
}
Vector3 tileCenter(int x, int y, float height = 0.0f) {
    return {static_cast<float>(x) + 0.5f, height, static_cast<float>(y) + 0.5f};
}
Camera3D makeFirstPersonCamera(Vec2 position, Vec2 facing, float pitch) {
    const Vector3 eye{position.x / kTileWorld, 0.98f, position.y / kTileWorld};
    const float cp = std::cos(pitch);
    const Vector3 forward{facing.x * cp, std::sin(pitch), facing.y * cp};
    Camera3D camera{};
    camera.position = eye;
    camera.target = {eye.x + forward.x, eye.y + forward.y, eye.z + forward.z};
    camera.up = {0.0f, 1.0f, 0.0f};
    camera.fovy = 78.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    return camera;
}

void text(const std::string& value, int x, int y, int size, Color color) {
    // System-font rendering is antialiased; a restrained one-pixel ink shadow
    // keeps the warmer foreground readable over both bright floors and dark panels.
    // Avoid micro-labels. Cool-pearl foreground and a soft shadow improve contrast.
    const int readableSize = std::max(size, 13);
    if (gUiFontLoaded) {
        const Vector2 position{static_cast<float>(x), static_cast<float>(y)};
        const float fontSize = static_cast<float>(readableSize);
        DrawTextEx(gUiFont, value.c_str(), {position.x + 1.0f, position.y + 1.0f},
                   fontSize, 0.25f, {7, 10, 15, static_cast<unsigned char>(color.a * 0.72f)});
        DrawTextEx(gUiFont, value.c_str(), position, fontSize, 0.25f, color);
    } else {
        DrawText(value.c_str(), x + 1, y + 1, readableSize, {8, 9, 11, static_cast<unsigned char>(color.a * 0.82f)});
        DrawText(value.c_str(), x, y, readableSize, color);
    }
}
int measureText(const char* value, int size) {
    const int readableSize = std::max(size, 13);
    if (gUiFontLoaded)
        return static_cast<int>(MeasureTextEx(gUiFont, value, static_cast<float>(readableSize), 0.25f).x);
    return MeasureText(value, readableSize);
}
void textCentered(const std::string& value, int y, int size, Color color) {
    const int width = GetScreenWidth() > 0 ? GetScreenWidth() : kWindowWidth;
    text(value, (width - measureText(value.c_str(), size)) / 2, y, size, color);
}
void panel(int x, int y, int width, int height, Color color = kInkSoft) {
    DrawRectangleRounded({static_cast<float>(x), static_cast<float>(y),
                          static_cast<float>(width), static_cast<float>(height)},
                         0.12f, 8, color);
}
void bar(int x, int y, int width, int height, float amount, Color fill,
         Color background = {55, 54, 54, 235}) {
    DrawRectangleRounded({static_cast<float>(x), static_cast<float>(y),
                          static_cast<float>(width), static_cast<float>(height)}, 0.25f, 6, background);
    const int filled = static_cast<int>(static_cast<float>(width) * clampf(amount, 0.0f, 1.0f));
    if (filled > 0) {
        DrawRectangleRounded({static_cast<float>(x), static_cast<float>(y),
                              static_cast<float>(filled), static_cast<float>(height)}, 0.25f, 6, fill);
    }
}
Color weaponColor(AmmoType type) {
    switch (type) {
        case AmmoType::FastFire: return kAmberBright;
        case AmmoType::Piercing: return kBlue;
        case AmmoType::SlowSteady: return kCoral;
    }
    return kIvory;
}
float weaponWidth(AmmoType type) {
    return type == AmmoType::SlowSteady ? 0.09f : 0.055f;
}

struct SkillCardRect {
    int x{};
    int y{};
    int width{};
    int height{};

    [[nodiscard]] bool contains(int px, int py) const noexcept {
        return px >= x && px <= x + width && py >= y && py <= y + height;
    }
};

SkillCardRect skillCardRect(int index, int screenWidth, int screenHeight) noexcept {
    const int panelW = std::min(1100, screenWidth - 28);
    const int panelH = std::min(650, screenHeight - 28);
    const int px = (screenWidth - panelW) / 2;
    const int py = (screenHeight - panelH) / 2;
    const int contentX = px + 24;
    const int contentW = panelW - 48;
    constexpr int gap = 13;
    const int cardW = (contentW - gap * 2) / 3;
    const int cardH = std::max(104, std::min(122, (panelH - 205) / 3));
    const int firstY = py + 130;
    const int branch = index / 3;
    const int tier = index % 3;
    return {contentX + branch * (cardW + gap), firstY + tier * (cardH + 10), cardW, cardH};
}

} // namespace

void Renderer::initialize() {
    if (gUiFontLoaded) return;
    // Prefer Lato Medium for a rounded, humanist UI feel. Load a high-resolution
    // glyph atlas and use bilinear texture filtering so small labels stay soft rather
    // than razor-sharp. The fallback fonts preserve readability without bundled assets.
    constexpr const char* candidates[]{
        "/usr/share/fonts/truetype/lato/Lato-Medium.ttf",
        "/usr/share/fonts/truetype/lato/Lato-Semibold.ttf",
        "/usr/share/fonts/truetype/lato/Lato-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf"
    };
    for (const char* path : candidates) {
        if (!FileExists(path)) continue;
        Font font = LoadFontEx(path, 64, nullptr, 0);
        if (font.texture.id == 0) continue;
        gUiFont = font;
        SetTextureFilter(gUiFont.texture, TEXTURE_FILTER_BILINEAR);
        gUiFontLoaded = true;
        break;
    }
}

void Renderer::shutdown() {
    if (gUiFontLoaded) UnloadFont(gUiFont);
    gUiFont = Font{};
    gUiFontLoaded = false;
}

int Renderer::titleMenuItemAt(int mouseX, int mouseY) const noexcept {
    const int sw = GetScreenWidth() > 0 ? GetScreenWidth() : kWindowWidth;
    const int sh = GetScreenHeight() > 0 ? GetScreenHeight() : kWindowHeight;
    const int panelX = 32;
    const int panelW = std::min(470, sw - 64);
    const int panelY = std::max(24, (sh - 640) / 2);
    const int menuY = panelY + 250;
    const int buttonX = panelX + 24;
    const int buttonW = panelW - 48;
    for (int index = 0; index < 3; ++index) {
        const int buttonY = menuY + index * 62;
        if (mouseX >= buttonX && mouseX <= buttonX + buttonW &&
            mouseY >= buttonY && mouseY <= buttonY + 48) return index;
    }
    return -1;
}

int Renderer::skillTreeItemAt(int mouseX, int mouseY) const noexcept {
    const int sw = GetScreenWidth() > 0 ? GetScreenWidth() : kWindowWidth;
    const int sh = GetScreenHeight() > 0 ? GetScreenHeight() : kWindowHeight;
    for (int index = 0; index < static_cast<int>(kSkillCount); ++index) {
        if (skillCardRect(index, sw, sh).contains(mouseX, mouseY)) return index;
    }
    return -1;
}

void Renderer::updateTitleAim(float mouseX, float mouseY, float frameDt) noexcept {
    const float sw = static_cast<float>(GetScreenWidth() > 0 ? GetScreenWidth() : kWindowWidth);
    const float sh = static_cast<float>(GetScreenHeight() > 0 ? GetScreenHeight() : kWindowHeight);
    const float avatarX = sw * 0.70f;
    const float avatarY = sh * 0.52f;
    const float targetYaw = clampf((mouseX - avatarX) / std::max(1.0f, sw * 0.30f), -1.0f, 1.0f) * 0.92f;
    const float targetPitch = clampf((avatarY - mouseY) / std::max(1.0f, sh * 0.42f), -1.0f, 1.0f) * 0.23f;
    const float dt = clampf(frameDt, 0.0f, 0.1f);
    const float smooth = 1.0f - std::exp(-9.0f * dt);
    titleAimYaw_ += (targetYaw - titleAimYaw_) * smooth;
    titleAimPitch_ += (targetPitch - titleAimPitch_) * smooth;
    titleIdleTime_ += dt;
}

void Renderer::recordPerformanceSample(PerformanceSample sample) noexcept {
    performanceSamples_[performanceWriteIndex_] = sample;
    performanceWriteIndex_ = (performanceWriteIndex_ + 1) % kPerfHistory;
    performanceSampleCount_ = std::min(kPerfHistory, performanceSampleCount_ + 1);
}

void Renderer::exportPerformanceCsv(const char* path) const {
    if (path == nullptr || *path == '\0') return;
    try {
        const std::filesystem::path destination(path);
        if (destination.has_parent_path()) std::filesystem::create_directories(destination.parent_path());
        std::ofstream output(destination);
        if (!output) return;
        output << "sample,frame_ms,update_ms,render_cpu_ms,present_ms,input_to_submit_ms\n";
        const std::size_t start = (performanceWriteIndex_ + kPerfHistory - performanceSampleCount_) % kPerfHistory;
        for (std::size_t i = 0; i < performanceSampleCount_; ++i) {
            const PerformanceSample& sample = performanceSamples_[(start + i) % kPerfHistory];
            output << i << ',' << sample.frameMs << ',' << sample.updateMs << ','
                   << sample.renderCpuMs << ',' << sample.presentMs << ',' << sample.inputToSubmitMs << '\n';
        }
    } catch (...) {
        // Telemetry export must never interrupt the game loop.
    }
}

void Renderer::draw(const Level& level, const Player& player, const std::vector<Enemy>& enemies,
                    const std::vector<EnemyProjectile>& enemyProjectiles,
                    const CombatSystem& combat, const VfxSystem& vfx, GameState state,
                    int levelNumber, int unlockedLevel, const std::string& toast, float toastTimer,
                    float interpolationAlpha, float cameraPitch, bool skillTreeOpen,
                    bool hasSave, int menuSelection, const std::string& saveNotice) const {
    ClearBackground({17, 19, 23, 255});

    if (state == GameState::Title) {
        drawTitleShowcase(player);
        drawOverlay(state, levelNumber, toast, toastTimer, hasSave, menuSelection, saveNotice);
        drawPerformanceOverlay();
        return;
    }

    const float alpha = clampf(interpolationAlpha, 0.0f, 1.0f);
    Player renderedPlayer = player;
    renderedPlayer.position = lerpVec2(player.previousPosition, player.position, alpha);
    // Mouse-look is updated directly from relative input at render rate. Do not
    // interpolate its facing again: that would add visible aim lag.
    renderedPlayer.facing = player.facing;
    const Camera3D camera = makeFirstPersonCamera(renderedPlayer.position, renderedPlayer.facing, cameraPitch);

    BeginMode3D(camera);
    drawMap(level, renderedPlayer.position, renderedPlayer.facing);
    drawVfx(vfx);
    drawProjectiles(combat, alpha);
    drawEnemyProjectiles(enemyProjectiles, alpha);
    for (const Enemy& enemy : enemies) {
        if (!enemy.alive) continue;
        const Vec2 offset = enemy.position - renderedPlayer.position;
        const float distance2 = lengthSquared(offset);
        const float maxDistance = 15.0f * kTileSize;
        if (distance2 > maxDistance * maxDistance) continue;
        if (distance2 > 4.0f * kTileSize * kTileSize &&
            offset.x * renderedPlayer.facing.x + offset.y * renderedPlayer.facing.y < -0.25f * kTileSize) continue;
        Enemy renderedEnemy = enemy;
        renderedEnemy.position = lerpVec2(enemy.previousPosition, enemy.position, alpha);
        drawEnemy(renderedEnemy);
    }
    EndMode3D();

    // A camera-locked weapon gives first-person feedback without submitting more
    // world geometry. HUD panels are drawn afterwards so the weapon never hides text.
    drawViewWeapon(player);
    drawHud(player, enemies, state, levelNumber, unlockedLevel);

    const int centerX = GetScreenWidth() / 2;
    const int centerY = GetScreenHeight() / 2;
    DrawLine(centerX - 13, centerY, centerX - 4, centerY, kIvory);
    DrawLine(centerX + 4, centerY, centerX + 13, centerY, kIvory);
    DrawLine(centerX, centerY - 13, centerX, centerY - 4, kIvory);
    DrawLine(centerX, centerY + 4, centerX, centerY + 13, kIvory);
    DrawCircleLines(centerX, centerY, 3.0f, Fade(kAmberBright, 0.95f));

    if (toastTimer > 0.0f && !toast.empty() && state == GameState::Playing) {
        const int width = std::min(620, measureText(toast.c_str(), 15) + 42);
        const int x = (GetScreenWidth() - width) / 2;
        panel(x, 84, width, 34, {24, 24, 28, 228});
        DrawRectangleRounded({static_cast<float>(x + 10), 95.0f, 4.0f, 12.0f}, 0.5f, 4, kAmber);
        text(toast, x + 24, 93, 15, kIvory);
    }
    drawOverlay(state, levelNumber, toast, toastTimer, hasSave, menuSelection, saveNotice);
    if (skillTreeOpen) drawSkillTree(player);
    drawPerformanceOverlay();
}

void Renderer::drawTitleShowcase(const Player& player) const {
    const float sw = static_cast<float>(GetScreenWidth() > 0 ? GetScreenWidth() : kWindowWidth);
    const float sh = static_cast<float>(GetScreenHeight() > 0 ? GetScreenHeight() : kWindowHeight);
    ClearBackground({15, 17, 22, 255});

    Camera3D camera{};
    camera.position = {12.5f, 3.1f, 13.0f};
    camera.target = {12.5f, 0.85f, 6.7f};
    camera.up = {0.0f, 1.0f, 0.0f};
    camera.fovy = 44.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    BeginMode3D(camera);
    DrawPlane({12.5f, -0.04f, 7.0f}, {14.0f, 12.0f}, {25, 28, 33, 255});
    DrawPlane({12.5f, -0.025f, 7.0f}, {7.8f, 7.8f}, {39, 41, 45, 255});
    DrawCube({12.5f, 1.40f, 1.45f}, 13.5f, 2.8f, 0.28f, {29, 32, 38, 255});
    DrawCube({12.5f, 2.82f, 1.45f}, 13.5f, 0.06f, 0.34f, kAmber);
    DrawCube({7.2f, 1.35f, 1.70f}, 0.24f, 2.6f, 0.24f, {60, 65, 74, 255});
    DrawCube({17.8f, 1.35f, 1.70f}, 0.24f, 2.6f, 0.24f, {60, 65, 74, 255});
    DrawCylinder({12.5f, 0.10f, 7.0f}, 1.10f, 1.18f, 0.18f, 48, {55, 56, 60, 255});
    DrawCylinderWires({12.5f, 0.11f, 7.0f}, 1.12f, 1.20f, 0.20f, 48, {118, 91, 59, 255});
    DrawSphere({9.0f, 2.6f, 4.2f}, 0.18f, {239, 166, 82, 110});
    DrawSphere({16.0f, 2.3f, 4.0f}, 0.14f, {132, 187, 223, 105});

    Player preview = player;
    preview.position = {12.5f * kTileSize, 6.95f * kTileSize};
    preview.previousPosition = preview.position;
    preview.velocity = {};
    preview.animationTime = titleIdleTime_;
    preview.facing = {std::sin(titleAimYaw_), std::cos(titleAimYaw_)};
    drawPlayer(preview, titleAimPitch_);
    EndMode3D();

    // Keep the showcase crisp and dark but reserve the whole right side for the
    // responsive operator preview instead of exposing a zoomed-out level map.
    DrawRectangleGradientV(0, 0, static_cast<int>(sw), static_cast<int>(sh),
                           {8, 10, 14, 24}, {8, 10, 14, 120});
    DrawRectangle(0, 0, 500, static_cast<int>(sh), {17, 19, 23, 35});
}

void Renderer::drawMap(const Level& level, Vec2 cameraPosition, Vec2 cameraFacing) const {
    // The old renderer submitted a floor cube and wireframe for all 450 tiles
    // every frame. A single floor plane plus a lightweight grid removes hundreds
    // of unnecessary 3D primitive submissions while retaining the grid aesthetic.
    const LevelPalette& theme = level.palette();
    DrawPlane({static_cast<float>(kMapWidth) * 0.5f, -0.02f,
               static_cast<float>(kMapHeight) * 0.5f},
              {static_cast<float>(kMapWidth), static_cast<float>(kMapHeight)}, rayColor(theme.floor));

    const Color gridLine = rayColor(theme.grid);
    const int centerX = static_cast<int>(cameraPosition.x / kTileWorld);
    const int centerZ = static_cast<int>(cameraPosition.y / kTileWorld);
    constexpr int gridRadius = 12;
    for (int x = std::max(0, centerX - gridRadius); x <= std::min(kMapWidth, centerX + gridRadius); ++x) {
        DrawLine3D({static_cast<float>(x), -0.006f, static_cast<float>(std::max(0, centerZ - gridRadius))},
                   {static_cast<float>(x), -0.006f, static_cast<float>(std::min(kMapHeight, centerZ + gridRadius))}, gridLine);
    }
    for (int z = std::max(0, centerZ - gridRadius); z <= std::min(kMapHeight, centerZ + gridRadius); ++z) {
        DrawLine3D({static_cast<float>(std::max(0, centerX - gridRadius)), -0.006f, static_cast<float>(z)},
                   {static_cast<float>(std::min(kMapWidth, centerX + gridRadius)), -0.006f, static_cast<float>(z)}, gridLine);
    }

    // Only tiles with visible geometry submit per-tile primitives. The floor is
    // already present underneath the entire map.
    const float cameraX = cameraPosition.x / kTileWorld;
    const float cameraZ = cameraPosition.y / kTileWorld;
    constexpr float visibleDistanceSquared = 15.0f * 15.0f;
    for (int z = std::max(0, centerZ - gridRadius); z < std::min(kMapHeight, centerZ + gridRadius + 1); ++z) {
        for (int x = std::max(0, centerX - gridRadius); x < std::min(kMapWidth, centerX + gridRadius + 1); ++x) {
            const float dx = static_cast<float>(x) + 0.5f - cameraX;
            const float dz = static_cast<float>(z) + 0.5f - cameraZ;
            const float distance2 = dx * dx + dz * dz;
            if (distance2 > visibleDistanceSquared) continue;
            // The FPS camera cannot see directly behind itself; allow a wide
            // cone so nearby geometry near the edge of the viewport remains safe.
            if (distance2 > 4.0f && dx * cameraFacing.x + dz * cameraFacing.y < -0.20f) continue;
            const Tile tile = level.tileAt(x, z);
            if (tile == Tile::Floor) continue;
            const Vector3 c = tileCenter(x, z, 0.0f);
            switch (tile) {
                case Tile::Wall: {
                    DrawCube({c.x, 0.70f, c.z}, 0.96f, 1.42f, 0.96f, rayColor(theme.wall));
                    DrawCube({c.x, 1.38f, c.z}, 0.99f, 0.12f, 0.99f, rayColor(theme.wallCap));
                    DrawCube({c.x, 0.98f, c.z + 0.486f}, 0.75f, 0.055f, 0.018f, rayColor(theme.wallDetail));
                    break;
                }
                case Tile::SecretWall: {
                    DrawCube({c.x, 0.70f, c.z}, 0.96f, 1.42f, 0.96f, rayColor(theme.secret));
                    DrawCube({c.x, 1.38f, c.z}, 0.99f, 0.12f, 0.99f, rayColor(theme.accent));
                    DrawCube({c.x, 0.70f, c.z + 0.49f}, 0.73f, 0.045f, 0.02f, rayColor(theme.accent));
                    DrawSphere({c.x + 0.30f, 1.18f, c.z + 0.50f}, 0.055f, rayColor(theme.accent));
                    break;
                }
                case Tile::SideDoor: {
                    DrawCube({c.x, 0.68f, c.z}, 0.88f, 1.30f, 0.88f, rayColor(theme.door));
                    DrawCube({c.x, 1.32f, c.z}, 0.98f, 0.12f, 0.98f, rayColor(theme.accent));
                    DrawCube({c.x, 0.65f, c.z + 0.45f}, 0.62f, 0.85f, 0.04f, rayColor(theme.wallDetail));
                    DrawSphere({c.x + 0.20f, 0.67f, c.z + 0.49f}, 0.045f, rayColor(theme.accent));
                    break;
                }
                case Tile::Spikes: {
                    DrawCube({c.x, 0.015f, c.z}, 0.86f, 0.045f, 0.86f, rayColor(theme.hazard));
                    for (int spikeZ = -1; spikeZ <= 1; ++spikeZ) {
                        for (int spikeX = -1; spikeX <= 1; ++spikeX) {
                            if (spikeX == 0 && spikeZ == 0) continue;
                            if ((spikeX + spikeZ) % 2 == 0) continue;
                            DrawCylinder({c.x + static_cast<float>(spikeX) * 0.21f, 0.10f,
                                          c.z + static_cast<float>(spikeZ) * 0.21f},
                                         0.0f, 0.085f, 0.24f, 5, rayColor(theme.hazard));
                        }
                    }
                    break;
                }
                case Tile::Poison: {
                    DrawCylinder({c.x, 0.005f, c.z}, 0.43f, 0.43f, 0.035f, 20, rayColor(theme.poison));
                    DrawCylinder({c.x - 0.14f, 0.04f, c.z + 0.10f}, 0.12f, 0.12f, 0.035f, 12, rayColor(theme.accent));
                    DrawSphere({c.x + 0.17f, 0.20f, c.z - 0.11f}, 0.055f, rayColor(theme.accent));
                    DrawSphere({c.x - 0.20f, 0.13f, c.z - 0.16f}, 0.035f, rayColor(theme.accent));
                    break;
                }
                case Tile::Exit: {
                    DrawCube({c.x, 0.015f, c.z}, 0.88f, 0.05f, 0.88f, rayColor(theme.wallCap));
                    DrawCubeWires({c.x, 0.045f, c.z}, 0.75f, 0.04f, 0.75f, rayColor(theme.accent));
                    DrawCubeWires({c.x, 0.05f, c.z}, 0.45f, 0.04f, 0.45f, rayColor(theme.accent));
                    DrawCylinderEx({c.x, 0.06f, c.z}, {c.x, 1.5f, c.z}, 0.035f, 0.02f, 8, Fade(rayColor(theme.accent), 0.48f));
                    DrawSphere({c.x, 1.42f, c.z}, 0.09f, rayColor(theme.accent));
                    break;
                }
                case Tile::Cache: {
                    DrawCube({c.x, 0.22f, c.z}, 0.47f, 0.38f, 0.38f, rayColor(theme.door));
                    DrawCube({c.x, 0.32f, c.z + 0.20f}, 0.48f, 0.08f, 0.025f, rayColor(theme.accent));
                    DrawCube({c.x, 0.22f, c.z + 0.21f}, 0.06f, 0.19f, 0.025f, kIvory);
                    break;
                }
                case Tile::AmmoCache: {
                    DrawCube({c.x, 0.23f, c.z}, 0.52f, 0.40f, 0.44f, {61, 77, 83, 255});
                    DrawCube({c.x, 0.44f, c.z}, 0.54f, 0.06f, 0.46f, rayColor(theme.accent));
                    for (int bullet = 0; bullet < 3; ++bullet)
                        DrawCylinder({c.x - 0.16f + bullet * 0.16f, 0.53f, c.z}, 0.035f, 0.035f, 0.16f, 8, kAmberBright);
                    break;
                }
                case Tile::AmmoLight:
                case Tile::AmmoPiercing:
                case Tile::AmmoHeavy: {
                    const Color ammoColor = tile == Tile::AmmoLight ? kAmberBright :
                                            (tile == Tile::AmmoPiercing ? kBlue : kCoral);
                    for (int bullet = 0; bullet < 3; ++bullet)
                        DrawCylinder({c.x - 0.12f + bullet * 0.12f, 0.15f, c.z}, 0.035f, 0.035f, 0.20f, 8, ammoColor);
                    DrawCylinder({c.x, 0.04f, c.z}, 0.18f, 0.18f, 0.035f, 12, Fade(ammoColor, 0.55f));
                    break;
                }
                case Tile::HealthPotion:
                    DrawCylinder({c.x, 0.11f, c.z}, 0.12f, 0.15f, 0.25f, 10, {180, 76, 72, 255});
                    DrawCylinder({c.x, 0.255f, c.z}, 0.07f, 0.07f, 0.055f, 8, {226, 193, 155, 255});
                    DrawCube({c.x, 0.14f, c.z + 0.15f}, 0.035f, 0.10f, 0.018f, {255, 220, 190, 255});
                    break;
                case Tile::RegenPotion:
                    DrawCylinder({c.x, 0.11f, c.z}, 0.12f, 0.15f, 0.25f, 10, {88, 132, 163, 255});
                    DrawCylinder({c.x, 0.255f, c.z}, 0.07f, 0.07f, 0.055f, 8, {226, 193, 155, 255});
                    DrawSphere({c.x, 0.20f, c.z + 0.15f}, 0.045f, {190, 224, 239, 255});
                    break;
                case Tile::Floor: break;
            }
        }
    }
}

void Renderer::drawPlayer(const Player& player, float weaponPitch) const {
    const Vector3 p = toWorld(player.position, 0.05f);
    const float moving = std::min(1.0f, length(player.velocity) / std::max(1.0f, player.moveSpeed));
    const float bob = moving > 0.05f ? std::fabs(std::sin(player.animationTime * 13.0f)) * 0.045f : 0.0f;
    const Color body = player.damageFlash > 0.0f ? kCoral : Color{214, 190, 146, 255};

    DrawCylinder({p.x, 0.025f, p.z}, 0.24f, 0.27f, 0.04f, 16, {25, 24, 24, 150}); // ground shadow
    DrawCylinder({p.x, 0.08f + bob, p.z}, 0.17f, 0.23f, 0.58f, 12, {94, 89, 81, 255});
    DrawCylinderWires({p.x, 0.08f + bob, p.z}, 0.17f, 0.23f, 0.58f, 12, {43, 39, 36, 255});
    DrawSphere({p.x, 0.76f + bob, p.z}, 0.185f, body);
    DrawSphere({p.x + player.facing.x * 0.075f, 0.79f + bob, p.z + player.facing.y * 0.075f},
               0.052f, {249, 237, 207, 255});

    const float pitch = clampf(weaponPitch, -0.28f, 0.28f);
    const float pitchScale = std::cos(pitch);
    const Vector3 gunStart{p.x + player.facing.x * 0.10f * pitchScale, 0.48f + bob + std::sin(pitch) * 0.10f,
                           p.z + player.facing.y * 0.10f * pitchScale};
    const Vector3 gunEnd{p.x + player.facing.x * 0.57f * pitchScale, 0.44f + bob + std::sin(pitch) * 0.57f,
                         p.z + player.facing.y * 0.57f * pitchScale};
    DrawCylinderEx(gunStart, gunEnd, weaponWidth(player.ammoType()), weaponWidth(player.ammoType()) * 0.70f,
                   7, weaponColor(player.ammoType()));
    if (player.reloading) {
        const float progress = 1.0f - player.reloadTimer / std::max(0.01f, player.reloadDuration);
        DrawCube({p.x, 1.06f, p.z}, 0.42f, 0.045f, 0.065f, {57, 54, 49, 255});
        DrawCube({p.x - 0.21f + 0.42f * clampf(progress, 0.0f, 1.0f), 1.06f, p.z},
                 0.42f * clampf(progress, 0.0f, 1.0f), 0.05f, 0.07f, kAmberBright);
    }
}

void Renderer::drawViewWeapon(const Player& player) const {
    const int sw = GetScreenWidth();
    const int sh = GetScreenHeight();
    const float sway = std::sin(player.animationTime * 10.0f) *
                       std::min(1.0f, length(player.velocity) / std::max(1.0f, player.moveSpeed));
    const float reloadDrop = player.reloading ? 30.0f : 0.0f;
    const float baseX = static_cast<float>(sw) * 0.735f + sway * 10.0f;
    const float baseY = static_cast<float>(sh) * 0.79f + reloadDrop;
    const Color accent = weaponColor(player.ammoType());
    const Color metal{34, 37, 43, 255};
    const Color edge{104, 108, 115, 255};

    // Stylized, camera-locked side of a weapon/forearm. Simple 2D geometry keeps
    // the viewmodel cheap and stable in WSLg; weapon class changes the accent.
    DrawTriangle({baseX + 12.0f, baseY + 156.0f}, {baseX + 88.0f, baseY + 90.0f},
                 {baseX + 164.0f, baseY + 210.0f}, {70, 63, 55, 255});
    DrawRectangleRounded({baseX + 60.0f, baseY + 65.0f, 190.0f, 145.0f}, 0.16f, 8, metal);
    DrawRectangleRounded({baseX + 80.0f, baseY + 38.0f, 144.0f, 54.0f}, 0.18f, 8, edge);
    DrawRectangleRounded({baseX + 107.0f, baseY + 16.0f, 124.0f, 25.0f}, 0.18f, 8, metal);
    DrawRectangleRounded({baseX + 122.0f, baseY + 19.0f, 92.0f, 9.0f}, 0.4f, 5, accent);
    DrawRectangleRounded({baseX + 84.0f, baseY + 101.0f, 142.0f, 8.0f}, 0.25f, 5, accent);
    DrawRectangleRounded({baseX + 99.0f, baseY + 120.0f, 114.0f, 45.0f}, 0.15f, 7, {23, 25, 29, 255});
    DrawLineEx({baseX + 109.0f, baseY + 132.0f}, {baseX + 203.0f, baseY + 132.0f}, 2.0f, edge);
    if (player.reloading) {
        const float progress = 1.0f - player.reloadTimer / std::max(0.01f, player.reloadDuration);
        DrawRectangleRounded({baseX + 92.0f, baseY + 174.0f, 105.0f, 8.0f}, 0.25f, 5, {65, 58, 48, 255});
        DrawRectangleRounded({baseX + 92.0f, baseY + 174.0f, 105.0f * clampf(progress, 0.0f, 1.0f), 8.0f},
                             0.25f, 5, kAmberBright);
    }
}

void Renderer::drawEnemy(const Enemy& enemy) const {
    if (!enemy.alive) return;
    const Vector3 p = toWorld(enemy.position, 0.04f);
    const Color flesh = enemy.hitFlash > 0.0f ? Color{255, 225, 205, 255} : Color{166, 73, 67, 255};
    const float ratio = clampf(enemy.health / std::max(1.0f, enemy.maxHealth), 0.0f, 1.0f);
    DrawCylinder({p.x, 0.025f, p.z}, 0.23f, 0.26f, 0.04f, 14, {18, 17, 18, 130});
    DrawCylinder({p.x, 0.07f, p.z}, 0.16f, 0.22f, 0.48f, 10, {72, 48, 47, 255});
    DrawSphere({p.x, 0.64f, p.z}, 0.17f, flesh);
    DrawSphere({p.x - 0.065f, 0.67f, p.z + 0.105f}, 0.028f, kAmberBright);
    DrawSphere({p.x + 0.065f, 0.67f, p.z + 0.105f}, 0.028f, kAmberBright);
    DrawCube({p.x, 1.00f, p.z}, 0.43f, 0.045f, 0.055f, {46, 33, 32, 245});
    if (ratio > 0.0f) DrawCube({p.x - 0.215f + 0.43f * ratio * 0.5f, 1.00f, p.z}, 0.43f * ratio, 0.048f, 0.06f, kCoral);
}

void Renderer::drawProjectiles(const CombatSystem& combat, float interpolationAlpha) const {
    for (const Projectile& projectile : combat.projectiles()) {
        const Vec2 renderPosition = lerpVec2(projectile.previous, projectile.position, interpolationAlpha);
        const Vector3 a = toWorld(projectile.previous, 0.42f);
        const Vector3 b = toWorld(renderPosition, 0.42f);
        const Color color = rayColor(projectile.color);
        DrawLine3D(a, b, Fade(color, 0.92f));
        DrawSphere(b, projectile.type == AmmoType::SlowSteady ? 0.075f : 0.052f, color);
    }
}

void Renderer::drawEnemyProjectiles(const std::vector<EnemyProjectile>& projectiles, float interpolationAlpha) const {
    for (const EnemyProjectile& projectile : projectiles) {
        const Vec2 rendered = lerpVec2(projectile.previous, projectile.position, interpolationAlpha);
        const Vector3 a = toWorld(projectile.previous, 0.42f);
        const Vector3 b = toWorld(rendered, 0.42f);
        DrawLine3D(a, b, Fade(kCoral, 0.72f));
        DrawSphere(b, 0.062f, {255, 100, 105, 255});
    }
}

void Renderer::drawVfx(const VfxSystem& vfx) const {
    for (const Tracer& tracer : vfx.tracers()) {
        const float alpha = clampf(tracer.life / std::max(0.001f, tracer.maxLife), 0.0f, 1.0f);
        DrawLine3D(toWorld(tracer.from, 0.48f), toWorld(tracer.to, 0.48f), Fade(rayColor(tracer.color), alpha));
    }
    for (const Particle& particle : vfx.particles()) {
        const float alpha = clampf(particle.life / std::max(0.001f, particle.maxLife), 0.0f, 1.0f);
        const float height = 0.10f + (particle.maxLife - particle.life) * 0.60f;
        const float size = clampf(particle.size / 42.0f, 0.025f, 0.085f) * (0.35f + alpha * 0.65f);
        DrawSphere(toWorld(particle.position, height), size, Fade(rayColor(particle.color), alpha));
    }
    for (const MagazineDrop& magazine : vfx.magazines()) {
        const Vector3 p = toWorld(magazine.position, 0.04f + magazine.height / kTileWorld);
        const float fade = magazine.age > 1.15f ? clampf((2.35f - magazine.age) / 1.2f, 0.0f, 1.0f) : 1.0f;
        DrawCube({p.x, 0.025f, p.z}, 0.15f, 0.025f, 0.12f, Fade({12, 12, 13, 255}, 0.4f * fade));
        DrawCube({p.x, p.y, p.z}, 0.15f, 0.065f, 0.12f, Fade({155, 148, 134, 255}, fade));
        DrawCubeWires({p.x, p.y, p.z}, 0.15f, 0.065f, 0.12f, Fade({227, 191, 135, 255}, fade));
    }
}

void Renderer::drawHud(const Player& player, const std::vector<Enemy>& enemies,
                       GameState state, int levelNumber, int unlockedLevel) const {
    (void)state;
    const int sw = GetScreenWidth() > 0 ? GetScreenWidth() : kWindowWidth;
    const int sh = GetScreenHeight() > 0 ? GetScreenHeight() : kWindowHeight;
    const int pad = 20;
    const int topW = sw - pad * 2;

    // Top strip floats over the world rather than reserving a dedicated side panel.
    panel(pad, 16, topW, 66, kInk);
    DrawRectangleRounded({static_cast<float>(pad), 16.0f, 4.0f, 66.0f}, 0.45f, 8, kAmber);
    text("GRIDFALL", pad + 18, 25, 23, kIvory);
    text("BELOW THE LINE", pad + 20, 52, 10, kMuted);
    DrawLine(pad + 196, 29, pad + 196, 69, {82, 76, 67, 255});
    text(TextFormat("SECTOR %02d / %02d", levelNumber, kMaxLevels), pad + 215, 28, 15, kAmberBright);
    text(TextFormat("%02d HOSTILES", EnemySystem::aliveCount(enemies)), pad + 215, 51, 12,
         EnemySystem::aliveCount(enemies) > 0 ? kCoral : kMint);

    const int vitalsX = pad + 385;
    text("VITALS", vitalsX, 25, 12, kMuted);
    text(TextFormat("%.0f / %.0f", player.health, player.maxHealth), vitalsX + 65, 24, 16,
         player.health < 30.0f ? kCoral : kIvory);
    bar(vitalsX, 49, 208, 9, player.health / std::max(1.0f, player.maxHealth),
        player.health < 30.0f ? kCoral : kMint);
    const int magX = sw - 250;
    DrawLine(magX - 20, 28, magX - 20, 68, {82, 76, 67, 255});
    text(CombatSystem::spec(player.ammoType()).name, magX, 24, 12, weaponColor(player.ammoType()));
    text(TextFormat("%02d", player.activeWeapon().magazine), magX, 42, 23, kIvory);
    text(TextFormat("/ %03d", player.activeWeapon().reserve), magX + 45, 51, 12, kMuted);

    // Small in-world-context prompt and status chips.
    int chipX = pad + 4;
    if (player.poisoned) {
        panel(chipX, 95, 117, 26, {65, 44, 36, 225});
        text("POISONED", chipX + 10, 101, 13, {222, 151, 104, 255});
        chipX += 125;
    }
    if (player.regenRemaining > 0.0f) {
        panel(chipX, 95, 132, 26, {39, 50, 43, 225});
        text(TextFormat("REGEN %.1fs", player.regenRemaining), chipX + 10, 101, 13, kMint);
        chipX += 140;
    }
    if (player.reloading) {
        panel(chipX, 95, 126, 26, {61, 47, 35, 225});
        text("RELOADING...", chipX + 10, 101, 13, kAmberBright);
    }

    // Low profile footer lets the playable world occupy nearly all of the screen.
    panel(pad, sh - 72, topW, 54, kInk);
    text(TextFormat("RANK %d", player.rank), pad + 16, sh - 58, 14, kAmberBright);
    text(TextFormat("%d / %d XP", player.xp, player.xpToNext), pad + 96, sh - 57, 12, kMuted);
    bar(pad + 16, sh - 35, 176, 6, static_cast<float>(player.xp) / static_cast<float>(std::max(1, player.xpToNext)), kAmber);

    text(TextFormat("Q MEDKIT x%d", player.inventory.healthPotions), pad + 228, sh - 53, 12, {226, 156, 139, 255});
    text(TextFormat("T REGEN x%d", player.inventory.regenPotions), pad + 228, sh - 33, 12, kBlue);

    const int skillX = pad + 420;
    text(TextFormat("SKILL POINTS %d", player.skillPoints), skillX, sh - 51, 12,
         player.skillPoints > 0 ? kAmberBright : kMuted);
    text("K SKILL TREE  //  CLICK TO UNLOCK", skillX, sh - 31, 10, kMuted);

    const int weaponX = sw - 385;
    const char* weaponLabels[3] = {"1 FAST", "2 PIERCE", "3 LONGSHOT"};
    for (int i = 0; i < 3; ++i) {
        const int x = weaponX + i * 111;
        const bool active = i == player.weaponIndex;
        if (active) {
            panel(x, sh - 57, 103, 34, {76, 58, 39, 235});
            DrawRectangleRounded({static_cast<float>(x + 8), static_cast<float>(sh - 49), 3.0f, 18.0f}, 0.5f, 4, weaponColor(static_cast<AmmoType>(i)));
        }
        text(weaponLabels[i], x + (active ? 17 : 8), sh - 46, 11,
             active ? kIvory : kMuted);
    }
    const char* controls = "WASD MOVE   MOUSE LOOK / CENTER FIRE   E INTERACT   R RELOAD   F5 SAVE   P PAUSE";
    const int controlsWidth = std::min(sw - 32, measureText(controls, 12) + 28);
    const int controlsX = (sw - controlsWidth) / 2;
    panel(controlsX, sh - 101, controlsWidth, 24, {13, 19, 28, 238});
    text(controls, controlsX + 14, sh - 96, 12, {202, 218, 232, 255});
    text(TextFormat("UNLOCKED %02d/%02d", unlockedLevel, kMaxLevels), sw - 135, sh - 91, 10, kAmberBright);
}

void Renderer::drawPerformanceOverlay() const {
    const int sw = GetScreenWidth() > 0 ? GetScreenWidth() : kWindowWidth;
    const int x = std::max(8, sw - (performanceExpanded_ ? 294 : 135));
    const std::size_t count = std::min<std::size_t>(performanceSampleCount_, 240);
    PerformanceSample latest{};
    if (count > 0) {
        const std::size_t latestIndex = (performanceWriteIndex_ + kPerfHistory - 1) % kPerfHistory;
        latest = performanceSamples_[latestIndex];
    }
    const float fps = latest.frameMs > 0.001f ? 1000.0f / latest.frameMs : static_cast<float>(GetFPS());

    // The default counter is deliberately cheap. F10 opts into the rolling
    // percentile calculation and graph during a performance investigation.
    if (!performanceExpanded_) {
        panel(x, 92, 126, 42, {18, 19, 23, 226});
        text(TextFormat("%.0f FPS", fps), x + 8, 96, 13, fps >= 120.0f ? kMint : kIvory);
        text(TextFormat("%.2f ms  // F10", latest.frameMs), x + 8, 115, 10, kMuted);
        return;
    }

    std::array<float, 240> frameTimes{};
    float average = 0.0f;
    float maximum = 0.0f;
    int spikes = 0;
    const std::size_t oldest = (performanceWriteIndex_ + kPerfHistory - count) % kPerfHistory;
    for (std::size_t i = 0; i < count; ++i) {
        const PerformanceSample& sample = performanceSamples_[(oldest + i) % kPerfHistory];
        frameTimes[i] = sample.frameMs;
        average += sample.frameMs;
        maximum = std::max(maximum, sample.frameMs);
        if (sample.frameMs > 10.42f) ++spikes; // >1.5x the 144 Hz frame budget
    }
    if (count > 0) average /= static_cast<float>(count);
    std::sort(frameTimes.begin(), frameTimes.begin() + static_cast<std::ptrdiff_t>(count));
    const float p95 = count > 0 ? frameTimes[static_cast<std::size_t>((count - 1) * 0.95)] : 0.0f;
    const float p99 = count > 0 ? frameTimes[static_cast<std::size_t>((count - 1) * 0.99)] : 0.0f;

    constexpr int panelWidth = 284;
    constexpr int panelHeight = 186;
    panel(x, 92, panelWidth, panelHeight, {18, 19, 23, 238});
    text("FRAME TELEMETRY  // F10 COLLAPSE", x + 10, 99, 11, kAmberBright);
    text(TextFormat("FPS %.1f   FRAME %.2f ms", fps, latest.frameMs), x + 10, 118, 12,
         fps >= 120.0f ? kMint : (fps >= 60.0f ? kIvory : kCoral));
    text(TextFormat("UPDATE %.2f  DRAW CPU %.2f ms", latest.updateMs, latest.renderCpuMs), x + 10, 137, 10, kIvory);
    text(TextFormat("PRESENT/WAIT %.2f  INPUT->SUBMIT %.2f ms", latest.presentMs, latest.inputToSubmitMs), x + 10, 153, 9, kMuted);
    text(TextFormat("AVG %.2f  P95 %.2f  P99 %.2f ms", average, p95, p99), x + 10, 168, 10, kIvory);
    text(TextFormat("MAX %.2f ms  SPIKES >10.42ms: %d/%d", maximum, spikes, static_cast<int>(count)), x + 10, 183, 9,
         spikes > 0 ? kCoral : kMint);

    const int graphX = x + 10;
    const int graphY = 205;
    const int graphW = panelWidth - 20;
    const int graphH = 48;
    DrawRectangle(graphX, graphY, graphW, graphH, {30, 32, 37, 245});
    const float maxGraphMs = std::max(16.67f, maximum * 1.1f);
    const float budgetY = static_cast<float>(graphY + graphH) - (6.944f / maxGraphMs) * graphH;
    DrawLine(graphX, static_cast<int>(budgetY), graphX + graphW, static_cast<int>(budgetY), Fade(kMint, 0.70f));
    constexpr std::size_t maxGraphPoints = 90;
    const std::size_t graphPoints = std::min(count, maxGraphPoints);
    if (graphPoints > 1) {
        for (std::size_t i = 1; i < graphPoints; ++i) {
            const std::size_t priorSample = (i - 1) * (count - 1) / (graphPoints - 1);
            const std::size_t currentSample = i * (count - 1) / (graphPoints - 1);
            const float x0 = static_cast<float>(graphX) + static_cast<float>(i - 1) * graphW / static_cast<float>(graphPoints - 1);
            const float x1 = static_cast<float>(graphX) + static_cast<float>(i) * graphW / static_cast<float>(graphPoints - 1);
            const float t0 = clampf(performanceSamples_[(oldest + priorSample) % kPerfHistory].frameMs / maxGraphMs, 0.0f, 1.0f);
            const float t1 = clampf(performanceSamples_[(oldest + currentSample) % kPerfHistory].frameMs / maxGraphMs, 0.0f, 1.0f);
            DrawLineEx({x0, static_cast<float>(graphY + graphH) - t0 * graphH},
                       {x1, static_cast<float>(graphY + graphH) - t1 * graphH}, 1.5f,
                       t1 * maxGraphMs > 10.42f ? kCoral : kAmberBright);
        }
    }
    text("FRAME TIME HISTORY   F11 EXPORT CSV", x + 10, 257, 9, kMuted);
}

void Renderer::drawSkillTree(const Player& player) const {
    const int sw = GetScreenWidth() > 0 ? GetScreenWidth() : kWindowWidth;
    const int sh = GetScreenHeight() > 0 ? GetScreenHeight() : kWindowHeight;
    const int panelW = std::min(1100, sw - 28);
    const int panelH = std::min(650, sh - 28);
    const int px = (sw - panelW) / 2;
    const int py = (sh - panelH) / 2;

    DrawRectangle(0, 0, sw, sh, Fade({9, 10, 13, 255}, 0.80f));
    panel(px, py, panelW, panelH, {24, 25, 29, 252});
    DrawRectangleRounded({static_cast<float>(px), static_cast<float>(py), static_cast<float>(panelW), 5.0f},
                         0.5f, 8, kAmber);
    text("OPERATOR SKILL TREE", px + 26, py + 22, 27, kIvory);
    text(TextFormat("AVAILABLE POINTS  %d", player.skillPoints), px + panelW - 255, py + 29, 15,
         player.skillPoints > 0 ? kAmberBright : kMuted);
    text("Click an available node to spend points. Each branch unlocks in sequence.", px + 28, py + 60, 13, kMuted);

    const int gap = 13;
    const SkillCardRect firstCard = skillCardRect(0, sw, sh);
    const int contentX = px + 24;
    const int cardW = firstCard.width;
    const int cardH = firstCard.height;
    const int firstY = py + 130;
    constexpr std::array<Color, 3> branchColors{{kMint, kAmberBright, kBlue}};
    constexpr std::array<const char*, 3> branchNames{{"SURVIVAL", "ASSAULT", "MOBILITY"}};

    // Draw branch connectors before node cards so the prerequisites read visually
    // as a real tree instead of nine disconnected buttons.
    for (int branch = 0; branch < 3; ++branch) {
        const int x = contentX + branch * (cardW + gap);
        const int centerX = x + cardW / 2;
        for (int tier = 0; tier < 2; ++tier) {
            const int y1 = firstY + tier * (cardH + 10) + cardH;
            const int y2 = y1 + 10;
            const bool previousUnlocked = ProgressionSystem::hasSkill(player, static_cast<SkillId>(branch * 3 + tier));
            const Color lineColor = previousUnlocked ? branchColors[static_cast<std::size_t>(branch)] : Color{75, 73, 72, 255};
            DrawLine(centerX, y1, centerX, y2, lineColor);
        }
    }

    const Vector2 mouse = GetMousePosition();
    for (int branch = 0; branch < 3; ++branch) {
        const int x = contentX + branch * (cardW + gap);
        text(branchNames[static_cast<std::size_t>(branch)], x + 8, py + 103, 12,
             branchColors[static_cast<std::size_t>(branch)]);

        for (int tier = 0; tier < 3; ++tier) {
            const int idx = branch * 3 + tier;
            const SkillId id = static_cast<SkillId>(idx);
            const SkillDefinition& skill = ProgressionSystem::skillDefinition(id);
            const bool unlocked = ProgressionSystem::hasSkill(player, id);
            const bool available = ProgressionSystem::canPurchaseSkill(player, id);
            const SkillCardRect rect = skillCardRect(idx, sw, sh);
            const int y = rect.y;
            const bool hovered = rect.contains(static_cast<int>(mouse.x), static_cast<int>(mouse.y));
            const Color branchColor = branchColors[static_cast<std::size_t>(branch)];
            const Color baseBorder = unlocked ? branchColor :
                                     (available ? kAmber : Color{70, 70, 75, 255});
            const Color border = hovered ? (available || unlocked ? kAmberBright : kMuted) : baseBorder;
            Color fill = unlocked ? Color{40, 51, 44, 248} :
                         (available ? Color{48, 42, 32, 246} : Color{31, 32, 37, 242});
            if (hovered) fill = unlocked ? Color{50, 66, 53, 252} :
                                (available ? Color{65, 51, 35, 252} : Color{42, 43, 48, 248});
            panel(x, y, cardW, cardH, fill);
            DrawRectangleRoundedLines({static_cast<float>(x), static_cast<float>(y),
                                       static_cast<float>(cardW), static_cast<float>(cardH)},
                                      0.09f, 6, border);
            const char* tag = unlocked ? "OWNED" : (available ? "CLICK" : "LOCKED");
            text(tag, x + cardW - measureText(tag, 11) - 12, y + 10, 11, border);
            text(skill.name, x + 12, y + 11, 14, unlocked ? branchColor : kIvory);
            text(skill.description, x + 12, y + 39, 11, kMuted);

            if (skill.prerequisite != SkillId::Count && !ProgressionSystem::hasSkill(player, skill.prerequisite)) {
                text(TextFormat("REQUIRES: %s", ProgressionSystem::skillDefinition(skill.prerequisite).name),
                     x + 12, y + 59, 9, {191, 132, 105, 255});
            } else if (unlocked) {
                text("PASSIVE ACTIVE", x + 12, y + 59, 10, kMint);
            } else {
                text(TextFormat("COST: %d SKILL POINT%s", skill.cost, skill.cost == 1 ? "" : "S"),
                     x + 12, y + 59, 10, available ? kAmberBright : kMuted);
            }

            const char* status = unlocked ? "UNLOCKED" : (available ? "CLICK TO UNLOCK" : "LOCKED");
            text(status, x + 12, y + cardH - 21, 10, border);
        }
    }

    const int footerY = py + panelH - 26;
    text("CLICK AN AVAILABLE NODE TO UNLOCK", px + 26, footerY, 11, kMuted);
    text("K / ESC CLOSE", px + panelW - 142, footerY, 11, kAmberBright);
}

void Renderer::drawOverlay(GameState state, int levelNumber, const std::string& toast, float toastTimer,
                           bool hasSave, int menuSelection, const std::string& saveNotice) const {
    if (state == GameState::Playing) return;
    const int sw = GetScreenWidth() > 0 ? GetScreenWidth() : kWindowWidth;
    const int sh = GetScreenHeight() > 0 ? GetScreenHeight() : kWindowHeight;

    if (state == GameState::Title) {
        // Let the world and operator remain visible. The menu occupies only the
        // left side of the screen instead of obscuring the character with a modal.
        DrawRectangle(0, 0, sw, sh, Fade({10, 12, 15, 255}, 0.34f));
        const int panelX = 32;
        const int panelW = std::min(470, sw - 64);
        const int panelY = std::max(24, (sh - 640) / 2);
        const int panelH = std::min(640, sh - 48);
        panel(panelX, panelY, panelW, panelH, {22, 24, 28, 242});
        DrawRectangleRounded({static_cast<float>(panelX), static_cast<float>(panelY), 5.0f,
                              static_cast<float>(panelH)}, 0.45f, 8, kAmber);
        text("GRIDFALL", panelX + 28, panelY + 28, 47, kIvory);
        text("BELOW THE LINE", panelX + 32, panelY + 83, 18, kAmberBright);
        text("TACTICAL GRID ACTION // 20 SECTORS", panelX + 32, panelY + 121, 13, kMuted);
        DrawLine(panelX + 30, panelY + 158, panelX + panelW - 30, panelY + 158, {82, 76, 67, 255});
        text("DEPLOYMENT", panelX + 30, panelY + 181, 14, kMuted);

        constexpr const char* labels[3]{"CONTINUE  //  LOAD SAVE", "NEW GAME", "EXIT TO DESKTOP"};
        constexpr const char* hints[3]{"Resume the last saved run", "Begin again from Sector 01", "Close Gridfall"};
        const int buttonX = panelX + 24;
        const int buttonW = panelW - 48;
        const int menuY = panelY + 250;
        for (int index = 0; index < 3; ++index) {
            const int buttonY = menuY + index * 62;
            const bool selected = index == menuSelection;
            const bool disabled = index == 0 && !hasSave;
            const Color fill = selected ? Color{67, 52, 37, 246} : Color{32, 34, 39, 232};
            panel(buttonX, buttonY, buttonW, 48, fill);
            if (selected) {
                DrawRectangleRounded({static_cast<float>(buttonX), static_cast<float>(buttonY), 4.0f, 48.0f},
                                     0.45f, 6, kAmberBright);
            }
            const Color heading = disabled ? Color{103, 103, 106, 255} :
                                  (selected ? kAmberBright : kIvory);
            text(labels[index], buttonX + 18, buttonY + 4, 17, heading);
            text(hints[index], buttonX + 18, buttonY + 27, 12,
                 disabled ? Color{105, 105, 108, 255} : kMuted);
        }
        text(hasSave ? "SAVE FOUND  // CONTINUE AVAILABLE" : "NO SAVE YET  // START A NEW RUN",
             panelX + 30, panelY + panelH - 82, 11, hasSave ? kMint : kMuted);
        if (!saveNotice.empty()) text(saveNotice, panelX + 30, panelY + panelH - 61, 12, kAmberBright);
        text("UP / DOWN SELECT  ·  ENTER CONFIRM  ·  MOUSE CLICK", panelX + 30, panelY + panelH - 35, 10, kMuted);
        text("F5 SAVES DURING A RUN", panelX + 30, panelY + panelH - 17, 10, kMuted);
        text("OPERATOR PREVIEW", std::min(sw - 230, panelX + panelW + 28), sh - 42, 13, kIvory);
        return;
    }

    DrawRectangle(0, 0, sw, sh, Fade({12, 12, 15, 255}, 0.76f));
    const int cardW = std::min(740, sw - 56);
    const int cardH = 340;
    const int x = (sw - cardW) / 2;
    const int y = (sh - cardH) / 2;
    panel(x, y, cardW, cardH, {27, 27, 31, 250});
    DrawRectangleRounded({static_cast<float>(x), static_cast<float>(y), 5.0f, static_cast<float>(cardH)}, 0.45f, 8, kAmber);

    switch (state) {
        case GameState::Title: break; // handled above
        case GameState::Paused:
            textCentered("PAUSED", y + 95, 42, kIvory);
            textCentered("FIELD SIMULATION SUSPENDED", y + 155, 14, kMuted);
            textCentered("P / ESC RESUME     F5 SAVE RUN", y + 225, 17, kAmberBright);
            break;
        case GameState::LevelComplete:
            textCentered("SECTOR CLEARED", y + 76, 35, kMint);
            textCentered(TextFormat("SECTOR %02d / %02d COMPLETE", levelNumber, kMaxLevels), y + 130, 16, kIvory);
            textCentered("XP BANKED. FIELD KIT REPLENISHED.", y + 170, 14, kMuted);
            textCentered(levelNumber >= kMaxLevels ? "PRESS ENTER FOR FINAL DEBRIEF" : "PRESS ENTER TO ENTER NEXT SECTOR", y + 235, 16, kAmberBright);
            break;
        case GameState::GameOver:
            textCentered("SIGNAL LOST", y + 77, 39, kCoral);
            textCentered("OPERATOR DOWN  /  MISSION FAILED", y + 133, 15, kIvory);
            textCentered("PRESS ENTER TO REDEPLOY", y + 220, 17, kAmberBright);
            break;
        case GameState::Victory:
            textCentered("EXTRACTION COMPLETE", y + 77, 32, kMint);
            textCentered("ALL TWENTY SECTORS SECURED", y + 135, 17, kIvory);
            textCentered("YOU MADE IT BELOW THE LINE.", y + 175, 15, kMuted);
            textCentered("ENTER: NEW RUN    ·    ESC: MAIN MENU", y + 235, 17, kAmberBright);
            break;
        case GameState::Playing: break;
    }
    if (toastTimer > 0.0f && !toast.empty() && state != GameState::Title) {
        textCentered(toast, y + cardH - 34, 11, {215, 204, 187, 255});
    }
}

} // namespace gridfall
