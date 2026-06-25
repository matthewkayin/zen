#include "frontend.h"

#include "core/logger.h"
#include "renderer/backend.h"
#include "math/quat.h"

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
        return true;
    }

    backend->update_global_state({
        .projection = mat4::perspective(45.0f * ZEN_DEG_TO_RAD, 1280.0f / 720.0f, 0.1f, 1000.0f),
        .view = mat4::translation(vec3(0.0f, 0.0f, 30.0f)).inversed()
    });

    static float angle = 0.01f;
    angle += 1.0f * packet->delta_time;
    quat rotation = quat::from_axis_angle(vec3::forward(), angle, false);
    mat4 model = rotation.to_rotation_matrix(vec3(0.0f));
    backend->update_object(model);

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
