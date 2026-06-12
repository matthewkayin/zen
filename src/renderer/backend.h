#pragma once

#include "types.h"

IRendererBackend* renderer_backend_create(RendererBackendType type, SDL_Window* window);
void renderer_backend_destroy(IRendererBackend* backend);
