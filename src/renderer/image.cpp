#include "image.h"

void vulkan_image_transition_layout(VulkanImageTransitionLayoutParams params) {
    VkImageMemoryBarrier2 image_memory_barrier {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = params.src_stage_mask,
        .srcAccessMask = params.src_access_mask,
        .dstStageMask = params.dst_stage_mask,
        .dstAccessMask = params.dst_access_mask,
        .oldLayout = params.old_layout,
        .newLayout = params.new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = params.image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };
    VkDependencyInfo dependency_info {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &image_memory_barrier
    };
    vkCmdPipelineBarrier2(params.command_buffer, &dependency_info);
}
