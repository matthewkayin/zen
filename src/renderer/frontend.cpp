#include "frontend.h"

#include "core/logger.h"
#include "renderer/backend.h"
#include "math/quat.h"
#include <SDL3/SDL.h>

struct RendererFrontendState {
    IRendererBackend* backend = nullptr;
    mat4 projection;
    mat4 view;
};
static RendererFrontendState state;

bool renderer_init(SDL_Window* window) {
    state.backend = IRendererBackend::create(RendererBackendType::VULKAN, window);
    if (state.backend == nullptr) {
        log_error("Failed to create backend.");
        return false;
    }

    if (!state.backend->init()) {
        log_error("Renderer backend init failed.");
        delete state.backend;
        return false;
    }

    state.projection = mat4::perspective(45.0f * ZEN_DEG_TO_RAD, 1280.0f / 720.0f, 0.1f, 1000.0f);
    state.view = mat4::translation(vec3(0.0f, 0.0f, 30.0f)).inversed();

    return true;
}

void renderer_quit() {
    state.backend->quit();
    delete state.backend;
}

void renderer_on_resized() {
    if (!state.backend) {
        log_warn("renderer_on_resized called with null backend.");
        return;
    }

    int window_width, window_height;
    SDL_GetWindowSize(state.backend->m_window, &window_width, &window_height);
    state.projection =
        mat4::perspective(45.0f * ZEN_DEG_TO_RAD, (float)window_width / (float)window_height, 0.1, 1000.0f);
    state.backend->on_resized();
}

bool renderer_draw_frame(RenderPacket* packet) {
    if (!state.backend->begin_frame(packet->delta_time)) {
        return true;
    }

    state.backend->update_global_state({
        .projection = state.projection,
        .view = state.view
    });

    static float angle = 0.01f;
    // angle += 1.0f * packet->delta_time;
    quat rotation = quat::from_axis_angle(vec3::forward(), angle, false);
    mat4 model = rotation.to_rotation_matrix(vec3(0.0f));
    state.backend->update_object(model);

    bool success = state.backend->end_frame(packet->delta_time);
    state.backend->m_frame_number++;
    if (!success) {
        log_error("renderer_end_frame failed. Application shutting down.");
        return false;
    }

    return true;
}

void renderer_set_view(mat4 view) {
    state.view = view;
}
