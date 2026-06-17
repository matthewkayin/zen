#include "buffer.h"

#include "core/logger.h"
#include "renderer/vulkan/command_buffer.h"
#include "renderer/vulkan/utils.h"
#include "vulkan/vulkan_core.h"
#include <cstring>

bool vulkan_buffer_create(
        VulkanContext* context,
        uint64_t size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags memory_property_flags,
        bool bind_on_create,
        VulkanBuffer* out_buffer) {

    memset(out_buffer, 0, sizeof(VulkanBuffer));
    out_buffer->total_size = size;
    out_buffer->usage = usage;
    out_buffer->memory_property_flags = memory_property_flags;

    VkBufferCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    create_info.size = size;
    create_info.usage = usage;
    create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateBuffer(
        context->device.logical_device,
        &create_info,
        context->allocator,
        &out_buffer->handle));

    // Gather memory requirements
    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(context->device.logical_device, out_buffer->handle, &requirements);
    out_buffer->memory_index = vulkan_find_memory_index(context, requirements.memoryTypeBits, out_buffer->memory_property_flags);
    if (out_buffer->memory_index == VULKAN_MEMORY_TYPE_NOT_FOUND) {
        log_error("Unable to create Vulkan buffer because the required memory type index was not found.");
        return false;
    }

    // Allocate memory info
    VkMemoryAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.allocationSize = requirements.size;
    allocate_info.memoryTypeIndex = out_buffer->memory_index;

    // Allocate the memory
    VkResult result = vkAllocateMemory(
        context->device.logical_device,
        &allocate_info,
        context->allocator,
        &out_buffer->memory);
    if (result != VK_SUCCESS) {
        log_error("Unable to create Vulkan buffer because the required memory allocation failed. Error: %s", vulkan_result_str(result));
        return false;
    }

    if (bind_on_create) {
        vulkan_buffer_bind(context, out_buffer, 0);
    }

    return true;
}

void vulkan_buffer_destroy(VulkanContext* context, VulkanBuffer* buffer) {
    if (buffer->memory) {
        vkFreeMemory(context->device.logical_device, buffer->memory, context->allocator);
        buffer->memory = nullptr;
    }
    if (buffer->handle) {
        vkDestroyBuffer(context->device.logical_device, buffer->handle, context->allocator);
        buffer->handle = nullptr;
    }
    buffer->total_size = 0;
    buffer->usage = 0;
    buffer->is_locked = false;
}

bool vulkan_buffer_resize(
        VulkanContext* context,
        uint64_t new_size,
        VulkanBuffer* buffer,
        VkQueue queue,
        VkCommandPool pool) {

    // Create a new buffer
    VkBufferCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    create_info.size = new_size;
    create_info.usage = buffer->usage;
    create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer new_buffer;
    VK_CHECK(vkCreateBuffer(context->device.logical_device, &create_info, context->allocator, &new_buffer));

    // Gather memory requirements
    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(context->device.logical_device, new_buffer, &requirements);

    // Allocate memory info
    VkMemoryAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.allocationSize = requirements.size;
    allocate_info.memoryTypeIndex = buffer->memory_index;

    // Allocate new memory
    VkDeviceMemory new_memory;
    VkResult result = vkAllocateMemory(context->device.logical_device, &allocate_info, context->allocator, &new_memory);
    if (result != VK_SUCCESS) {
        log_error("Unable to resize Vulkan buffer because the required memory allocation failed with %s", vulkan_result_str(result));
        return false;
    }

    // Bind the new buffer's memory
    VK_CHECK(vkBindBufferMemory(context->device.logical_device, new_buffer, new_memory, 0));

    // Copy the data
    vulkan_buffer_copy_to(context, pool, queue, buffer->handle, 0, new_buffer, 0, buffer->total_size);

    // Make sure that anything potentially using these is finished
    vkDeviceWaitIdle(context->device.logical_device);

    // Destroy the old buffer
    if (buffer->memory) {
        vkFreeMemory(context->device.logical_device, buffer->memory, context->allocator);
        buffer->memory = nullptr;
    }
    if (buffer->handle) {
        vkDestroyBuffer(context->device.logical_device, buffer->handle, context->allocator);
        buffer->handle = nullptr;
    }

    // Set new properties
    buffer->total_size = new_size;
    buffer->memory = new_memory;
    buffer->handle = new_buffer;

    return true;
}

void vulkan_buffer_bind(VulkanContext* context, VulkanBuffer* buffer, uint64_t offset) {
    VK_CHECK(vkBindBufferMemory(context->device.logical_device, buffer->handle, buffer->memory, offset));
}

void* vulkan_buffer_lock_memory(VulkanContext* context, VulkanBuffer* buffer, uint64_t offset, uint64_t size, uint32_t flags) {
    void* data;
    VK_CHECK(vkMapMemory(context->device.logical_device, buffer->memory, offset, size, flags, &data));
    return data;
}

void vulkan_buffer_unlock_memory(VulkanContext* context, VulkanBuffer* buffer) {
    vkUnmapMemory(context->device.logical_device, buffer->memory);
}

void vulkan_buffer_load_data(
        VulkanContext* context,
        VulkanBuffer* buffer,
        uint64_t offset,
        uint64_t size,
        uint32_t flags,
        const void* data) {

    void* buffer_data_ptr;
    VK_CHECK(vkMapMemory(context->device.logical_device, buffer->memory, offset, size, flags, &buffer_data_ptr));
    memcpy(buffer_data_ptr, data, size);
    vkUnmapMemory(context->device.logical_device, buffer->memory);
}

void vulkan_buffer_copy_to(
        VulkanContext* context,
        VkCommandPool pool,
        VkQueue queue,
        VkBuffer source,
        uint64_t source_offset,
        VkBuffer dest,
        uint64_t dest_offset,
        uint64_t size) {

    // Wait on the queue
    vkQueueWaitIdle(queue);

    VulkanCommandBuffer temp_command_buffer;
    vulkan_command_buffer_allocate_and_begin_single_use(context, pool, &temp_command_buffer);

    // Prepare the copy command and add it to the command buffer
    VkBufferCopy copy_region;
    copy_region.srcOffset = source_offset;
    copy_region.dstOffset = dest_offset;
    copy_region.size = size;

    vkCmdCopyBuffer(temp_command_buffer.handle, source, dest, 1, &copy_region);

    // Submit the buffer for execution and await it
    vulkan_command_buffer_end_single_use(context, pool, &temp_command_buffer, queue);
}
