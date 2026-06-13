#include "utils.h"

#include "core/logger.h"

uint32_t vulkan_find_memory_index(VulkanContext* context, uint32_t type_filter, uint32_t property_flags) {
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(context->device.physical_device, &memory_properties);

    for (uint32_t index = 0; index < memory_properties.memoryTypeCount; index++) {
        // Check the memory type against the type filter
        uint32_t type_flag = 1U << index;
        if (!(type_filter & type_flag)) {
            continue;
        }

        // Check to see if the memory type bit is set
        if ((memory_properties.memoryTypes[index].propertyFlags & property_flags) == property_flags) {
            return index;
        }
    }

    log_warn("vulkan_find_memory_index unable to find suitable memory type.");
    return VULKAN_MEMORY_TYPE_NOT_FOUND;
}
