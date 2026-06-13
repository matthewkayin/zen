#include "swapchain.h"

#include "core/logger.h"
#include "renderer/vulkan/device.h"
#include "renderer/vulkan/image.h"
#include "vulkan/vulkan_core.h"
#include <algorithm>

void vulkan_swapchain_create_internal(
    VulkanContext* context,
    uint32_t width,
    uint32_t height,
    VulkanSwapchain* swapchain);
void vulkan_swapchain_destroy_internal(VulkanContext* context, VulkanSwapchain* swapchain);

void vulkan_swapchain_create(
        VulkanContext* context,
        uint32_t width,
        uint32_t height,
        VulkanSwapchain* out_swapchain) {
    vulkan_swapchain_create_internal(context, width, height, out_swapchain);
}

void vulkan_swapchain_recreate(
        VulkanContext* context,
        uint32_t width,
        uint32_t height,
        VulkanSwapchain* swapchain) {
    vulkan_swapchain_destroy_internal(context, swapchain);
    vulkan_swapchain_create_internal(context, width, height, swapchain);
}

void vulkan_swapchain_destroy(VulkanContext* context, VulkanSwapchain* swapchain) {
    vulkan_swapchain_destroy_internal(context, swapchain);
}

bool vulkan_swapchain_acquire_next_image_index(
        VulkanContext* context,
        VulkanSwapchain* swapchain,
        uint64_t timeout_ns,
        VkSemaphore image_available_semaphore,
        VkFence fence,
        uint32_t* out_image_index) {
    VkResult result = vkAcquireNextImageKHR(context->device.logical_device, swapchain->handle, timeout_ns, image_available_semaphore, fence, out_image_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        // Trigger swapchain recreation, then boot out of the render loop
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
        VkQueue graphics_queue,
        VkQueue present_queue,
        VkSemaphore render_complete_semaphore,
        uint32_t present_image_index) {

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &render_complete_semaphore;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &swapchain->handle;
    present_info.pImageIndices = &present_image_index;
    present_info.pResults = nullptr;

    VkResult result = vkQueuePresentKHR(present_queue, &present_info);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        vulkan_swapchain_recreate(context, context->framebuffer_width, context->framebuffer_height, swapchain);
    } else if (result != VK_SUCCESS) {
        log_error("Failed to present swapchain image.");
    }
}

void vulkan_swapchain_create_internal(
        VulkanContext* context,
        uint32_t width,
        uint32_t height,
        VulkanSwapchain* swapchain) {
    VkExtent2D swapchain_extent = {
        .width = width,
        .height = height
    };
    swapchain->max_frames_in_flight = 2;

    // Choose a swap surface format
    uint32_t format_index;
    for (format_index = 0; format_index < context->device.swapchain_support_info.format_count; format_index++) {
        VkSurfaceFormatKHR format = context->device.swapchain_support_info.formats[format_index];
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
                format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            swapchain->image_format = format;
            break;
        }
    }
    // Fallback to the first format if the preferred one was not found
    if (format_index == context->device.swapchain_support_info.format_count) {
        format_index = 0;
    }

    // Choose a present mode
    VkPresentModeKHR selected_present_mode = VK_PRESENT_MODE_FIFO_KHR;
    for (uint32_t present_mode_index = 0; present_mode_index < context->device.swapchain_support_info.present_mode_count; present_mode_index++) {
        VkPresentModeKHR present_mode = context->device.swapchain_support_info.present_modes[present_mode_index];
        if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            selected_present_mode = present_mode;
            break;
        }
    }

    // Require swapchain support
    vulkan_device_query_swapchain_support(
        context->device.physical_device,
        context->surface,
        &context->device.swapchain_support_info);

    // Swapchain extent
    if (context->device.swapchain_support_info.capabilities.currentExtent.width != UINT32_MAX) {
        swapchain_extent = context->device.swapchain_support_info.capabilities.currentExtent;
    }

    // Clamp to the value allowed by the GPU
    VkExtent2D min = context->device.swapchain_support_info.capabilities.minImageExtent;
    VkExtent2D max = context->device.swapchain_support_info.capabilities.maxImageExtent;
    swapchain_extent.width = std::clamp(swapchain_extent.width, min.width, max.width);
    swapchain_extent.height = std::clamp(swapchain_extent.height, min.height, max.height);

    // Determine minimum image count
    uint32_t image_count = context->device.swapchain_support_info.capabilities.minImageCount + 1;
    if (context->device.swapchain_support_info.capabilities.maxImageCount > 0 &&
            image_count > context->device.swapchain_support_info.capabilities.maxImageCount) {
        image_count = context->device.swapchain_support_info.capabilities.maxImageCount;
    }

    // Swapchain create info
    VkSwapchainCreateInfoKHR swapchain_create_info{};
    swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create_info.surface = context->surface;
    swapchain_create_info.minImageCount = image_count;
    swapchain_create_info.imageFormat = swapchain->image_format.format;
    swapchain_create_info.imageColorSpace = swapchain->image_format.colorSpace;
    swapchain_create_info.imageExtent = swapchain_extent;
    swapchain_create_info.imageArrayLayers = 1;
    swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // Setup the queue family indices
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

    VK_CHECK(vkCreateSwapchainKHR(
        context->device.logical_device,
        &swapchain_create_info,
        context->allocator,
        &swapchain->handle));

    // Start with 0 frame index
    context->current_frame = 0;

    // Images
    swapchain->image_count = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(context->device.logical_device, swapchain->handle, &swapchain->image_count, nullptr));
    if (!swapchain->images) {
        swapchain->images = (VkImage*)malloc(swapchain->image_count * sizeof(VkImage));
    }
    if (!swapchain->views) {
        swapchain->views = (VkImageView*)malloc(swapchain->image_count * sizeof(VkImageView));
    }
    VK_CHECK(vkGetSwapchainImagesKHR(context->device.logical_device, swapchain->handle, &swapchain->image_count, swapchain->images));

    // Views
    for (uint32_t image_index = 0; image_index < swapchain->image_count; image_index++) {
        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = swapchain->images[image_index];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = swapchain->image_format.format;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(context->device.logical_device, &view_info, context->allocator, &swapchain->views[image_index]));
    }

    // Depth resources
    if (!vulkan_device_detect_depth_format(&context->device)) {
        context->device.depth_format = VK_FORMAT_UNDEFINED;
        log_error("No supported depth format found.");
    }

    // Create depth image and its view
    vulkan_image_create(
        context,
        VK_IMAGE_TYPE_2D,
        swapchain_extent.width,
        swapchain_extent.height,
        context->device.depth_format,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        true,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        &swapchain->depth_attachment);

    log_info("Swapchain created successfully.");
}

void vulkan_swapchain_destroy_internal(VulkanContext* context, VulkanSwapchain* swapchain) {
    vulkan_image_destroy(context, &swapchain->depth_attachment);

    // Only destroy the views, not the images, since those are created by the swapchain
    for (uint32_t image_index = 0; image_index < swapchain->image_count; image_index++) {
        vkDestroyImageView(context->device.logical_device, swapchain->views[image_index], context->allocator);
    }

    vkDestroySwapchainKHR(context->device.logical_device, swapchain->handle, context->allocator);
}
