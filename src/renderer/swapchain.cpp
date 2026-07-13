#include "swapchain.h"

#include <algorithm>

static const uint32_t VULKAN_SURFACE_SIZE_DETERMINED_BY_SWAPCHAIN_EXTENT = UINT32_MAX;
static const uint32_t VULKAN_SURFACE_MAX_IMAGE_COUNT_LIMITLESS = 0U;

// Internal
VkSurfaceFormatKHR vulkan_swapchain_choose_image_format(VulkanContext* context);
VkPresentModeKHR vulkan_swapchain_choose_present_mode(VulkanContext* context);

void vulkan_swapchain_create(VulkanContext* context) {
    // Query swapchain support info
    VkSurfaceCapabilitiesKHR surface_capabilities;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        context->device.physical_device, context->surface, &surface_capabilities));

    // Determine swapchain extent
    if (surface_capabilities.currentExtent.width == VULKAN_SURFACE_SIZE_DETERMINED_BY_SWAPCHAIN_EXTENT) {
        context->swapchain.extent = {
            .width = context->window_width,
            .height = context->window_height
        };
    } else {
        context->swapchain.extent = surface_capabilities.currentExtent;
    }

    // Clamp swapchain extent
    context->swapchain.extent.width = std::clamp(
        context->swapchain.extent.width,
        surface_capabilities.minImageExtent.width,
        surface_capabilities.maxImageExtent.width);
    context->swapchain.extent.height = std::clamp(
        context->swapchain.extent.height,
        surface_capabilities.minImageExtent.height,
        surface_capabilities.maxImageExtent.height);

    // Determine swapchain image count
    uint32_t target_image_count = surface_capabilities.minImageCount + 1;
    if (surface_capabilities.maxImageCount != VULKAN_SURFACE_MAX_IMAGE_COUNT_LIMITLESS &&
        target_image_count > surface_capabilities.maxImageCount
    ) {
        target_image_count = surface_capabilities.maxImageCount;
    }

    // Choose image format
    context->swapchain.image_format = vulkan_swapchain_choose_image_format(context);

    // Determine queue family indices
    uint32_t queue_family_indices[] = {
        context->device.graphics_queue_index,
        context->device.present_queue_index
    };
    VkSharingMode image_sharing_mode;
    uint32_t queue_family_index_count;
    uint32_t* p_queue_family_indices;
    if (context->device.graphics_queue_index != context->device.present_queue_index) {
        image_sharing_mode = VK_SHARING_MODE_CONCURRENT;
        queue_family_index_count = 2;
        p_queue_family_indices = queue_family_indices;
    } else {
        image_sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
        queue_family_index_count = 0;
        p_queue_family_indices = nullptr;
    }

    VkSwapchainCreateInfoKHR swapchain_create_info {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = context->surface,
        .minImageCount = target_image_count,
        .imageFormat = context->swapchain.image_format.format,
        .imageColorSpace = context->swapchain.image_format.colorSpace,
        .imageExtent = context->swapchain.extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = image_sharing_mode,
        .queueFamilyIndexCount = queue_family_index_count,
        .pQueueFamilyIndices = p_queue_family_indices,
        .preTransform = surface_capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = vulkan_swapchain_choose_present_mode(context),
        .clipped = VK_TRUE,
        .oldSwapchain = nullptr
    };
    VK_CHECK(vkCreateSwapchainKHR(
        context->device.logical_device, &swapchain_create_info, context->allocator, &context->swapchain.handle));

    // Get images from the swapchain
    uint32_t image_count;
    VK_CHECK(vkGetSwapchainImagesKHR(
        context->device.logical_device, context->swapchain.handle, &image_count, nullptr));
    context->swapchain.images = std::vector<VkImage>(image_count);
    VK_CHECK(vkGetSwapchainImagesKHR(
        context->device.logical_device, context->swapchain.handle, &image_count, context->swapchain.images.data()));

    // Create image views
    context->swapchain.image_views = std::vector<VkImageView>(image_count);
    for (uint32_t image_index = 0; image_index < image_count; image_index++) {
        VkImageViewCreateInfo view_create_info {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = context->swapchain.images[image_index],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = context->swapchain.image_format.format,
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
            &context->swapchain.image_views[image_index]));
    }
}

void vulkan_swapchain_destroy(VulkanContext* context) {
    vkDeviceWaitIdle(context->device.logical_device);

    // Destroy views
    for (VkImageView view : context->swapchain.image_views) {
        vkDestroyImageView(context->device.logical_device, view, context->allocator);
    }
    context->swapchain.images.clear();
    context->swapchain.image_views.clear();

    vkDestroySwapchainKHR(context->device.logical_device, context->swapchain.handle, context->allocator);
}

// INTERNAL

VkSurfaceFormatKHR vulkan_swapchain_choose_image_format(VulkanContext* context) {
    // Get surface format count
    uint32_t surface_format_count;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
        context->device.physical_device, context->surface, &surface_format_count, nullptr));
    ZEN_ASSERT(surface_format_count != 0);

    // Get surface formats
    std::vector<VkSurfaceFormatKHR> surface_formats(surface_format_count);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
        context->device.physical_device, context->surface, &surface_format_count, surface_formats.data()));

    // Search for a preferred format and choose it if it's found
    for (const VkSurfaceFormatKHR surface_format : surface_formats) {
        if (surface_format.format == VK_FORMAT_B8G8R8A8_UNORM &&
            surface_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
        ) {
            return surface_format;
        }
    }

    // Default to the first format if preferred was not found
    return surface_formats[0];
}

VkPresentModeKHR vulkan_swapchain_choose_present_mode(VulkanContext* context) {
    // Get present mode count
    uint32_t present_mode_count;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
        context->device.physical_device, context->surface, &present_mode_count, nullptr));
    ZEN_ASSERT(present_mode_count != 0);

    // Get present modes
    std::vector<VkPresentModeKHR> present_modes(present_mode_count);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
        context->device.physical_device, context->surface, &present_mode_count, present_modes.data()));

    // Search for MAILBOX and choose it if it's found
    for (const VkPresentModeKHR present_mode : present_modes) {
        if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return present_mode;
        }
    }

    // Default to FIFO if MAILBOX was not found
    return VK_PRESENT_MODE_FIFO_KHR;
}
