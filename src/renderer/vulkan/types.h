#pragma once

#include "core/asserts.h"
#include <vulkan/vulkan.h>

#define VK_CHECK(expr)              \
 {                                  \
    ZEN_ASSERT(expr == VK_SUCCESS); \
 }

struct VulkanSwapchainSupportInfo {
    VkSurfaceCapabilitiesKHR capabilities;
    uint32_t format_count;
    VkSurfaceFormatKHR* formats;
    uint32_t present_mode_count;
    VkPresentModeKHR* present_modes;
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

    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    VkPhysicalDeviceMemoryProperties memory_properties;
};

struct VulkanContext {
    VkInstance instance;
    VkAllocationCallbacks* allocator;
    VkSurfaceKHR surface;
    VulkanDevice device;

    VkDebugUtilsMessengerEXT debug_messenger;
};
