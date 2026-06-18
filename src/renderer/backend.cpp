#include "backend.h"

#include "renderer/vulkan/backend.h"

IRendererBackend* IRendererBackend::create(RendererBackendType type,
                                           SDL_Window* window) {
    IRendererBackend* backend = nullptr;

    switch (type) {
    case RendererBackendType::VULKAN: {
        backend = new VulkanBackend();
        break;
    }
    }

    if (backend == nullptr) {
        return nullptr;
    }

    backend->m_window = window;
    backend->m_frame_number = 0;

    return backend;
}
