#pragma once

#include "math/mat4.h"

struct SDL_Window;

struct RenderPacket {
    double delta_time;
};

bool renderer_init(SDL_Window* window);
void renderer_quit();

void renderer_on_resized();
bool renderer_draw_frame(RenderPacket* packet);
void renderer_set_view(mat4 view);
