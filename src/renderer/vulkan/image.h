#pragma once

#include "renderer/vulkan/types.h"

bool vulkan_image_create(
    VulkanContext* context,
    VkImageType image_type,
    uint32_t width,
    uint32_t height,
    VkFormat format,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags memory_property_flags,
    bool create_view,
    VkImageAspectFlags view_aspect_flags,
    VulkanImage* out_image);

void vulkan_image_view_create(
    VulkanContext* context,
    VkFormat format,
    VulkanImage* image,
    VkImageAspectFlags aspect_flags);

void vulkan_image_destroy(VulkanContext* context, VulkanImage* image);
