#include "frontend.h"

#include "core/logger.h"
#include "renderer/backend.h"

bool renderer_begin_frame(double delta_time);
bool renderer_end_frame(double delta_time);

static IRendererBackend* backend = nullptr;

bool renderer_init(SDL_Window* window) {
    backend = IRendererBackend::create(RendererBackendType::VULKAN, window);
    if (backend == nullptr) {
        log_error("Failed to create backend.");
        return false;
    }

    if (!backend->init()) {
        log_error("Renderer backend init failed.");
        delete backend;
        return false;
    }

    return true;
}

void renderer_quit() {
    backend->quit();
    delete backend;
}

void renderer_on_resized() {
    if (!backend) {
        log_warn("renderer_on_resized called with null backend.");
        return;
    }

    backend->on_resized();
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
