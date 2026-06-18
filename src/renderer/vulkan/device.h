#pragma once

#include "renderer/vulkan/types.h"

bool vulkan_device_create(VkInstance instance, VulkanDevice* out_device);
void vulkan_device_destroy(VkInstance instance, VulkanDevice* device);
void vulkan_device_query_swapchain_support(
    VkPhysicalDevice physical_device, VkSurfaceKHR surface,
    VulkanSwapchainSupportInfo* out_swapchain_support_info);
void vulkan_device_detect_depth_format(VulkanDevice* device);
