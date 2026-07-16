#include "image.h"

#include "core/logger.h"
#include "renderer/buffer.h"
#include "renderer/command_buffer.h"
#include "renderer/util.h"
#include "vulkan/vulkan_core.h"
#include <SDL3/SDL.h>

bool vulkan_image_create(VulkanContext* context, VulkanImageCreateParams params, VulkanImage* out_image) {
    // Create image
    VkImageCreateInfo image_create_info {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = params.format,
        .extent = {
            .width = params.width,
            .height = params.height,
            .depth = 1
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = params.tiling,
        .usage = params.usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VkResult create_result = vkCreateImage(
        context->device.logical_device, &image_create_info, context->allocator, &out_image->handle);
    if (create_result != VK_SUCCESS) {
        log_error("vulkan_image_create - Image creation failed with error %s.", vulkan_result_str(create_result));
        return false;
    }

    // Get image memory
    VkMemoryRequirements memory_reqeuirements;
    vkGetImageMemoryRequirements(context->device.logical_device, out_image->handle, &memory_reqeuirements);
    uint32_t memory_type_index = vulkan_find_memory_index(
        context, memory_reqeuirements.memoryTypeBits, params.memory_properties);
    if (memory_type_index == VULKAN_MEMORY_TYPE_INDEX_NOT_FOUND) {
        log_error("vulkan_image_create - The required memory type index was not found.");
        return false;
    }

    // Allocate image memory
    VkMemoryAllocateInfo allocate_info {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memory_reqeuirements.size,
        .memoryTypeIndex = memory_type_index
    };
    VkResult allocate_result = vkAllocateMemory(
        context->device.logical_device, &allocate_info, context->allocator, &out_image->memory);
    if (allocate_result != VK_SUCCESS) {
        log_error("vulkan_image_create - Memory allocation failed with error %s.", vulkan_result_str(allocate_result));
        return false;
    }

    // Bind image memory
    VK_CHECK(vkBindImageMemory(context->device.logical_device, out_image->handle, out_image->memory, 0));

    // Create image view
    vulkan_image_view_create(
        context,
        out_image->handle,
        VK_FORMAT_R8G8B8A8_SRGB,
        &out_image->view);

    return true;
}

bool vulkan_image_create_texture(VulkanContext* context, const char* path, VulkanImage* out_image) {
    // Load texture surface
    SDL_Surface* image_surface = SDL_LoadPNG(path);
    if (!image_surface) {
        log_error("Failed to load image at path %s.", path);
        return false;
    }
    if (image_surface->format != SDL_PIXELFORMAT_RGBA8888) {
        SDL_Surface* old_surface = image_surface;
        image_surface = SDL_ConvertSurface(old_surface, SDL_PIXELFORMAT_RGBA8888);
        SDL_DestroySurface(old_surface);
    }
    if (!image_surface) {
        log_error("Failed to convert image %s. %s", path, SDL_GetError());
        return false;
    }
    const size_t image_size = image_surface->w * image_surface->h * 4U;

    // Copy image data to staging buffer
    VulkanBuffer staging_buffer;
    vulkan_buffer_create(context, {
        .size = image_size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .memory_properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    }, &staging_buffer);
    vulkan_buffer_bind(context, &staging_buffer, 0);
    void* staging_buffer_data = vulkan_buffer_map_memory(context, &staging_buffer, {
        .offset = 0,
        .size = VK_WHOLE_SIZE
    });
    memcpy(staging_buffer_data, image_surface->pixels, image_size);
    vulkan_buffer_unmap_memory(context, &staging_buffer);

    // Create image
    bool image_create_succeeded = vulkan_image_create(context, {
        .width = (uint32_t)image_surface->w,
        .height = (uint32_t)image_surface->h,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    }, out_image);
    if (!image_create_succeeded) {
        return false;
    }

    // Copy image data to the VulkanImage
    VkCommandBuffer temp_command_buffer;
    vulkan_command_buffer_begin_single_use(context, &temp_command_buffer);

    // Transition layout to TRANSFER_DST
    vulkan_image_transition_layout({
        .command_buffer = temp_command_buffer,
        .image = out_image->handle,
        .old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
        .new_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    });

    // Copy from buffer into image
    VkBufferImageCopy copy_region {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        .imageOffset = { .x = 0, .y = 0, .z = 0 },
        .imageExtent = {
            .width = (uint32_t)image_surface->w,
            .height = (uint32_t)image_surface->h,
            .depth = 1
        }
    };

    vkCmdCopyBufferToImage(
        temp_command_buffer,
        staging_buffer.handle,
        out_image->handle,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &copy_region);

    // Transition layout to SHADER_READ_ONLY
    vulkan_image_transition_layout({
        .command_buffer = temp_command_buffer,
        .image = out_image->handle,
        .old_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    });

    vulkan_command_buffer_end_single_use(context, &temp_command_buffer);
    vulkan_buffer_destroy(context, &staging_buffer);
    SDL_DestroySurface(image_surface);

    return true;
}

void vulkan_image_destroy(VulkanContext* context, VulkanImage* image) {
    vkDestroyImageView(context->device.logical_device, image->view, context->allocator);
    vkFreeMemory(context->device.logical_device, image->memory, context->allocator);
    vkDestroyImage(context->device.logical_device, image->handle, context->allocator);
}

void vulkan_image_view_create(VulkanContext* context, VkImage image, VkFormat format, VkImageView* out_image_view) {
    VkImageViewCreateInfo view_create_info {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };
    VK_CHECK(vkCreateImageView(
        context->device.logical_device, &view_create_info, context->allocator, out_image_view));
}

void vulkan_image_view_destroy(VulkanContext* context, VkImageView view) {
    vkDestroyImageView(context->device.logical_device, view, context->allocator);
}

void vulkan_image_transition_layout(VulkanImageTransitionLayoutParams params) {
    VkAccessFlags2 src_access_mask;
    VkAccessFlags2 dst_access_mask;
    VkPipelineStageFlags2 src_stage_mask;
    VkPipelineStageFlags2 dst_stage_mask;

    if (params.old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
        params.new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    ) {
        src_access_mask = 0;
        dst_access_mask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        src_stage_mask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        dst_stage_mask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    } else if (params.old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
        params.new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    ) {
        src_access_mask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        dst_access_mask = VK_ACCESS_2_SHADER_READ_BIT;
        src_stage_mask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        dst_stage_mask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    } else {
        ZEN_ASSERT_MESSAGE(false, "Unsupported layout transition.");
        return;
    }

    vulkan_image_transition_layout_ext({
        .command_buffer = params.command_buffer,
        .image = params.image,
        .old_layout = params.old_layout,
        .new_layout = params.new_layout,
        .src_access_mask = src_access_mask,
        .dst_access_mask = dst_access_mask,
        .src_stage_mask = src_stage_mask,
        .dst_stage_mask = dst_stage_mask
    });
}

void vulkan_image_transition_layout_ext(VulkanImageTransitionLayoutExtParams params) {
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
