#pragma once

#include "core/asserts.h"
#include "renderer/types.h"
#include <vulkan/vulkan.h>

#define VK_CHECK(expr)              \
 {                                  \
    ZEN_ASSERT(expr == VK_SUCCESS); \
 }

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
    VkInstance m_instance;
    VkDebugUtilsMessengerEXT m_debug_messenger;
};
