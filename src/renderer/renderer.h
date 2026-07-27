#pragma once

struct SDL_Window;

bool renderer_init(SDL_Window* window);
void renderer_quit();

void renderer_on_resized();

void renderer_draw_frame(double delta_time);
