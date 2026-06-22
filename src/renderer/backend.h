#pragma once

#include "renderer/shader_types.h"
#include <cstdint>

struct SDL_Window;

enum class RendererBackendType {
    VULKAN
};

class IRendererBackend {
public:
    static IRendererBackend* create(RendererBackendType type,
                                    SDL_Window* window);

    IRendererBackend() = default;
    virtual ~IRendererBackend() = default;

    // No copy
    IRendererBackend(const IRendererBackend&) = delete;
    IRendererBackend& operator=(const IRendererBackend&) = delete;

    virtual bool init() = 0;
    virtual void quit() = 0;

    virtual void on_resized() = 0;
    virtual bool begin_frame(double delta_time) = 0;
    virtual bool end_frame(double delta_time) = 0;

    virtual void update_global_state(GlobalUniformObject global_ubo) = 0;

    SDL_Window* m_window;
    uint64_t m_frame_number;
};
