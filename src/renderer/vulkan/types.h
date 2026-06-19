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
    uint32_t compute_queue_index;

    VkQueue graphics_queue;
    VkQueue present_queue;
    VkQueue transfer_queue;

    VkCommandPool graphics_command_pool;

    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    VkPhysicalDeviceMemoryProperties memory_properties;

    VkFormat depth_format;
};

// IMAGE

struct VulkanImage {
    VkImage handle;
    VkDeviceMemory memory;
    VkImageView view;
    uint32_t width;
    uint32_t height;
};

// RENDERPASS

enum class VulkanRenderpassState {
    READY,
    RECORDING,
    IN_RENDER_PASS,
    RECORDING_ENDED,
    SUBMITTED,
    NOT_ALLOCATED
};

struct VulkanRenderpass {
    VkRenderPass handle;
    float x, y, w, h;
    float r, g, b, a;
    float depth;
    uint32_t stencil;
    VulkanRenderpassState state;
};

// FRAMEBUFFER

struct VulkanFramebuffer {
    VkFramebuffer handle;
    uint32_t attachment_count;
    VkImageView* attachments;
    VulkanRenderpass* renderpass;
};

// SWAPCHAIN

struct VulkanSwapchain {
    VkSurfaceFormatKHR image_format;
    uint8_t max_frames_in_flight;
    VkSwapchainKHR handle;

    VulkanImage depth_attachment;

    uint32_t image_count;
    VkImage* images;
    VkImageView* views;
    VulkanFramebuffer* framebuffers;
};

// CONTEXT

struct VulkanContext {
    VkInstance instance;
    VkAllocationCallbacks* allocator;
    VkSurfaceKHR surface;
    VulkanDevice device;
    VulkanSwapchain swapchain;

    // Debug
    VkDebugUtilsMessengerEXT debug_messenger;

    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint64_t framebuffer_size_generation;
    uint64_t framebuffer_size_last_generation;

    uint32_t frame_index;
    uint32_t image_index;
    bool is_recreating_swapchain;
};
