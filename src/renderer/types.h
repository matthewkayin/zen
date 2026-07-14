#pragma once

#include "core/asserts.h"
#include <vulkan/vulkan.h>
#include <vector>

#define VK_CHECK(expr)                  \
    {                                   \
        ZEN_ASSERT(expr == VK_SUCCESS); \
    }

const uint32_t VULKAN_MAX_FRAMES_IN_FLIGHT = 2U;

struct VulkanDevice {
    VkPhysicalDevice physical_device;
    VkDevice logical_device;

    uint32_t graphics_queue_index;
    uint32_t present_queue_index;
    uint32_t compute_queue_index;
    uint32_t transfer_queue_index;

    VkQueue graphics_queue;
    VkQueue present_queue;

    VkCommandPool graphics_command_pool;
};

struct VulkanSwapchain {
    VkSwapchainKHR handle;
    VkSurfaceFormatKHR image_format;
    VkExtent2D extent;
    std::vector<VkImage> images;
    std::vector<VkImageView> image_views;
};

struct VulkanPipeline {
    VkPipeline handle;
    VkPipelineLayout layout;
};

struct VulkanBuffer {
    VkBuffer handle;
    VkDeviceMemory memory;
};

struct VulkanContext {
    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_messenger;
    VkAllocationCallbacks* allocator;

    VkSurfaceKHR surface;
    uint32_t window_width;
    uint32_t window_height;

    VulkanDevice device;
    VulkanSwapchain swapchain;
    VulkanPipeline graphics_pipeline;

    VkSemaphore acquire_semaphores[VULKAN_MAX_FRAMES_IN_FLIGHT];
    std::vector<VkSemaphore> submit_semaphores;
    VkFence frame_fences[VULKAN_MAX_FRAMES_IN_FLIGHT];
    VkCommandBuffer graphics_command_buffers[VULKAN_MAX_FRAMES_IN_FLIGHT];
    uint32_t frame_index;
    uint32_t image_index;
};
