#include "device.h"

#include "core/logger.h"
#include <cstring>

static const uint32_t VULKAN_PHYSICAL_DEVICE_QUEUE_FAMILY_NOT_SUPPORTED = UINT32_MAX;

static const uint32_t VULKAN_PHYSICAL_DEVICE_FLAG_GRAPHICS = 1U;
static const uint32_t VULKAN_PHYSICAL_DEVICE_FLAG_PRESENT = 1U << 1U;
static const uint32_t VULKAN_PHYSICAL_DEVICE_FLAG_COMPUTE = 1U << 2U;
static const uint32_t VULKAN_PHYSICAL_DEVICE_FLAG_TRANSFER = 1U << 3U;
static const uint32_t VULKAN_PHYSICAL_DEVICE_FLAG_SAMPLER_ANISOTROPY = 1U << 4U;
static const uint32_t VULKAN_PHYSICAL_DEVICE_FLAG_DISCRETE_GPU = 1U << 5U;

static const char* VULKAN_DEVICE_REQUIRED_EXTENSION_NAMES[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

bool vulkan_device_select_physical_device(VulkanContext* context);

void vulkan_device_init_from_physical_device(
    VkPhysicalDevice physical_device,
    VkSurfaceKHR surface,
    VulkanDevice* out_device);

bool vulkan_device_meets_requirements(
    const VulkanDevice* device,
    uint32_t required_capabilities,
    uint32_t required_device_extension_name_count,
    const char* const* required_device_extension_names);

const char* vulkan_device_get_type_str(VkPhysicalDeviceType device_type);

bool vulkan_device_create(VulkanContext* context) {
    if (!vulkan_device_select_physical_device(context)) {
        return false;
    }

    log_info("Creating logical device...");

    // Get a list of indices to create queues for
    std::vector<uint32_t> queue_indices;
    queue_indices.push_back(context->device.graphics_queue_index);
    if (context->device.present_queue_index != context->device.graphics_queue_index) {
        queue_indices.push_back(context->device.present_queue_index);
    }
    if (context->device.transfer_queue_index != context->device.graphics_queue_index) {
        queue_indices.push_back(context->device.transfer_queue_index);
    }

    // Queue create infos
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos(queue_indices.size());
    float queue_priority = 1.0f;
    for (uint32_t index = 0; index < queue_indices.size(); index++) {
        queue_create_infos[index] = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = queue_indices[index],
            .queueCount = 1,
            .pQueuePriorities = &queue_priority
        };
    }

    // Request device features
    VkPhysicalDeviceFeatures device_features{};
    device_features.samplerAnisotropy = VK_TRUE;

    // Device create info
    VkDeviceCreateInfo device_create_info {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queueCreateInfoCount = (uint32_t)queue_create_infos.size(),
        .pQueueCreateInfos = queue_create_infos.data(),
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = ARRAY_LENGTH(VULKAN_DEVICE_REQUIRED_EXTENSION_NAMES),
        .ppEnabledExtensionNames = VULKAN_DEVICE_REQUIRED_EXTENSION_NAMES,
        .pEnabledFeatures = &device_features
    };

    // Create the device
    VK_CHECK(vkCreateDevice(
        context->device.physical_device, &device_create_info, context->allocator,
        &context->device.logical_device));
    log_info("Logical device created.");

    // Get queue handles
    vkGetDeviceQueue(
        context->device.logical_device, context->device.graphics_queue_index,
        0, &context->device.graphics_queue);
    vkGetDeviceQueue(
        context->device.logical_device, context->device.present_queue_index,
        0, &context->device.present_queue);
    vkGetDeviceQueue(
        context->device.logical_device, context->device.transfer_queue_index,
        0, &context->device.transfer_queue);
    log_info("Queues obtained.")

    // Create info for graphics command pool
    VkCommandPoolCreateInfo pool_create_info{};
    pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_create_info.queueFamilyIndex = context->device.graphics_queue_index;
    pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    // Create graphics command pool
    VK_CHECK(vkCreateCommandPool(
        context->device.logical_device, &pool_create_info, context->allocator,
        &context->device.graphics_command_pool));
    log_info("Graphics command pool created.");

    return true;
}

void vulkan_device_destroy(VulkanContext* context) {
    // Unset queues
    context->device.graphics_queue = nullptr;
    context->device.present_queue = nullptr;
    context->device.transfer_queue = nullptr;

    log_info("Destroying command pools...");
    vkDestroyCommandPool(
        context->device.logical_device, context->device.graphics_command_pool, context->allocator);

    log_info("Destroying logical device...");
    if (context->device.logical_device) {
        vkDestroyDevice(context->device.logical_device, context->allocator);
        context->device.logical_device = nullptr;
    }

    context->device.physical_device = nullptr;

    context->device.swapchain_support_info.formats.clear();
    context->device.swapchain_support_info.present_modes.clear();
    memset(&context->device.swapchain_support_info.capabilities, 0,
        sizeof(context->device.swapchain_support_info));

    context->device.graphics_queue_index = VULKAN_PHYSICAL_DEVICE_QUEUE_FAMILY_NOT_SUPPORTED;
    context->device.present_queue_index = VULKAN_PHYSICAL_DEVICE_QUEUE_FAMILY_NOT_SUPPORTED;
    context->device.transfer_queue_index = VULKAN_PHYSICAL_DEVICE_QUEUE_FAMILY_NOT_SUPPORTED;
}

void vulkan_device_query_swapchain_support(
    VkPhysicalDevice physical_device,
    VkSurfaceKHR surface,
    VulkanSwapchainSupportInfo* out_swapchain_support_info
) {
    // Surface capabilities
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        physical_device, surface, &out_swapchain_support_info->capabilities));

    // Surface formats
    uint32_t format_count;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
        physical_device, surface, &format_count, nullptr));
    if (format_count != 0) {
        out_swapchain_support_info->formats = std::vector<VkSurfaceFormatKHR>(format_count);
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
            physical_device, surface, &format_count, out_swapchain_support_info->formats.data()));
    }

    // Present modes
    uint32_t present_mode_count;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
        physical_device, surface, &present_mode_count, nullptr));
    if (present_mode_count != 0) {
        out_swapchain_support_info->present_modes = std::vector<VkPresentModeKHR>(present_mode_count);
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
            physical_device, surface, &present_mode_count, out_swapchain_support_info->present_modes.data()));
    }
}

bool vulkan_device_detect_depth_format(VulkanDevice* device) {
    // Format candidates
    VkFormat format_candidates[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };

    uint32_t flags = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
    for (uint32_t candidate_index = 0; candidate_index < ARRAY_LENGTH(format_candidates); candidate_index++) {
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(
            device->physical_device, format_candidates[candidate_index], &properties);

        const bool device_supports_depth_format =
            ((properties.linearTilingFeatures & flags) == flags) ||
            ((properties.optimalTilingFeatures & flags) == flags);
        if (device_supports_depth_format) {
            device->depth_format = format_candidates[candidate_index];
            return true;
        }
    }

    return false;
}

bool vulkan_device_select_physical_device(VulkanContext* context) {
    // Get the physical device count
    uint32_t physical_device_count = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(context->instance, &physical_device_count, nullptr));
    if (physical_device_count == 0) {
        log_error("No physical devices which support Vulkan were found.");
        return false;
    }

    // Get the physical devices
    std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
    VK_CHECK(vkEnumeratePhysicalDevices(context->instance, &physical_device_count, physical_devices.data()));

    // Init each candidate device
    std::vector<VulkanDevice> devices(physical_device_count);
    for (uint32_t device_index = 0; device_index < physical_device_count; device_index++) {
        vulkan_device_init_from_physical_device(physical_devices[device_index], context->surface, &devices[device_index]);
    }

    // Determine the requirements for a physical device
    uint32_t required_device_capabilities =
        VULKAN_PHYSICAL_DEVICE_FLAG_GRAPHICS |
        VULKAN_PHYSICAL_DEVICE_FLAG_PRESENT |
        VULKAN_PHYSICAL_DEVICE_FLAG_TRANSFER |
        VULKAN_PHYSICAL_DEVICE_FLAG_SAMPLER_ANISOTROPY |
        VULKAN_PHYSICAL_DEVICE_FLAG_DISCRETE_GPU;
    std::vector<const char*> required_device_extension_names;
    required_device_extension_names.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    // Select a device which matches the requirements
    const uint32_t DEVICE_NOT_SELECTED = UINT32_MAX;
    uint32_t selected_device_index = DEVICE_NOT_SELECTED;
    for (uint32_t device_index = 0; device_index < physical_device_count; device_index++) {
        if (vulkan_device_meets_requirements(&devices[device_index], required_device_capabilities,
            ARRAY_LENGTH(VULKAN_DEVICE_REQUIRED_EXTENSION_NAMES), VULKAN_DEVICE_REQUIRED_EXTENSION_NAMES)
        ) {
            selected_device_index = device_index;
            break;
        }
    }

    // If no device was found, loosen the requirements and try again
    if (selected_device_index == DEVICE_NOT_SELECTED) {
        required_device_capabilities &= ~VULKAN_PHYSICAL_DEVICE_FLAG_DISCRETE_GPU;

        for (uint32_t device_index = 0; device_index < physical_device_count; device_index++) {
            if (vulkan_device_meets_requirements(&devices[device_index], required_device_capabilities,
                ARRAY_LENGTH(VULKAN_DEVICE_REQUIRED_EXTENSION_NAMES), VULKAN_DEVICE_REQUIRED_EXTENSION_NAMES)
            ) {
                selected_device_index = device_index;
                break;
            }
        }
    }

    // If still not device was found, then return false
    if (selected_device_index == DEVICE_NOT_SELECTED) {
        log_error("No physical devices were found which meet the requirements.");
        return false;
    }

    context->device = devices[selected_device_index];

    // Print device info
    log_info("Selected device %s | Type %s",
        context->device.properties.deviceName,
        vulkan_device_get_type_str(context->device.properties.deviceType));
    log_info("GPU driver version: %d.%d.%d",
        VK_VERSION_MAJOR(context->device.properties.driverVersion),
        VK_VERSION_MINOR(context->device.properties.driverVersion),
        VK_VERSION_PATCH(context->device.properties.driverVersion));
    log_info("Vulkan API version: %d.%d.%d",
        VK_API_VERSION_MAJOR(context->device.properties.apiVersion),
        VK_API_VERSION_MINOR(context->device.properties.apiVersion),
        VK_API_VERSION_PATCH(context->device.properties.apiVersion));

    // Device memory info
    for (uint32_t heap_index = 0;
        heap_index < context->device.memory_properties.memoryHeapCount;
        heap_index++
    ) {
        VkMemoryHeap heap = context->device.memory_properties.memoryHeaps[heap_index];
        float memory_size_gib = (((float)heap.size) / 1024.0f / 1024.0f / 1024.0f);
        const char* memory_type_str = heap.flags &
            VK_MEMORY_HEAP_DEVICE_LOCAL_BIT
                ? "Local GPU memory"
                : "Shared system memory";
        log_info("%s: %.2f GiB", memory_type_str, memory_size_gib);
    }

    return true;
}

void vulkan_device_init_from_physical_device(
    VkPhysicalDevice physical_device,
    VkSurfaceKHR surface,
    VulkanDevice* out_device
) {
    out_device->physical_device = physical_device;

    // Get the device properties
    vkGetPhysicalDeviceProperties(physical_device, &out_device->properties);
    vkGetPhysicalDeviceFeatures(physical_device, &out_device->features);
    vkGetPhysicalDeviceMemoryProperties(physical_device, &out_device->memory_properties);

    // Set queue families to not supported by default
    out_device->graphics_queue_index = VULKAN_PHYSICAL_DEVICE_QUEUE_FAMILY_NOT_SUPPORTED;
    out_device->present_queue_index = VULKAN_PHYSICAL_DEVICE_QUEUE_FAMILY_NOT_SUPPORTED;
    out_device->transfer_queue_index = VULKAN_PHYSICAL_DEVICE_QUEUE_FAMILY_NOT_SUPPORTED;
    out_device->compute_queue_index = VULKAN_PHYSICAL_DEVICE_QUEUE_FAMILY_NOT_SUPPORTED;

    // Get queue families
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(out_device->physical_device, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(out_device->physical_device, &queue_family_count, queue_families.data());

    log_info("Graphics | Present | Compute | Transfer | Name");
    uint32_t min_transfer_score = UINT32_MAX;
    for (uint32_t family_index = 0; family_index < queue_family_count; family_index++) {
        uint32_t family_transfer_score = 0;

        // Graphics queue?
        if (queue_families[family_index].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            out_device->graphics_queue_index = family_index;
            family_transfer_score++;
        }

        // Compute queue?
        if (queue_families[family_index].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            out_device->compute_queue_index = family_index;
            family_transfer_score++;
        }

        // Transfer queue?
        if (queue_families[family_index].queueFlags & VK_QUEUE_TRANSFER_BIT) {
            // Take the current family if it is currently the lowest
            // This increases the likelihood that it is a dedicated transfer queue
            if (family_transfer_score <= min_transfer_score) {
                min_transfer_score = family_index;
                out_device->transfer_queue_index = family_index;
            }
        }

        // Present queue?
        VkBool32 supports_present;
        VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(
            out_device->physical_device, family_index, surface, &supports_present));
        if (supports_present) {
            out_device->present_queue_index = family_index;
        }
    }

    vulkan_device_query_swapchain_support(physical_device, surface, &out_device->swapchain_support_info);

    log_info("       %d |       %d |       %d |        %d | %s",
        (bool)(out_device->graphics_queue_index != VULKAN_PHYSICAL_DEVICE_QUEUE_FAMILY_NOT_SUPPORTED),
        (bool)(out_device->present_queue_index != VULKAN_PHYSICAL_DEVICE_QUEUE_FAMILY_NOT_SUPPORTED),
        (bool)(out_device->compute_queue_index != VULKAN_PHYSICAL_DEVICE_QUEUE_FAMILY_NOT_SUPPORTED),
        (bool)(out_device->transfer_queue_index != VULKAN_PHYSICAL_DEVICE_QUEUE_FAMILY_NOT_SUPPORTED),
        out_device->properties.deviceName);
}

bool vulkan_device_meets_requirements(
    const VulkanDevice* device,
    uint32_t required_capabilities,
    uint32_t required_device_extension_name_count,
    const char* const* required_device_extension_names
) {
    // Begin with no capabilities
    uint32_t capabilities = 0;

    // Mark capabilities flags
    if (device->graphics_queue_index != VULKAN_PHYSICAL_DEVICE_QUEUE_FAMILY_NOT_SUPPORTED) {
        capabilities |= VULKAN_PHYSICAL_DEVICE_FLAG_GRAPHICS;
    }
    if (device->present_queue_index != VULKAN_PHYSICAL_DEVICE_QUEUE_FAMILY_NOT_SUPPORTED) {
        capabilities |= VULKAN_PHYSICAL_DEVICE_FLAG_PRESENT;
    }
    if (device->compute_queue_index != VULKAN_PHYSICAL_DEVICE_QUEUE_FAMILY_NOT_SUPPORTED) {
        capabilities |= VULKAN_PHYSICAL_DEVICE_FLAG_COMPUTE;
    }
    if (device->transfer_queue_index != VULKAN_PHYSICAL_DEVICE_QUEUE_FAMILY_NOT_SUPPORTED) {
        capabilities |= VULKAN_PHYSICAL_DEVICE_FLAG_TRANSFER;
    }
    if (device->properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        capabilities |= VULKAN_PHYSICAL_DEVICE_FLAG_DISCRETE_GPU;
    }
    if (device->features.samplerAnisotropy) {
        capabilities |= VULKAN_PHYSICAL_DEVICE_FLAG_SAMPLER_ANISOTROPY;
    }

    if ((capabilities & required_capabilities) != required_capabilities) {
        log_info("Device %s does not meet required capabilities.", device->properties.deviceName);
        return false;
    }

    // Check swapchain support
    if (device->swapchain_support_info.formats.empty() || device->swapchain_support_info.present_modes.empty()) {
        log_info("Device %s lacks required swapchain support.", device->properties.deviceName);
        return false;
    }

    // Get device extensions
    uint32_t device_extension_count;
    VK_CHECK(vkEnumerateDeviceExtensionProperties(
        device->physical_device, nullptr, &device_extension_count, nullptr));

    std::vector<VkExtensionProperties> device_extensions(device_extension_count);
    VK_CHECK(vkEnumerateDeviceExtensionProperties(
        device->physical_device, nullptr, &device_extension_count, device_extensions.data()));

    // For each required extension, check if it exists in the device extensions
    for (uint32_t required_extension_index = 0;
        required_extension_index < required_device_extension_name_count;
        required_extension_index++
    ) {
        const char* required_extension_name = required_device_extension_names[required_extension_index];
        bool device_has_extension = false;
        for (uint32_t device_extension_index = 0;
            device_extension_index < device_extension_count;
            device_extension_index++
        ) {
            if (strcmp(device_extensions[device_extension_index].extensionName, required_extension_name) == 0) {
                device_has_extension = true;
                break;
            }
        }

        if (!device_has_extension) {
            log_info("Device %s is missing required extension %s", device->properties.deviceName, required_extension_name);
            return false;
        }
    }

    return true;
}

const char* vulkan_device_get_type_str(VkPhysicalDeviceType device_type) {
    switch (device_type) {
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            return "Integrated GPU";
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            return "Discrete GPU";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            return "Virtual GPU";
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            return "CPU";
        case VK_PHYSICAL_DEVICE_TYPE_OTHER:
        default:
            return "Unknown";
    }
}
