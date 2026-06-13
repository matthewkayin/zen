#pragma once

#include "renderer/vulkan/types.h"

bool vulkan_device_create(VulkanContext* context);
void vulkan_device_destroy(VulkanContext* context);
void vulkan_device_query_swapchain_support(
    VkPhysicalDevice physical_device,
    VkSurfaceKHR surface,
    VulkanSwapchainSupportInfo* out_swapchain_support_info);
