#pragma once

#include "renderer/backend.h"
#include "renderer/vulkan/types.h"

class VulkanBackend : public IRendererBackend {
public:
    VulkanBackend() = default;
    ~VulkanBackend() override = default;

    bool init() override;
    void quit() override;
    void on_resized() override;
    bool begin_frame(double delta_time) override;
    bool end_frame(double delta_time) override;

private:
    bool recreate_swapchain();
    bool create_swapchain_dependent_resources();
    void destroy_swapchain_dependent_resources();

    VulkanContext m_context;
};
