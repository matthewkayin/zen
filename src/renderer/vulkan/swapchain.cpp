#include "swapchain.h"

#include "core/logger.h"
#include "renderer/vulkan/device.h"
#include "renderer/vulkan/image.h"
#include <algorithm>

/*
https://docs.vulkan.org/refpages/latest/refpages/source/VkSurfaceCapabilitiesKHR.html

currentExtent is the current width and height of the surface,
or the special value (0xFFFFFFFF, 0xFFFFFFFF) indicating that the surface size will be determined
by the extent of a swapchain targeting the surface.
*/
static const uint32_t VULKAN_SURFACE_SIZE_DETERMINED_BY_SWAPCHAIN_EXTENT = UINT32_MAX;
static const uint32_t VULKAN_SWAPCHAIN_MAX_IMAGE_COUNT_LIMITLESS = 0;

bool vulkan_swapchain_create(
    VulkanContext* context, uint32_t width, uint32_t height,
    VulkanSwapchain* out_swapchain
) {
    // Requery swapchain support
    vulkan_device_query_swapchain_support(
        context->device.physical_device, context->surface, &context->device.swapchain_support_info);

    // Choose a swap surface format, using the first format as a default
    out_swapchain->image_format = context->device.swapchain_support_info.formats[0];
    for (uint32_t format_index = 0;
        format_index < (uint32_t)context->device.swapchain_support_info.formats.size();
        format_index++
    ) {
        VkSurfaceFormatKHR format = context->device.swapchain_support_info.formats[format_index];
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
        ) {
            out_swapchain->image_format = format;
            break;
        }
    }

    // Choose a present mode, preferring MAILBOX, using FIFO as a default
    VkPresentModeKHR selected_present_mode = VK_PRESENT_MODE_FIFO_KHR;
    for (uint32_t present_mode_index = 0;
        present_mode_index < (uint32_t)context->device.swapchain_support_info.present_modes.size();
        present_mode_index++
    ) {
        VkPresentModeKHR present_mode = context->device.swapchain_support_info.present_modes[present_mode_index];
        if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            selected_present_mode = present_mode;
            break;
        }
    }

    // Swapchain extent
    VkExtent2D swapchain_extent;
    if (context->device.swapchain_support_info.capabilities.currentExtent.width ==
        VULKAN_SURFACE_SIZE_DETERMINED_BY_SWAPCHAIN_EXTENT
    ) {
        swapchain_extent = {
            .width = width,
            .height = height
        };
    } else {
        swapchain_extent = context->device.swapchain_support_info.capabilities.currentExtent;
    }

    // Clamp swapchain extent
    VkExtent2D min_extent = context->device.swapchain_support_info.capabilities.minImageExtent;
    VkExtent2D max_extent = context->device.swapchain_support_info.capabilities.maxImageExtent;
    swapchain_extent.width = std::clamp(swapchain_extent.width, min_extent.width, max_extent.width);
    swapchain_extent.height = std::clamp(swapchain_extent.height, min_extent.height, max_extent.height);

    // Determine minimum image count
    uint32_t image_count = context->device.swapchain_support_info.capabilities.minImageCount + 1;
    if (context->device.swapchain_support_info.capabilities.maxImageCount !=
        VULKAN_SWAPCHAIN_MAX_IMAGE_COUNT_LIMITLESS &&
        image_count > context->device.swapchain_support_info.capabilities.maxImageCount
    ) {
        image_count = context->device.swapchain_support_info.capabilities.maxImageCount;
    }

    out_swapchain->max_frames_in_flight = image_count - 1;

    // Swapchain create info
    VkSwapchainCreateInfoKHR swapchain_create_info {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = context->surface,
        .minImageCount = image_count,
        .imageFormat = out_swapchain->image_format.format,
        .imageColorSpace = out_swapchain->image_format.colorSpace,
        .imageExtent = swapchain_extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
    };

    // Determine queue family indices
    uint32_t queue_family_indices[] = {
        context->device.graphics_queue_index,
        context->device.present_queue_index
    };
    if (context->device.graphics_queue_index != context->device.present_queue_index) {
        swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchain_create_info.queueFamilyIndexCount = 2;
        swapchain_create_info.pQueueFamilyIndices = queue_family_indices;
    } else {
        swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchain_create_info.queueFamilyIndexCount = 0;
        swapchain_create_info.pQueueFamilyIndices = nullptr;
    }

    swapchain_create_info.preTransform = context->device.swapchain_support_info.capabilities.currentTransform;
    swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_create_info.presentMode = selected_present_mode;
    swapchain_create_info.clipped = VK_TRUE;
    swapchain_create_info.oldSwapchain = nullptr;

    // Create the swapchain
    VK_CHECK(vkCreateSwapchainKHR(
        context->device.logical_device, &swapchain_create_info, context->allocator, &out_swapchain->handle));

    // Get images from the recently-created swapchain
    VK_CHECK(vkGetSwapchainImagesKHR(
        context->device.logical_device, out_swapchain->handle, &out_swapchain->image_count, nullptr));
    out_swapchain->images = (VkImage*)malloc(out_swapchain->image_count * sizeof(VkImage));
    out_swapchain->views = (VkImageView*)malloc(out_swapchain->image_count * sizeof(VkImageView));

    // Get the images from the swapchain
    VK_CHECK(vkGetSwapchainImagesKHR(
        context->device.logical_device, out_swapchain->handle,
        &out_swapchain->image_count, out_swapchain->images));

    // Create views
    for (uint32_t image_index = 0; image_index < out_swapchain->image_count; image_index++) {
        VkImageViewCreateInfo view_create_info {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = out_swapchain->images[image_index],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = out_swapchain->image_format.format,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };

        VK_CHECK(vkCreateImageView(
            context->device.logical_device, &view_create_info, context->allocator,
            &out_swapchain->views[image_index]))
    }

    // Start with the first image
    context->frame_index = 0;

    // Depth resources
    if (!vulkan_device_detect_depth_format(&context->device)) {
        context->device.depth_format = VK_FORMAT_UNDEFINED;
        log_error("No supported depth format found.");
        return false;
    }

    // Create depth image and its view
    if (!vulkan_image_create(context, VK_IMAGE_TYPE_2D, swapchain_extent.width, swapchain_extent.height,
        context->device.depth_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, true, VK_IMAGE_ASPECT_DEPTH_BIT, &out_swapchain->depth_attachment)
    ) {
        log_error("vulkan_swapchain_create - failed to create depth attachment image.");
    }

    log_info("Swapchain created successfully.");
    return true;
}

bool vulkan_swapchain_recreate(
    VulkanContext* context, uint32_t width, uint32_t height,
    VulkanSwapchain* out_swapchain
) {
    vulkan_swapchain_destroy(context, out_swapchain);
    return vulkan_swapchain_create(context, width, height, out_swapchain);
}

void vulkan_swapchain_destroy(VulkanContext* context, VulkanSwapchain* swapchain) {
    vkDeviceWaitIdle(context->device.logical_device);
    vulkan_image_destroy(context, &swapchain->depth_attachment);

    // Destroy views
    for (uint32_t image_index = 0; image_index < swapchain->image_count; image_index++) {
        vkDestroyImageView(context->device.logical_device, swapchain->views[image_index], context->allocator);
    }
    free(swapchain->images);
    free(swapchain->views);

    vkDestroySwapchainKHR(context->device.logical_device, swapchain->handle, context->allocator);
}

bool vulkan_swapchain_acquire_next_image_index(
    VulkanContext* context,
    VulkanSwapchain* swapchain,
    uint64_t timeout_ns,
    VkSemaphore acquire_semaphore,
    VkFence fence,
    uint32_t* out_image_index
) {
    VkResult result = vkAcquireNextImageKHR(
        context->device.logical_device, swapchain->handle, timeout_ns, acquire_semaphore, fence, out_image_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        // Trigger swapchain recreation, then boot ouf of render loop
        vulkan_swapchain_recreate(context, context->framebuffer_width, context->framebuffer_height, swapchain);
        return false;
    }

    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        log_error("Failed to acquire swapchain image.");
        return false;
    }

    return true;
}

void vulkan_swapchain_present(
    VulkanContext* context,
    VulkanSwapchain* swapchain,
    VkQueue present_queue,
    VkSemaphore render_complete_semaphore,
    uint32_t present_image_index
) {
    VkPresentInfoKHR present_info {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &render_complete_semaphore,
        .swapchainCount = 1,
        .pSwapchains = &swapchain->handle,
        .pImageIndices = &present_image_index,
        .pResults = nullptr
    };

    VkResult result = vkQueuePresentKHR(present_queue, &present_info);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        vulkan_swapchain_recreate(context, context->framebuffer_width, context->framebuffer_height, swapchain);
    } else if (result != VK_SUCCESS) {
        log_error("Failed to present swapchain image.");
    }

    context->frame_index++;
    if (context->frame_index == swapchain->max_frames_in_flight) {
        context->frame_index = 0;
    }
}
