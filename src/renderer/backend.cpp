#include "backend.h"

#include "vulkan/backend.h"
#include <SDL3/SDL.h>

IRendererBackend* renderer_backend_create(RendererBackendType type, SDL_Window* window) {
    IRendererBackend* backend = nullptr;

    // Alloc backend
    switch (type) {
        case RENDERER_BACKEND_TYPE_VULKAN: {
            backend = new RendererBackendVulkan();
        }
    }

    if (backend == nullptr) {
        return nullptr;
    }

    // Init backend
    backend->m_window = window;
    backend->m_frame_number = 0;

    return backend;
}

void renderer_backend_destroy(IRendererBackend* backend) {
    delete backend;
}
