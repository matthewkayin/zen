#pragma once

#include "types.h"
#include <cstdint>

struct StaticMeshData;
struct SDL_Window;

bool renderer_init(SDL_Window* window);
void renderer_quit();

void renderer_on_resized(uint32_t width, uint32_t height);

bool renderer_draw_frame(RenderPacket* packet);
