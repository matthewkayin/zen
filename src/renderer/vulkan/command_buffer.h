#pragma once

#include "renderer/vulkan/types.h"

void vulkan_command_buffer_allocate(
    VulkanContext* context,
    VkCommandPool pool,
    VkCommandBufferLevel level,
    VulkanCommandBuffer* out_command_buffer);
void vulkan_command_buffer_free(VulkanContext* context, VkCommandPool pool, VulkanCommandBuffer* command_buffer);

void vulkan_command_buffer_begin_recording(VulkanCommandBuffer* command_buffer, VkCommandBufferUsageFlags flags);
void vulkan_command_buffer_end_recording(VulkanCommandBuffer* command_buffer);
void vulkan_command_buffer_set_submitted(VulkanCommandBuffer* command_buffer);
void vulkan_command_buffer_reset(VulkanCommandBuffer* command_buffer);

void vulkan_command_buffer_begin_renderpass(
    VulkanCommandBuffer* command_buffer,
    VulkanRenderpass* renderpass,
    VkFramebuffer framebuffer);
void vulkan_command_buffer_end_renderpass(VulkanCommandBuffer* command_buffer);

void vulkan_command_buffer_single_use_alloc_and_begin(
    VulkanContext* context,
    VkCommandPool pool,
    VulkanCommandBuffer* out_command_buffer);
void vulkan_command_buffer_single_use_end(
    VulkanCommandBuffer* command_buffer,
    VkQueue queue);
