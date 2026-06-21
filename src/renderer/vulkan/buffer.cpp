#include "buffer.h"

#include "core/logger.h"
#include "renderer/vulkan/utils.h"
#include "renderer/vulkan/command_buffer.h"

bool vulkan_buffer_create(VulkanContext* context, VulkanBufferCreateParams* params, VulkanBuffer* out_buffer) {
    memset(out_buffer, 0, sizeof(VulkanBuffer));

    out_buffer->total_size = params->size;
    out_buffer->usage = params->usage;
    out_buffer->memory_property_flags = params->memory_property_flags;

    VkBufferCreateInfo create_info {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = params->size,
        .usage = params->usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VK_CHECK(vkCreateBuffer(
        context->device.logical_device,
        &create_info,
        context->allocator,
        &out_buffer->handle));

    // Gather memory requirements
    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(context->device.logical_device, out_buffer->handle, &memory_requirements);

    out_buffer->memory_index = vulkan_find_memory_index(
        context, memory_requirements.memoryTypeBits, out_buffer->memory_property_flags);
    if (out_buffer->memory_index == VULKAN_MEMORY_TYPE_INDEX_NOT_FOUND) {
        log_error("vulkan_buffer_create - Failed because the required memory type index was not found.");
        return false;
    }

    // Allocate memory info
    VkMemoryAllocateInfo allocate_info {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memory_requirements.size,
        .memoryTypeIndex = out_buffer->memory_index
    };

    // Allocate the memory
    VkResult result = vkAllocateMemory(
        context->device.logical_device, &allocate_info, context->allocator, &out_buffer->memory);
    if (result != VK_SUCCESS) {
        log_error("vulkan_buffer_create - Failed because memory allocation failed with error %s.", vulkan_result_str(result));
        return false;
    }

    if (params->bind_on_create) {
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
    VulkanContext* context, VkCommandPool pool, VkQueue queue,
    VulkanBuffer* buffer, uint64_t new_size
) {
    VulkanBufferCreateParams new_buffer_create_params {
        .size = new_size,
        .usage = buffer->usage,
        .memory_property_flags = buffer->memory_property_flags,
        .bind_on_create = true
    };
    VulkanBuffer new_buffer;
    if (!vulkan_buffer_create(context, &new_buffer_create_params, &new_buffer)) {
        log_error("vulkan_buffer_resize - Failed to create new buffer.");
        return false;
    }

    vulkan_buffer_copy(context, pool, queue, new_buffer.handle, 0, buffer->handle, 0, buffer->total_size);
    vkDeviceWaitIdle(context->device.logical_device);
    vulkan_buffer_destroy(context, buffer);

    *buffer = new_buffer;

    return true;
}

void vulkan_buffer_bind(VulkanContext* context, VulkanBuffer* buffer, uint64_t offset) {
    VK_CHECK(vkBindBufferMemory(context->device.logical_device, buffer->handle, buffer->memory, offset));
}

void* vulkan_buffer_lock_memory(
    VulkanContext* context, VulkanBuffer* buffer,
    uint64_t offset, uint64_t size, uint32_t flags
) {
    void* buffer_data;
    VK_CHECK(vkMapMemory(context->device.logical_device, buffer->memory, offset, size, flags, &buffer_data));
    return buffer_data;
}

void vulkan_buffer_unlock_memory(VulkanContext* context, VulkanBuffer* buffer) {
    vkUnmapMemory(context->device.logical_device, buffer->memory);
}

void vulkan_buffer_load_data(
    VulkanContext* context, VulkanBuffer* buffer,
    uint64_t offset, uint64_t size, uint32_t flags, const void* data
) {
    void* buffer_data;
    VK_CHECK(vkMapMemory(context->device.logical_device, buffer->memory, offset, size, flags, &buffer_data));
    memcpy(buffer_data, data, size);
    vkUnmapMemory(context->device.logical_device, buffer->memory);
}

void vulkan_buffer_copy(
    VulkanContext* context, VkCommandPool pool, VkQueue queue,
    VkBuffer dst, uint64_t dst_offset, VkBuffer src, uint64_t src_offset, uint64_t size
) {
    vkQueueWaitIdle(queue);

    VulkanCommandBuffer temp_command_buffer;
    vulkan_command_buffer_single_use_alloc_and_begin(context, pool, &temp_command_buffer);

    // Send copy command to the buffer
    VkBufferCopy copy_region {
        .srcOffset = src_offset,
        .dstOffset = dst_offset,
        .size = size
    };
    vkCmdCopyBuffer(temp_command_buffer.handle, src, dst, 1, &copy_region);

    // Submit the buffer for execution and await it
    vulkan_command_buffer_single_use_end(&temp_command_buffer, queue);
}

void vulkan_buffer_upload_data(
    VulkanContext* context, VkCommandPool pool, VkQueue queue,
    VulkanBuffer* buffer, uint64_t offset, uint64_t size, void* data
) {
    // Create a host-visible staging buffer to upload to and mark it as the source of transfer
    VulkanBufferCreateParams staging_buffer_create_params {
        .size = size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .memory_property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        .bind_on_create = true
    };
    VulkanBuffer staging_buffer;
    ZEN_ASSERT(vulkan_buffer_create(context, &staging_buffer_create_params, &staging_buffer));

    // Load the data into the staging buffer
    vulkan_buffer_load_data(context, &staging_buffer, 0, size, 0, data);

    // Copy from the staging buffer to the device-local buffer
    vulkan_buffer_copy(context, pool, queue, buffer->handle, 0, staging_buffer.handle, offset, size);

    // Clean up the staging buffer
    vulkan_buffer_destroy(context, &staging_buffer);
}
