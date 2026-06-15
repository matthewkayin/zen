#include "frontend.h"

#include "renderer/backend.h"

#include "core/logger.h"

static IRendererBackend* backend = nullptr;

bool renderer_begin_frame(double delta_time);
bool renderer_end_frame(double delta_time);

bool renderer_init(SDL_Window* window) {
    backend = renderer_backend_create(RENDERER_BACKEND_TYPE_VULKAN, window);
    if (backend == nullptr) {
        log_error("Failed to alloc backend.");
        return false;
    }

    if (!backend->init()) {
        renderer_backend_destroy(backend);
        log_error("Renderer backend failed to initialize.");
        return false;
    }

    return true;
}

void renderer_quit() {
    backend->quit();
    renderer_backend_destroy(backend);
}

void renderer_on_resized(uint32_t width, uint32_t height) {
    if (!backend) {
        log_warn("renderer_on_resized - backend does not exist.");
        return;
    }

    backend->on_resized(width, height);
}

bool renderer_draw_frame(RenderPacket* packet) {
    if (!renderer_begin_frame(packet->delta_time)) {
        log_warn("renderer_begin_frame failed.");
        return true;
    }

    if (!renderer_end_frame(packet->delta_time)) {
        log_error("renderer_end_frame failed. Application shutting down.");
        return false;
    }

    return true;
}

bool renderer_begin_frame(double delta_time) {
    return backend->begin_frame(delta_time);
}

bool renderer_end_frame(double delta_time) {
    bool result = backend->end_frame(delta_time);
    backend->m_frame_number++;
    return result;
}
