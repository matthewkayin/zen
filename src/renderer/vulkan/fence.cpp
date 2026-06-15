#include "fence.h"

#include "core/logger.h"
#include "vulkan/vulkan_core.h"

void vulkan_fence_create(VulkanContext* context, bool create_signaled, VulkanFence* out_fence) {
    out_fence->is_signaled = create_signaled;

    VkFenceCreateInfo fence_create_info = {};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (out_fence->is_signaled) {
        fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    }

    VK_CHECK(vkCreateFence(
        context->device.logical_device,
        &fence_create_info,
        context->allocator,
        &out_fence->handle));
}

void vulkan_fence_destroy(VulkanContext* context, VulkanFence* fence) {
    if (fence->handle) {
        vkDestroyFence(context->device.logical_device, fence->handle, context->allocator);
        fence->handle = nullptr;
    }
    fence->is_signaled = false;
}

bool vulkan_fence_wait(VulkanContext* context, VulkanFence* fence, uint64_t timeout_ns) {
    if (fence->is_signaled) {
        return true;
    }

    VkResult result = vkWaitForFences(context->device.logical_device, 1, &fence->handle, VK_TRUE, timeout_ns);
    switch (result) {
        case VK_SUCCESS: {
            fence->is_signaled = true;
            return true;
        }
        case VK_TIMEOUT: {
            log_warn("vulkan_fence_wait - timed out.");
            break;
        }
        case VK_ERROR_DEVICE_LOST: {
            log_error("vulkan_fence_wait - device lost.");
            break;
        }
        case VK_ERROR_OUT_OF_HOST_MEMORY: {
            log_error("vulkan_fence_wait - out of host memory.");
            break;
        }
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: {
            log_error("vulkan_fence_wait - out of device memory.");
            break;
        }
        default: {
            log_error("vulkan_fence_wait - an unknown error has occurred.");
            break;
        }
    }

    return false;
}

void vulkan_fence_reset(VulkanContext* context, VulkanFence* fence) {
    if (fence->is_signaled) {
        VK_CHECK(vkResetFences(context->device.logical_device, 1, &fence->handle));
        fence->is_signaled = false;
    }
}
