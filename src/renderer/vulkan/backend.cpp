#include "backend.h"

#include "defines.h"
#include "core/logger.h"
#include "types.h"

static VulkanContext context;

bool RendererBackendVulkan::init() {
    // Setup Vulkan instance
    VkApplicationInfo app_info;
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pNext = nullptr;
    app_info.pApplicationName = ZEN_APP_NAME;
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "Zen Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_4;

    VkInstanceCreateInfo create_info;
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pNext = nullptr;
    create_info.flags = 0;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledLayerCount = 0;
    create_info.ppEnabledLayerNames = 0;
    create_info.enabledExtensionCount = 0;
    create_info.ppEnabledExtensionNames = 0;

    VkResult result = vkCreateInstance(&create_info, nullptr, &context.instance);
    if (result != VK_SUCCESS) {
        log_error("vkCreateInstance failed with result %u", result);
    }

    log_info("Vulkan backend initialized successfully.");
    return true;
}

void RendererBackendVulkan::quit() {

}

void RendererBackendVulkan::on_resized(uint32_t width, uint32_t height) {

}

bool RendererBackendVulkan::begin_frame(double delta_time) {
    return true;
}

bool RendererBackendVulkan::end_frame(double delta_time) {
    return true;
}
