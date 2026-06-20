#include "image.h"

#include "core/logger.h"
#include "renderer/vulkan/utils.h"

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
    VulkanImage* out_image
) {
    out_image->width = width;
    out_image->height = height;

    VkImageCreateInfo image_create_info {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .imageType = image_type,
        .format = format,
        .extent = {
            .width = width,
            .height = height,
            .depth = 1
        },
        .mipLevels = 4,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = tiling,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VK_CHECK(vkCreateImage(
        context->device.logical_device, &image_create_info, context->allocator, &out_image->handle));

    // Query memory requirements
    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(context->device.logical_device, out_image->handle, &memory_requirements);

    uint32_t memory_type_index = vulkan_find_memory_index(
        context, memory_requirements.memoryTypeBits, memory_property_flags);
    if (memory_type_index == VULKAN_MEMORY_TYPE_INDEX_NOT_FOUND) {
        log_error("vulkan_image_create failed - Required memory type not found.");
        return false;
    }

    // Allocate memory
    VkMemoryAllocateInfo memory_allocate_info {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memory_requirements.size,
        .memoryTypeIndex = memory_type_index
    };
    VK_CHECK(vkAllocateMemory(
        context->device.logical_device, &memory_allocate_info, context->allocator, &out_image->memory));

    // Bind the memory
    VK_CHECK(vkBindImageMemory(context->device.logical_device, out_image->handle, out_image->memory, 0));

    // Create view
    if (create_view) {
        out_image->view = nullptr;
        vulkan_image_view_create(context, format, out_image, view_aspect_flags);
    }

    return true;
}

void vulkan_image_view_create(
    VulkanContext* context,
    VkFormat format,
    VulkanImage* image,
    VkImageAspectFlags aspect_flags
) {
    VkImageViewCreateInfo view_create_info {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = image->handle,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange = {
            .aspectMask = aspect_flags,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    VK_CHECK(vkCreateImageView(
        context->device.logical_device, &view_create_info, context->allocator, &image->view));
}

void vulkan_image_destroy(VulkanContext* context, VulkanImage* image) {
    if (image->view) {
        vkDestroyImageView(context->device.logical_device, image->view, context->allocator);
        image->view = nullptr;
    }
    if (image->memory) {
        vkFreeMemory(context->device.logical_device, image->memory, context->allocator);
        image->memory = nullptr;
    }
    if (image->handle) {
        vkDestroyImage(context->device.logical_device, image->handle, context->allocator);
        image->handle = nullptr;
    }
}
