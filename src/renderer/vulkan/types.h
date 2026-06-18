#pragma once

#include "core/asserts.h"
#include <vulkan/vulkan.h>

#define VK_CHECK(expr)                                                         \
    {                                                                          \
        ZEN_ASSERT(expr == VK_SUCCESS);                                        \
    }

struct VulkanContext {
    VkInstance instance;
    VkAllocationCallbacks* allocator;
    VkSurfaceKHR surface;

    // Debug
    VkDebugUtilsMessengerEXT debug_messenger;
};
