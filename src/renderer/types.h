#pragma once

#include <cstdint>

struct SDL_Window;

enum RendererBackendType {
    RENDERER_BACKEND_TYPE_VULKAN
};

class IRendererBackend {
public:
    IRendererBackend() = default;
    virtual ~IRendererBackend() = default;

    // No copy
    IRendererBackend(const IRendererBackend&) = delete;
    IRendererBackend& operator=(const IRendererBackend&) = delete;

    virtual bool init() = 0;
    virtual void quit() = 0;

    virtual void on_resized(uint32_t width, uint32_t height) = 0;
    virtual bool begin_frame(double delta_time) = 0;
    virtual bool end_frame(double delta_time) = 0;

    SDL_Window* m_window;
    uint64_t m_frame_number;
};

struct RenderPacket {
    double delta_time;
};
