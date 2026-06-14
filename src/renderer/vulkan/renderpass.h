#pragma once

#include "renderer/vulkan/types.h"

void vulkan_renderpass_create(
    VulkanContext* context,
    VulkanRenderpass* out_renderpass,
    float x, float y, float w, float h,
    float r, float g, float b, float a,
    float depth,
    uint32_t stencil);
void vulkan_renderpass_destroy(VulkanContext* context, VulkanRenderpass* renderpass);

void vulkan_renderpass_begin(
    VulkanCommandBuffer* command_buffer,
    VulkanRenderpass* renderpass,
    VkFramebuffer framebuffer);
void vulkan_renderpass_end(VulkanCommandBuffer* command_buffer, VulkanRenderpass* renderpass);
