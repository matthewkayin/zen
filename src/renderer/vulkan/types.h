#pragma once

#include "core/asserts.h"
#include <vector>
#include <vulkan/vulkan.h>

#define VK_CHECK(expr)                                                         \
    {                                                                          \
        ZEN_ASSERT(expr == VK_SUCCESS);                                        \
    }

// DEVICE

struct VulkanSwapchainSupportInfo {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

struct VulkanDevice {
    VkPhysicalDevice physical_device;
    VkDevice logical_device;
    VulkanSwapchainSupportInfo swapchain_support_info;

    uint32_t graphics_queue_index;
    uint32_t present_queue_index;
    uint32_t transfer_queue_index;

    VkQueue graphics_queue;
    VkQueue present_queue;
    VkQueue transfer_queue;

    VkCommandPool graphics_command_pool;

    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    VkPhysicalDeviceMemoryProperties memory_properties;

    VkFormat depth_format;
};

// CONTEXT

struct VulkanContext {
    VkInstance instance;
    VkAllocationCallbacks* allocator;
    VkSurfaceKHR surface;

    // Debug
    VkDebugUtilsMessengerEXT debug_messenger;
};
