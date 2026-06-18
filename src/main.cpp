#include "math/vec2.h"
#include "math/vec3.h"

#include "core/input.h"
#include "core/logger.h"
#include "renderer/frontend.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

struct GameState {
    SDL_Window* window;

    uint64_t last_time;
    uint64_t last_second;
    uint32_t frames;
    uint32_t updates;
    uint32_t fps;
    uint32_t ups;
};
static GameState state;

bool game_init();
void game_quit();
bool game_is_running();
double game_timekeep();

int main() {
    if (!game_init()) {
        return 1;
    }

    while (game_is_running()) {
        double delta_time = game_timekeep();
        input_poll_events();
        state.updates++;

        RenderPacket packet;
        packet.delta_time = delta_time;
        renderer_draw_frame(&packet);
        state.frames++;
    }

    game_quit();
    return 0;
}

bool game_init() {
    if (!logger_init()) {
        return false;
    }

    // Log initialization messages
    log_info("Initializing %s.", ZEN_APP_NAME);
    log_info("Detected platform %s.", ZEN_PLATFORM_STR);
    log_info("%s build.", ZEN_BUILD_STR);

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        log_error("Failed to initialize SDL: %s", SDL_GetError());
        return false;
    }

    // Check if Vulkan is supported
    if (!SDL_Vulkan_LoadLibrary(nullptr)) {
        log_error("Failed to load Vulkan library: %s", SDL_GetError());
    }

    // Create window
    const int window_width = 1280;
    const int window_height = 720;
    state.window = SDL_CreateWindow(ZEN_APP_NAME, window_width, window_height,
                                    SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (state.window == nullptr) {
        log_error("Error creating window: %s", SDL_GetError());
        return false;
    }

    // Init sub-systems
    input_init(state.window);
    if (!renderer_init(state.window)) {
        return false;
    }

    return true;
}

void game_quit() {
    renderer_quit();

    SDL_DestroyWindow(state.window);
    SDL_Quit();

    log_info("%s quit gracefully.", ZEN_APP_NAME);
    logger_quit();
}

bool game_is_running() { return !input_user_requests_exit(); }

double game_timekeep() {
    uint64_t current_time = SDL_GetTicksNS();
    double elapsed_time = (double)(current_time - state.last_time);
    state.last_time = current_time;

    double delta = elapsed_time / (double)SDL_NS_PER_SECOND;

    if (current_time - state.last_second >= SDL_NS_PER_SECOND) {
        state.fps = state.frames;
        state.ups = state.updates;
        state.frames = 0;
        state.updates = 0;
        state.last_second += SDL_NS_PER_SECOND;
    }

    return delta;
}
