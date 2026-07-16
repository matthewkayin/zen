#pragma once

#include "renderer/types.h"

struct VulkanImageCreateParams {
    uint32_t width;
    uint32_t height;
    VkFormat format;
    VkImageTiling tiling;
    VkImageUsageFlags usage;
    VkMemoryPropertyFlags memory_properties;
};

struct VulkanImageTransitionLayoutParams {
    VkCommandBuffer command_buffer;
    VkImage image;
    VkImageLayout old_layout;
    VkImageLayout new_layout;
};

struct VulkanImageTransitionLayoutExtParams {
    VkCommandBuffer command_buffer;
    VkImage image;
    VkImageLayout old_layout;
    VkImageLayout new_layout;
    VkAccessFlags2 src_access_mask;
    VkAccessFlags2 dst_access_mask;
    VkPipelineStageFlags2 src_stage_mask;
    VkPipelineStageFlags2 dst_stage_mask;
};

bool vulkan_image_create(VulkanContext* context, VulkanImageCreateParams params, VulkanImage* out_image);
bool vulkan_image_create_texture(VulkanContext* context, const char* path, VulkanImage* out_image);
void vulkan_image_destroy(VulkanContext* context, VulkanImage* image);
void vulkan_image_transition_layout(VulkanImageTransitionLayoutParams params);
void vulkan_image_transition_layout_ext(VulkanImageTransitionLayoutExtParams params);
