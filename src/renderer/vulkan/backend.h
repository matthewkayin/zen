#pragma once

#include "renderer/vulkan/types.h"
#include "renderer/types.h"

class RendererBackendVulkan : public IRendererBackend {
public:
    RendererBackendVulkan() = default;
    ~RendererBackendVulkan() override = default;

    bool init() override;
    void quit() override;
    void on_resized(uint32_t width, uint32_t height) override;
    bool begin_frame(double delta_time) override;
    bool end_frame(double delta_time) override;
private:
    VulkanContext m_context;
};
