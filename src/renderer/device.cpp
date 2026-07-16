#include "device.h"

#include "core/logger.h"

static const uint32_t VULKAN_QUEUE_FAMILY_NOT_SUPPORTED = UINT32_MAX;
static const uint32_t VULKAN_DEVICE_DOES_NOT_MEET_REQUIREMENTS = UINT32_MAX;

static const char* VULKAN_DEVICE_REQUIRED_EXTENSION_NAMES[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

void vulkan_device_get_queue_indices(
    VkPhysicalDevice device,
    VkSurfaceKHR surface,
    uint32_t* out_graphics_index,
    uint32_t* out_present_index);
uint32_t vulkan_device_score_physical_device(VkPhysicalDevice device, VkSurfaceKHR surface);

bool vulkan_device_create(VulkanContext* context) {
    log_debug("Selecting physical device...");

    // Get physical device count
    uint32_t physical_device_count = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(context->instance, &physical_device_count, nullptr));
    if (physical_device_count == 0) {
        log_error("No physical devices which support vulkan were found.");
        return false;
    }

    // Get physical devices
    std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
    VK_CHECK(vkEnumeratePhysicalDevices(context->instance, &physical_device_count, physical_devices.data()));

    // Choose the device with the best score
    uint32_t selected_device_score = VULKAN_DEVICE_DOES_NOT_MEET_REQUIREMENTS;
    for (uint32_t index = 0; index < physical_device_count; index++) {
        uint32_t device_score = vulkan_device_score_physical_device(physical_devices[index], context->surface);

        if (device_score > selected_device_score ||
            selected_device_score == VULKAN_DEVICE_DOES_NOT_MEET_REQUIREMENTS
        ) {
            context->device.physical_device = physical_devices[index];
            selected_device_score = device_score;
        }
    }
    if (selected_device_score == VULKAN_DEVICE_DOES_NOT_MEET_REQUIREMENTS) {
        log_error("No physical devices which meet renderer requirements were found.");
        return false;
    }

    log_debug("Creating logical device...");

    // Get queue indices
    vulkan_device_get_queue_indices(
        context->device.physical_device,
        context->surface,
        &context->device.graphics_queue_index,
        &context->device.present_queue_index);

    std::vector<uint32_t> queue_indices;
    queue_indices.push_back(context->device.graphics_queue_index);
    if (context->device.present_queue_index != context->device.graphics_queue_index) {
        queue_indices.push_back(context->device.present_queue_index);
    }

    // Queue create info
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos(queue_indices.size());
    float queue_priority = 1.0f;
    for (uint32_t index = 0; index < (uint32_t)queue_indices.size(); index++) {
        queue_create_infos[index] = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = queue_indices[index],
            .queueCount = 1,
            .pQueuePriorities = &queue_priority
        };
    }

    // Device features to request
    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT device_extended_dynamic_state_features {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
        .pNext = nullptr,
        .extendedDynamicState = VK_TRUE
    };
    VkPhysicalDeviceVulkan13Features device_vulkan13_features {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &device_extended_dynamic_state_features,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE
    };
    VkPhysicalDeviceVulkan11Features device_vulkan11_features {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .pNext = &device_vulkan13_features,
        .shaderDrawParameters = VK_TRUE
    };
    VkPhysicalDeviceFeatures2 device_features {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &device_vulkan11_features,
        .features = {
            .samplerAnisotropy = VK_TRUE
        }
    };

    // Create the device
    VkDeviceCreateInfo device_create_info {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &device_features,
        .queueCreateInfoCount = (uint32_t)queue_create_infos.size(),
        .pQueueCreateInfos = queue_create_infos.data(),
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = ARRAY_LENGTH(VULKAN_DEVICE_REQUIRED_EXTENSION_NAMES),
        .ppEnabledExtensionNames = VULKAN_DEVICE_REQUIRED_EXTENSION_NAMES
    };
    VK_CHECK(vkCreateDevice(context->device.physical_device, &device_create_info, context->allocator, &context->device.logical_device));
    log_debug("Logical device created.");

    // Get queue handles
    vkGetDeviceQueue(context->device.logical_device, context->device.graphics_queue_index, 0, &context->device.graphics_queue);
    vkGetDeviceQueue(context->device.logical_device, context->device.present_queue_index, 0, &context->device.present_queue);

    // Create the graphics command pool
    VkCommandPoolCreateInfo graphics_command_pool_create_info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = context->device.graphics_queue_index
    };
    VK_CHECK(vkCreateCommandPool(
        context->device.logical_device,
        &graphics_command_pool_create_info,
        context->allocator,
        &context->device.graphics_command_pool));
    log_debug("Graphics command pool created.");

    return true;
}

void vulkan_device_destroy(VulkanContext* context) {
    log_debug("Destroying graphics command pool...");
    vkDestroyCommandPool(context->device.logical_device, context->device.graphics_command_pool, context->allocator);

    log_debug("Destroying logical device...");
    vkDestroyDevice(context->device.logical_device, context->allocator);
}

void vulkan_device_get_queue_indices(
    VkPhysicalDevice device,
    VkSurfaceKHR surface,
    uint32_t* out_graphics_index,
    uint32_t* out_present_index
) {
    // Default values
    *out_graphics_index = VULKAN_QUEUE_FAMILY_NOT_SUPPORTED;
    *out_present_index = VULKAN_QUEUE_FAMILY_NOT_SUPPORTED;

    // Query queue family properties
    uint32_t queue_family_count;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

    // Iterate through them to find queue indices
    for (uint32_t queue_index = 0; queue_index < (uint32_t)queue_families.size(); queue_index++) {
        if (queue_families[queue_index].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            *out_graphics_index = queue_index;
        }

        VkBool32 queue_supports_present;
        VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(device, queue_index, surface, &queue_supports_present));
        if (queue_supports_present) {
            *out_present_index = queue_index;
        }
    }
}

uint32_t vulkan_device_score_physical_device(VkPhysicalDevice device, VkSurfaceKHR surface) {
    // Get device properties
    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(device, &device_properties);

    log_debug("Checking suitability of device %s...", device_properties.deviceName);

    // Check supported API version
    if (device_properties.apiVersion < VK_API_VERSION_1_4) {
        log_debug("Device does not support the required API version.");
        // return VULKAN_DEVICE_DOES_NOT_MEET_REQUIREMENTS;
    }

    // Check queue support
    uint32_t graphics_index, present_index;
    vulkan_device_get_queue_indices(device, surface, &graphics_index, &present_index);
    if (graphics_index == VULKAN_QUEUE_FAMILY_NOT_SUPPORTED) {
        log_debug("Device does not have a graphics queue.");
        return VULKAN_DEVICE_DOES_NOT_MEET_REQUIREMENTS;
    }
    if (present_index == VULKAN_QUEUE_FAMILY_NOT_SUPPORTED) {
        log_debug("Device does not have a present queue.");
        return VULKAN_DEVICE_DOES_NOT_MEET_REQUIREMENTS;
    }

    // Get device features
    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT device_extended_dynamic_state_features {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
        .pNext = nullptr
    };
    VkPhysicalDeviceVulkan13Features device_vulkan13_features {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &device_extended_dynamic_state_features
    };
    VkPhysicalDeviceVulkan11Features device_vulkan11_features {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .pNext = &device_vulkan13_features
    };
    VkPhysicalDeviceFeatures2 device_features {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &device_vulkan11_features
    };
    vkGetPhysicalDeviceFeatures2(device, &device_features);

    // Check device features
    if (!device_features.features.samplerAnisotropy) {
        log_debug("Device does not support sampler anisotropy.");
        return VULKAN_DEVICE_DOES_NOT_MEET_REQUIREMENTS;
    }
    if (!device_vulkan11_features.shaderDrawParameters) {
        log_debug("Device does not support shader draw parameters.");
        return VULKAN_DEVICE_DOES_NOT_MEET_REQUIREMENTS;
    }
    if (!device_vulkan13_features.synchronization2) {
        log_debug("Device does not support synchronization2.");
        return VULKAN_DEVICE_DOES_NOT_MEET_REQUIREMENTS;
    }
    if (!device_vulkan13_features.dynamicRendering) {
        log_debug("Device does not support dynamic rendering.");
        return VULKAN_DEVICE_DOES_NOT_MEET_REQUIREMENTS;
    }
    if (!device_extended_dynamic_state_features.extendedDynamicState) {
        log_debug("Device does not support extended dynamic state.");
        return VULKAN_DEVICE_DOES_NOT_MEET_REQUIREMENTS;
    }

    // Get device extensions
    uint32_t device_extension_count;
    VK_CHECK(vkEnumerateDeviceExtensionProperties(device, nullptr, &device_extension_count, nullptr));
    std::vector<VkExtensionProperties> device_extensions(device_extension_count);
    VK_CHECK(
        vkEnumerateDeviceExtensionProperties(device, nullptr, &device_extension_count, device_extensions.data()));

    // Check device extensions against required extensions
    for (uint32_t required_index = 0; required_index < ARRAY_LENGTH(VULKAN_DEVICE_REQUIRED_EXTENSION_NAMES); required_index++) {
        const char* required_extension_name = VULKAN_DEVICE_REQUIRED_EXTENSION_NAMES[required_index];

        bool device_has_extension = false;
        for (uint32_t extension_index = 0; extension_index < device_extension_count; extension_index++) {
            if (strcmp(device_extensions[extension_index].extensionName, required_extension_name) == 0) {
                device_has_extension = true;
                break;
            }
        }

        if (!device_has_extension) {
            log_debug("Device is missing required extension %s.", required_extension_name);
            return VULKAN_DEVICE_DOES_NOT_MEET_REQUIREMENTS;
        }
    }

    uint32_t device_score = 0;

    // Check discrete GPU
    if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        log_debug("Device is a discrete GPU.");
        device_score++;
    }

    log_debug("Device is suitable. Score: %u.", device_score);
    return device_score;
}
