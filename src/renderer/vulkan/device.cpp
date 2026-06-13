#include "device.h"

#include "core/logger.h"
#include "vulkan/vulkan_core.h"
#include <vector>

static const uint32_t VULKAN_PHYSICAL_DEVICE_QUEUE_FAMILY_NOT_SUPPORTED = UINT32_MAX;

static const uint32_t VULKAN_PHYSICAL_DEVICE_FLAG_GRAPHICS = 1U;
static const uint32_t VULKAN_PHYSICAL_DEVICE_FLAG_PRESENT = 1U << 1U;
static const uint32_t VULKAN_PHYSICAL_DEVICE_FLAG_COMPUTE = 1U << 2U;
static const uint32_t VULKAN_PHYSICAL_DEVICE_FLAG_TRANSFER = 1U << 3U;
static const uint32_t VULKAN_PHYSICAL_DEVICE_FLAG_SAMPLER_ANISOTROPY = 1U << 4U;
static const uint32_t VULKAN_PHYSICAL_DEVICE_FLAG_DISCRETE_GPU = 1U << 5U;

struct VulkanPhysicalDeviceRequirements {
    uint32_t flags;
    std::vector<const char*> device_extension_names;
};

struct VulkanPhysicalDeviceQueueFamilyInfo {
    uint32_t graphics_family_index;
    uint32_t present_family_index;
    uint32_t compute_family_index;
    uint32_t transfer_family_index;
};

bool vulkan_select_physical_device(VulkanContext* context);
bool vulkan_physical_device_meets_requirements(
    VkPhysicalDevice device,
    VkSurfaceKHR surface,
    const VkPhysicalDeviceProperties* properties,
    const VkPhysicalDeviceFeatures* features,
    const VulkanPhysicalDeviceRequirements* requirements,
    VulkanPhysicalDeviceQueueFamilyInfo* out_queue_family_info,
    VulkanSwapchainSupportInfo* out_swapchain_support_info);
const char* vulkan_device_get_type_str(VkPhysicalDeviceType device_type);

bool vulkan_device_create(VulkanContext* context) {
    if (!vulkan_select_physical_device(context)) {
        return false;
    }

    log_info("Creating logical device...");

    std::vector<uint32_t> indices;
    indices.push_back(context->device.graphics_queue_index);
    if (context->device.present_queue_index != context->device.graphics_queue_index) {
        indices.push_back(context->device.present_queue_index);
    }
    if (context->device.transfer_queue_index != context->device.graphics_queue_index) {
        indices.push_back(context->device.transfer_queue_index);
    }

    std::vector<VkDeviceQueueCreateInfo> queue_create_infos(indices.size());
    float queue_priority = 1.0f;
    for (uint32_t index = 0; index < indices.size(); index++) {
        queue_create_infos[index].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_infos[index].queueFamilyIndex = indices[index];
        queue_create_infos[index].queueCount = 1;
        queue_create_infos[index].flags = 0;
        queue_create_infos[index].pNext = nullptr;
        queue_create_infos[index].pQueuePriorities = &queue_priority;
    }

    // Request device features
    VkPhysicalDeviceFeatures device_features = {};
    device_features.samplerAnisotropy = VK_TRUE; // Request anisotropy

    VkDeviceCreateInfo device_create_info{};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount = (uint32_t)indices.size();
    device_create_info.pQueueCreateInfos = queue_create_infos.data();
    device_create_info.pEnabledFeatures = &device_features;
    device_create_info.enabledExtensionCount = 1;

    const char* extension_names = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    device_create_info.ppEnabledExtensionNames = &extension_names;

    // Deprecated and ignored
    device_create_info.enabledLayerCount = 0;
    device_create_info.ppEnabledLayerNames = nullptr;

    // Create the device
    VK_CHECK(vkCreateDevice(
        context->device.physical_device,
        &device_create_info,
        context->allocator,
        &context->device.logical_device));

    log_info("Logical device created.");

    // Get queue handles
    vkGetDeviceQueue(
        context->device.logical_device,
        context->device.graphics_queue_index,
        0,
        &context->device.graphics_queue);
    vkGetDeviceQueue(
        context->device.logical_device,
        context->device.present_queue_index,
        0,
        &context->device.present_queue);
    vkGetDeviceQueue(
        context->device.logical_device,
        context->device.transfer_queue_index,
        0,
        &context->device.transfer_queue);
    log_info("Queues obtained.");

    return true;
}

void vulkan_device_destroy(VulkanContext* context) {
    // Unset queues
    context->device.graphics_queue = nullptr;
    context->device.present_queue = nullptr;
    context->device.transfer_queue = nullptr;

    log_info("Destroying logical device...");
    if (context->device.logical_device) {
        vkDestroyDevice(context->device.logical_device, context->allocator);
        context->device.logical_device = nullptr;
    }

    log_info("Releasing physical device resources...");
    context->device.physical_device = nullptr;

    if (context->device.swapchain_support_info.formats) {
        free(context->device.swapchain_support_info.formats);
        context->device.swapchain_support_info.formats = nullptr;
        context->device.swapchain_support_info.format_count = 0;
    }

    if (context->device.swapchain_support_info.present_modes) {
        free(context->device.swapchain_support_info.present_modes);
        context->device.swapchain_support_info.present_modes = nullptr;
        context->device.swapchain_support_info.present_mode_count = 0;
    }

    memset(&context->device.swapchain_support_info.capabilities, 0, sizeof(context->device.swapchain_support_info.capabilities));

    context->device.graphics_queue_index = VULKAN_PHYSICAL_DEVICE_QUEUE_FAMILY_NOT_SUPPORTED;
    context->device.present_queue_index = VULKAN_PHYSICAL_DEVICE_QUEUE_FAMILY_NOT_SUPPORTED;
    context->device.transfer_queue_index = VULKAN_PHYSICAL_DEVICE_QUEUE_FAMILY_NOT_SUPPORTED;
}

void vulkan_device_query_swapchain_support(
        VkPhysicalDevice physical_device,
        VkSurfaceKHR surface,
        VulkanSwapchainSupportInfo* out_swapchain_support_info) {

    // Surface capabilities
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &out_swapchain_support_info->capabilities));

    // Surface formats
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
        physical_device,
        surface,
        &out_swapchain_support_info->format_count,
        nullptr));
    if (out_swapchain_support_info->format_count != 0) {
        if (!out_swapchain_support_info->formats) {
            out_swapchain_support_info->formats = (VkSurfaceFormatKHR*)malloc(out_swapchain_support_info->format_count * sizeof(VkSurfaceFormatKHR));
        }
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
            physical_device,
            surface,
            &out_swapchain_support_info->format_count,
            out_swapchain_support_info->formats));
    }

    // Present modes
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
        physical_device,
        surface,
        &out_swapchain_support_info->present_mode_count,
        nullptr));
    if (out_swapchain_support_info->present_mode_count != 0) {
        if (!out_swapchain_support_info->present_modes) {
            out_swapchain_support_info->present_modes = (VkPresentModeKHR*)malloc(out_swapchain_support_info->present_mode_count * sizeof(VkPresentModeKHR));
        }
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
            physical_device,
            surface,
            &out_swapchain_support_info->present_mode_count,
            out_swapchain_support_info->present_modes));
    }
}

bool vulkan_device_detect_depth_format(VulkanDevice* device) {
    // Format candidates
    const uint32_t candidate_count = 3;
    VkFormat candidates[candidate_count] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };

    uint32_t flags = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
    for (uint32_t candidate_index = 0; candidate_index < candidate_count; candidate_index++) {
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(device->physical_device, candidates[candidate_index], &properties);

        const bool device_supports_depth_format =
            ((properties.linearTilingFeatures & flags) == flags) ||
            ((properties.optimalTilingFeatures & flags) == flags);
        if (device_supports_depth_format) {
            device->depth_format = candidates[candidate_index];
            return true;
        }
    }

    return false;
}

bool vulkan_select_physical_device(VulkanContext* context) {
    uint32_t physical_device_count = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(context->instance, &physical_device_count, 0));
    if (physical_device_count == 0) {
        log_error("No physical devices which support Vulkan were found.");
        return false;
    }

    std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
    VK_CHECK(vkEnumeratePhysicalDevices(context->instance, &physical_device_count, physical_devices.data()));

    for (uint32_t index = 0; index < physical_device_count; index++) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(physical_devices[index], &properties);

        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceFeatures(physical_devices[index], &features);

        VkPhysicalDeviceMemoryProperties memory_properties;
        vkGetPhysicalDeviceMemoryProperties(physical_devices[index], &memory_properties);

        VulkanPhysicalDeviceRequirements requirements{};
        requirements.flags =
            VULKAN_PHYSICAL_DEVICE_FLAG_GRAPHICS |
            VULKAN_PHYSICAL_DEVICE_FLAG_PRESENT |
            VULKAN_PHYSICAL_DEVICE_FLAG_TRANSFER |
            VULKAN_PHYSICAL_DEVICE_FLAG_SAMPLER_ANISOTROPY |
            VULKAN_PHYSICAL_DEVICE_FLAG_DISCRETE_GPU;
        requirements.device_extension_names.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

        VulkanPhysicalDeviceQueueFamilyInfo queue_info{};
        bool device_meets_requirements = vulkan_physical_device_meets_requirements(
            physical_devices[index],
            context->surface,
            &properties,
            &features,
            &requirements,
            &queue_info,
            &context->device.swapchain_support_info);
        if (device_meets_requirements) {
            log_info("Selected device. Name: %s Type: %s", properties.deviceName, vulkan_device_get_type_str(properties.deviceType));
            log_info("GPU driver version: %d.%d.%d",
                VK_VERSION_MAJOR(properties.driverVersion),
                VK_VERSION_MINOR(properties.driverVersion),
                VK_VERSION_PATCH(properties.driverVersion));
            log_info("Vulkan API version: %d.%d.%d",
                VK_VERSION_MAJOR(properties.apiVersion),
                VK_VERSION_MINOR(properties.apiVersion),
                VK_VERSION_PATCH(properties.apiVersion));

            // Device memory info
            for (uint32_t heap_index = 0; heap_index < memory_properties.memoryHeapCount; heap_index++) {
                float memory_size_gib =
                    (((float)memory_properties.memoryHeaps[heap_index].size) / 1024.0f / 1024.0f / 1024.0f);
                const char* memory_type_str =
                    memory_properties.memoryHeaps[heap_index].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT
                    ? "Local GPU memory"
                    : "Shared system memory";
                log_info("%s: %.2f GiB", memory_type_str, memory_size_gib);
            }

            context->device.physical_device = physical_devices[index];
            context->device.graphics_queue_index = queue_info.graphics_family_index;
            context->device.present_queue_index = queue_info.present_family_index;
            context->device.transfer_queue_index = queue_info.transfer_family_index;
            // Note: set compute here if needed

            // Keep a copy of properties, features, and memory properties for later
            context->device.properties = properties;
            context->device.features = features;
            context->device.memory_properties = memory_properties;

            break;
        }
    }

    if (!context->device.physical_device) {
        log_error("No physical devices were found which meet the requiremenets.");
        return false;
    }

    return true;
}

bool vulkan_physical_device_meets_requirements(
        VkPhysicalDevice device,
        VkSurfaceKHR surface,
        const VkPhysicalDeviceProperties* properties,
        const VkPhysicalDeviceFeatures* features,
        const VulkanPhysicalDeviceRequirements* requirements,
        VulkanPhysicalDeviceQueueFamilyInfo* out_queue_family_info,
        VulkanSwapchainSupportInfo* out_swapchain_support_info) {

    uint32_t device_capabilities_flags = 0;

    // Query queue family info
    out_queue_family_info->graphics_family_index = VULKAN_PHYSICAL_DEVICE_QUEUE_FAMILY_NOT_SUPPORTED;
    out_queue_family_info->present_family_index = VULKAN_PHYSICAL_DEVICE_QUEUE_FAMILY_NOT_SUPPORTED;
    out_queue_family_info->compute_family_index = VULKAN_PHYSICAL_DEVICE_QUEUE_FAMILY_NOT_SUPPORTED;
    out_queue_family_info->transfer_family_index = VULKAN_PHYSICAL_DEVICE_QUEUE_FAMILY_NOT_SUPPORTED;

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, 0);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

    log_info("Graphics | Present | Compute | Transfer | Name");
    uint8_t min_transfer_score = 255;
    for (uint32_t index = 0; index < queue_family_count; index++) {
        uint8_t current_transfer_score = 0;

        // Graphics queue?
        if (queue_families[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            out_queue_family_info->graphics_family_index = index;
            device_capabilities_flags |= VULKAN_PHYSICAL_DEVICE_FLAG_GRAPHICS;
            current_transfer_score++;
        }

        // Compute queue?
        if (queue_families[index].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            out_queue_family_info->compute_family_index = index;
            device_capabilities_flags |= VULKAN_PHYSICAL_DEVICE_FLAG_COMPUTE;
            current_transfer_score++;
        }

        // Transfer queue?
        if (queue_families[index].queueFlags & VK_QUEUE_TRANSFER_BIT) {
            // Take the index if it is currently the lowest
            // This increases the likelihood that it is a dedicated transfer queue
            if (current_transfer_score <= min_transfer_score) {
                min_transfer_score = current_transfer_score;
                out_queue_family_info->transfer_family_index = index;
                device_capabilities_flags |= VULKAN_PHYSICAL_DEVICE_FLAG_TRANSFER;
            }
        }

        // Present queue?
        VkBool32 supports_present = VK_FALSE;
        VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(device, index, surface, &supports_present));
        if (supports_present) {
            out_queue_family_info->present_family_index = index;
        }
    }

    log_info("       %d |       %d |       %d |        %d | %s",
        out_queue_family_info->graphics_family_index != VULKAN_PHYSICAL_DEVICE_QUEUE_FAMILY_NOT_SUPPORTED,
        out_queue_family_info->present_family_index != VULKAN_PHYSICAL_DEVICE_QUEUE_FAMILY_NOT_SUPPORTED,
        out_queue_family_info->compute_family_index != VULKAN_PHYSICAL_DEVICE_QUEUE_FAMILY_NOT_SUPPORTED,
        out_queue_family_info->transfer_family_index != VULKAN_PHYSICAL_DEVICE_QUEUE_FAMILY_NOT_SUPPORTED,
        properties->deviceName);

    // Discrete GPU?
    if (properties->deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        device_capabilities_flags |= VULKAN_PHYSICAL_DEVICE_FLAG_DISCRETE_GPU;
    }

    // Sampler anisotropy?
    if (features->samplerAnisotropy) {
        device_capabilities_flags |= VULKAN_PHYSICAL_DEVICE_FLAG_SAMPLER_ANISOTROPY;
    }

    // Query swapchain support
    vulkan_device_query_swapchain_support(device, surface, out_swapchain_support_info);
    if (out_swapchain_support_info->format_count < 1 || out_swapchain_support_info->present_mode_count < 1) {
        if (out_swapchain_support_info->formats) {
            free(out_swapchain_support_info->formats);
            out_swapchain_support_info->formats = nullptr;
        }
        if (out_swapchain_support_info->present_modes) {
            free(out_swapchain_support_info->present_modes);
            out_swapchain_support_info->present_modes = nullptr;
        }

        log_info("Required swapchain support not present. Skipping device.");
        return false;
    }

    // Device extensions
    if (!requirements->device_extension_names.empty()) {
        uint32_t available_extension_count = 0;
        VK_CHECK(vkEnumerateDeviceExtensionProperties(
            device,
            nullptr,
            &available_extension_count,
            nullptr));

        if (available_extension_count != 0) {
            std::vector<VkExtensionProperties> available_extensions;
            available_extensions = std::vector<VkExtensionProperties>(available_extension_count);
            VK_CHECK(vkEnumerateDeviceExtensionProperties(
                device,
                nullptr,
                &available_extension_count,
                available_extensions.data()
            ));

            for (uint32_t index = 0; index < (uint32_t)requirements->device_extension_names.size(); index++) {
                uint32_t available_extension_index;
                for (available_extension_index = 0; available_extension_index < available_extension_count; available_extension_index++) {
                    if (strcmp(available_extensions[available_extension_index].extensionName,
                            requirements->device_extension_names[index]) == 0) {
                        break;
                    }
                }

                if (available_extension_index == available_extension_count) {
                    log_info("Required extension %s not found on device %s.", requirements->device_extension_names[index]);
                    return false;
                }
            }
        }
    }

    // Compare capabilities with requirements
    const bool device_meets_requirements = device_capabilities_flags & requirements->flags;
    log_info("Device meets requirements ? %d", device_meets_requirements);
    return device_meets_requirements;
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
