#pragma once

#include "renderer/backend.h"
#include "renderer/vulkan/types.h"

class VulkanBackend : public IRendererBackend {
public:
    VulkanBackend() = default;
    ~VulkanBackend() override = default;

    bool init() override;
    void quit() override;
    void on_resized(uint32_t width, uint32_t height) override;
    bool begin_frame(double delta_time) override;
    bool end_frame(double delta_time) override;

private:
    bool create_framebuffers(VulkanSwapchain* swapchain, VulkanRenderpass* renderpass);
    bool recreate_framebuffers(VulkanSwapchain* swapchain, VulkanRenderpass* renderpass);
    void destroy_framebuffers();

    VulkanContext m_context;
};
