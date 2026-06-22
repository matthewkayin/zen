#pragma once

#include "core/asserts.h"
#include "renderer/shader_types.h"
#include <vector>
#include <vulkan/vulkan.h>

#define VK_CHECK(expr)                                                         \
    {                                                                          \
        ZEN_ASSERT(expr == VK_SUCCESS);                                        \
    }

// BUFFER

struct VulkanBuffer {
    uint64_t total_size;
    VkBuffer handle;
    VkBufferUsageFlags usage;
    VkDeviceMemory memory;
    uint32_t memory_index;
    VkMemoryPropertyFlags memory_property_flags;
    bool is_locked;
};

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
};

// COMMAND BUFFER

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

// SHADER MODULE

struct VulkanShaderStage {
    VkShaderModuleCreateInfo create_info;
    VkShaderModule handle;
    VkPipelineShaderStageCreateInfo shader_stage_create_info;
};

struct VulkanPipeline {
    VkPipeline handle;
    VkPipelineLayout pipeline_layout;
};

const uint32_t VULKAN_OBJECT_SHADER_STAGE_COUNT = 2;

struct VulkanObjectShader {
    VulkanShaderStage stages[VULKAN_OBJECT_SHADER_STAGE_COUNT];

    VkDescriptorPool global_descriptor_pool;
    VkDescriptorSetLayout global_descriptor_set_layout;
    VkDescriptorSet* global_descriptor_sets;
    GlobalUniformObject global_ubo;
    VulkanBuffer global_uniform_buffer;

    VulkanPipeline pipeline;
};

// CONTEXT

struct VulkanContext {
    VkInstance instance;
    VkAllocationCallbacks* allocator;
    VkSurfaceKHR surface;
    VulkanDevice device;
    VulkanSwapchain swapchain;
    VulkanRenderpass main_renderpass;

    VulkanFramebuffer* framebuffers;
    VulkanCommandBuffer* graphics_command_buffers;
    VkSemaphore* acquire_semaphores;
    VkSemaphore* submit_semaphores;
    VkFence* frame_fences;

    // Debug
    VkDebugUtilsMessengerEXT debug_messenger;

    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint64_t framebuffer_size_generation;
    uint64_t framebuffer_size_last_generation;

    uint32_t frame_index;
    uint32_t image_index;
    bool is_recreating_swapchain;

    VulkanObjectShader object_shader;
    VulkanBuffer object_vertex_buffer;
    VulkanBuffer object_index_buffer;
    uint64_t geometry_vertex_offset;
    uint64_t geometry_index_offset;
};
