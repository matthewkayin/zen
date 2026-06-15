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

    VkCommandPool graphics_command_pool;

    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    VkPhysicalDeviceMemoryProperties memory_properties;

    VkFormat depth_format;
};

struct VulkanImage {
    VkImage handle;
    VkDeviceMemory memory;
    VkImageView view;
    uint32_t width;
    uint32_t height;
};

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

struct VulkanFramebuffer {
    VkFramebuffer handle;
    uint32_t attachment_count;
    VkImageView* attachments;
    VulkanRenderpass* renderpass;
};

struct VulkanSwapchain {
    VkSurfaceFormatKHR image_format;
    uint8_t max_frames_in_flight;
    VkSwapchainKHR handle;
    uint32_t image_count;
    VkImage* images;
    VkImageView* views;

    VulkanImage depth_attachment;

    VulkanFramebuffer* framebuffers;
};

enum class VulkanCommandBufferState {
    READY,
    RECORDING,
    IN_RENDER_PASS,
    RECORDING_ENDED,
    SUBMITTED,
    NOT_ALLOCATED
};

struct VulkanCommandBuffer {
    VkCommandBuffer handle;
    VulkanCommandBufferState state;
};

struct VulkanContext {
    VkInstance instance;
    VkAllocationCallbacks* allocator;
    VkSurfaceKHR surface;
    VulkanDevice device;
    VulkanSwapchain swapchain;
    VulkanRenderpass main_renderpass;
    VulkanCommandBuffer* graphics_command_buffers;

    VkSemaphore* acquire_semaphores;
    VkSemaphore* submit_semaphores;
    VkFence* frame_fences;

    uint32_t image_index;
    uint32_t frame_index;
    bool is_recreating_swapchain;

    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint64_t framebuffer_size_generation;
    uint64_t framebuffer_size_last_generation;

    VkDebugUtilsMessengerEXT debug_messenger;
};
