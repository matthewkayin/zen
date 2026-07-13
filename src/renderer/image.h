#pragma once

#include <vulkan/vulkan.h>

struct VulkanImageTransitionLayoutParams {
    VkCommandBuffer command_buffer;
    VkImage image;
    VkImageLayout old_layout;
    VkImageLayout new_layout;
    VkAccessFlags2 src_access_mask;
    VkAccessFlags2 dst_access_mask;
    VkPipelineStageFlags2 src_stage_mask;
    VkPipelineStageFlags2 dst_stage_mask;
};

void vulkan_image_transition_layout(VulkanImageTransitionLayoutParams params);
