#pragma once

#include <cstdint>

struct SDL_Window;

struct RenderPacket {
    double delta_time;
};

bool renderer_init(SDL_Window* window);
void renderer_quit();

void renderer_on_resized(uint32_t width, uint32_t height);

bool renderer_draw_frame(RenderPacket* packet);
