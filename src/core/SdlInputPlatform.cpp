#include "gridfall/core/MouseInput.hpp"
#include <SDL.h>
#include <cstdio>
#include <cstring>
namespace gridfall {
void MouseInput::configurePlatformBeforeWindow() noexcept {
    const char* requestedDriver = SDL_getenv("SDL_VIDEODRIVER");
    if (requestedDriver && requestedDriver[0] != '\0') {
        std::fprintf(stderr, "[SDL_INPUT] Respecting explicit SDL_VIDEODRIVER=%s\n", requestedDriver);
        std::fflush(stderr);
        return;
    }
    const char* waylandDisplay = SDL_getenv("WAYLAND_DISPLAY");
    if (!waylandDisplay || waylandDisplay[0] == '\0') return;
    bool available = false;
    const int count = SDL_GetNumVideoDrivers();
    for (int i=0; i<count; ++i) {
        const char* driver = SDL_GetVideoDriver(i);
        if (driver && std::strcmp(driver, "wayland") == 0) { available = true; break; }
    }
    if (!available) {
        std::fprintf(stderr, "[SDL_INPUT] WAYLAND_DISPLAY is set but this SDL2 build has no Wayland driver; using SDL default.\n");
        std::fflush(stderr);
        return;
    }
    const SDL_bool driverHint = SDL_SetHintWithPriority(SDL_HINT_VIDEODRIVER,"wayland",SDL_HINT_OVERRIDE);
    const SDL_bool relativeHint = SDL_SetHintWithPriority(SDL_HINT_MOUSE_RELATIVE_MODE_WARP,"0",SDL_HINT_OVERRIDE);
    std::fprintf(stderr,"[SDL_INPUT] WSLg Wayland available; native relative motion requested (driverHint=%s, rawRelativeHint=%s).\n",
                 driverHint==SDL_TRUE?"set":"rejected",relativeHint==SDL_TRUE?"set":"rejected");
    std::fflush(stderr);
}
} // namespace gridfall
