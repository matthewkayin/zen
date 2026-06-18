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

bool vulkan_device_select_physical_device(VkInstance instance, VulkanDevice* device);
void vulkan_device_init_from_physical_device(VulkanDevice* device);
bool vulkan_physical_device_meets_requirements(VkPhysicalDevice device,
    VkSurfaceKHR surface,
    const VkPhysicalDeviceProperties* properties,
    const VkPhysicalDeviceFeatures* features,
    const VulkanPhysicalDeviceRequirements* requirements,
    VulkanPhysicalDeviceQueueFamilyInfo* out_queue_family_info,
    VulkanSwapchainSupportInfo* out_swapchain_support_info);

bool vulkan_device_create(VkInstance instance, VulkanDevice* out_device) {}

void vulkan_device_destroy(VkInstance instance, VulkanDevice* device);
void vulkan_device_query_swapchain_support(VkPhysicalDevice physical_device,
    VkSurfaceKHR surface,
    VulkanSwapchainSupportInfo* out_swapchain_support_info);
void vulkan_device_detect_depth_format(VulkanDevice* device);

bool vulkan_device_select_physical_device(VkInstance instance, VulkanDevice* device) {
    // Determine the requirements for a physical device
    VulkanPhysicalDeviceRequirements requirements{};
    requirements.flags = VULKAN_PHYSICAL_DEVICE_FLAG_GRAPHICS | VULKAN_PHYSICAL_DEVICE_FLAG_PRESENT |
                         VULKAN_PHYSICAL_DEVICE_FLAG_TRANSFER | VULKAN_PHYSICAL_DEVICE_FLAG_SAMPLER_ANISOTROPY |
                         VULKAN_PHYSICAL_DEVICE_FLAG_DISCRETE_GPU;
    requirements.device_extension_names.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    // Get the physical device count
    uint32_t physical_device_count = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &physical_device_count, nullptr));
    if (physical_device_count == 0) {
        log_error("No physical devices which support Vulkan were found.");
        return false;
    }

    // Get the physical devices
    std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &physical_device_count, physical_devices.data()));

    // Consider each device to be selected
    for (uint32_t device_index = 0; device_index < physical_device_count; device_index++) {
        VkPhysicalDeviceProperties properties;
        VkPhysicalDeviceFeatures features;
        VkPhysicalDeviceMemoryProperties memory_properties;

        // Get the device properties
        vkGetPhysicalDeviceProperties(physical_devices[device_index], &properties);
        vkGetPhysicalDeviceFeatures(physical_devices[device_index], &features);
        vkGetPhysicalDeviceMemoryProperties(physical_devices[device_index], &memory_properties);

        //
    }
}

void vulkan_device_set_physical_device(VulkanDevice* device, VkPhysicalDevice physical_device) {
    device->physical_device = physical_device;

    // Get the device properties
    vkGetPhysicalDeviceProperties(physical_device, &device->properties);
    vkGetPhysicalDeviceFeatures(physical_device, &device->features);
    vkGetPhysicalDeviceMemoryProperties(physical_device, &device->memory_properties);
}

void vulkan_device_get_capabilities(const VulkanDevice* device,
    VkSurfaceKHR surface,
    uint32_t* out_capabilities,
    VulkanPhysicalDeviceQueueFamilyInfo* out_queue_family_info) {
    // Begin with no capabilities
    uint32_t capabilities = 0;
    VulkanPhysicalDeviceQueueFamilyInfo queue_family_info = {
        .graphics_family_index = VULKAN_PHYSICAL_DEVICE_QUEUE_FAMILY_NOT_SUPPORTED,
        .present_family_index = VULKAN_PHYSICAL_DEVICE_QUEUE_FAMILY_NOT_SUPPORTED,
        .compute_family_index = VULKAN_PHYSICAL_DEVICE_QUEUE_FAMILY_NOT_SUPPORTED,
        .transfer_family_index = VULKAN_PHYSICAL_DEVICE_QUEUE_FAMILY_NOT_SUPPORTED,
    };

    // Get queue families
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device->physical_device, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device->physical_device, &queue_family_count, queue_families.data());

    log_info("Graphics | Present | Compute | Transfer | Name");
    uint32_t min_transfer_score = UINT32_MAX;
    for (uint32_t family_index = 0; family_index < queue_family_count; family_index++) {
        uint32_t family_transfer_score = 0;

        // Graphics queue?
        if (queue_families[family_index].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            queue_family_info.graphics_family_index = family_index;
            family_transfer_score++;
        }

        // Compute queue?
        if (queue_families[family_index].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            queue_family_info.compute_family_index = family_index;
            family_transfer_score++;
        }

        // Transfer queue?
        if (queue_families[family_index].queueFlags & VK_QUEUE_TRANSFER_BIT) {
            // Take the current family if it is currently the lowest
            // This increases the likelihood that it is a dedicated transfer queue
            if (family_transfer_score <= min_transfer_score) {
                min_transfer_score = family_index;
                queue_family_info.transfer_family_index = family_index;
            }
        }

        // Present queue?
        VkBool32 supports_present;
        VK_CHECK(
            vkGetPhysicalDeviceSurfaceSupportKHR(device->physical_device, family_index, surface, &supports_present));
        if (supports_present) {
            queue_family_info.present_family_index = family_index;
        }
    }

    // Mark capabilities flags
    if (queue_family_info.graphics_family_index != VULKAN_PHYSICAL_DEVICE_QUEUE_FAMILY_NOT_SUPPORTED) {
        capabilities |= VULKAN_PHYSICAL_DEVICE_FLAG_GRAPHICS;
    }
    if (queue_family_info.present_family_index != VULKAN_PHYSICAL_DEVICE_FLAG_PRESENT) {
        capabilities |= VULKAN_PHYSICAL_DEVICE_FLAG_PRESENT;
    }
    if (queue_family_info.compute_family_index != VULKAN_PHYSICAL_DEVICE_QUEUE_FAMILY_NOT_SUPPORTED) {
        capabilities |= VULKAN_PHYSICAL_DEVICE_FLAG_COMPUTE;
    }
    if (queue_family_info.transfer_family_index != VULKAN_PHYSICAL_DEVICE_QUEUE_FAMILY_NOT_SUPPORTED) {
        capabilities |= VULKAN_PHYSICAL_DEVICE_FLAG_TRANSFER;
    }
    if (device->properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        capabilities |= VULKAN_PHYSICAL_DEVICE_FLAG_DISCRETE_GPU;
    }
    if (device->features.samplerAnisotropy) {
        capabilities |= VULKAN_PHYSICAL_DEVICE_FLAG_SAMPLER_ANISOTROPY;
    }

    log_info("       %d |       %d |       %d |        %d | %s",
        (bool)(capabilities & VULKAN_PHYSICAL_DEVICE_FLAG_GRAPHICS),
        (bool)(capabilities & VULKAN_PHYSICAL_DEVICE_FLAG_PRESENT),
        (bool)(capabilities & VULKAN_PHYSICAL_DEVICE_FLAG_COMPUTE),
        (bool)(capabilities & VULKAN_PHYSICAL_DEVICE_FLAG_TRANSFER),
        device->properties.deviceName);

    if (out_capabilities != nullptr) {
        *out_capabilities = capabilities;
    }
    if (out_queue_family_info != nullptr) {
        *out_queue_family_info = queue_family_info;
    }
}

bool vulkan_device_has_extensions(const VulkanDevice* device,
    const std::vector<const char*>& required_device_extension_names) {
    // Get device extensions
    uint32_t device_extension_count;
    VK_CHECK(vkEnumerateDeviceExtensionProperties(device->physical_device, nullptr, &device_extension_count, nullptr));
    std::vector<VkExtensionProperties> device_extensions(device_extension_count);
    VK_CHECK(vkEnumerateDeviceExtensionProperties(
        device->physical_device, nullptr, &device_extension_count, device_extensions.data()));

    for (uint32_t required_extension_index = 0;
        required_extension_index < (uint32_t)required_device_extension_names.size();
        required_extension_index++) {
        const char* required_extension_name = required_device_extension_names[required_extension_index];
        bool device_has_extension = false;
        for (uint32_t device_extension_index = 0; device_extension_index < device_extension_count;
            device_extension_index++) {
            if (strcmp(device_extensions[device_extension_index].extensionName, required_extension_name) == 0) {
                device_has_extension = true;
                break;
            }
        }

        if (!device_has_extension) {
            log_info("Required extension %s not found on device %s");
            return false;
        }
    }

    return true;
}
