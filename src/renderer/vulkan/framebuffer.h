#pragma once

#include "renderer/vulkan/types.h"

bool vulkan_framebuffer_create(
    VulkanContext* context,
    VulkanRenderpass* renderpass,
    uint32_t width, uint32_t height,
    uint32_t attachment_count,
    VkImageView* attachments,
    VulkanFramebuffer* out_framebuffer
);

void vulkan_framebuffer_destroy(VulkanContext* context, VulkanFramebuffer* framebuffer);
