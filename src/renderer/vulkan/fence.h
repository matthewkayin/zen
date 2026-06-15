#pragma once

#include "renderer/vulkan/types.h"

void vulkan_fence_create(VulkanContext* context, bool create_signaled, VulkanFence* out_fence);
void vulkan_fence_destroy(VulkanContext* context, VulkanFence* fence);
bool vulkan_fence_wait(VulkanContext* context, VulkanFence* fence, uint64_t timeout_ns);
void vulkan_fence_reset(VulkanContext* context, VulkanFence* fence);
