#pragma once

#include "renderer/vulkan/types.h"

void vulkan_swapchain_create(
    VulkanContext* context,
    uint32_t width,
    uint32_t height,
    VulkanSwapchain* out_swapchain);
void vulkan_swapchain_recreate(
    VulkanContext* context,
    uint32_t width,
    uint32_t height,
    VulkanSwapchain* swapchain);
void vulkan_swapchain_destroy(VulkanContext* context, VulkanSwapchain* swapchain);

bool vulkan_swapchain_acquire_next_image_index(
    VulkanContext* context,
    VulkanSwapchain* swapchain,
    uint64_t timeout_ns,
    VkSemaphore image_available_semaphore,
    VkFence fence,
    uint32_t* out_image_index);
void vulkan_swapchain_present(
    VulkanContext* context,
    VulkanSwapchain* swapchain,
    VkQueue graphics_queue,
    VkQueue present_queue,
    VkSemaphore render_complete_semaphore,
    uint32_t present_image_index);
