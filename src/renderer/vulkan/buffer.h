#pragma once

#include "renderer/vulkan/types.h"

bool vulkan_buffer_create(
    VulkanContext* context,
    uint64_t size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags memory_property_flags,
    bool bind_on_create,
    VulkanBuffer* out_buffer);

void vulkan_buffer_destroy(VulkanContext* context, VulkanBuffer* buffer);

bool vulkan_buffer_resize(
    VulkanContext* context,
    uint64_t new_size,
    VulkanBuffer* buffer,
    VkQueue queue,
    VkCommandPool pool);

void vulkan_buffer_bind(VulkanContext* context, VulkanBuffer* buffer, uint64_t offset);

void* vulkan_buffer_lock_memory(VulkanContext* context, VulkanBuffer* buffer, uint64_t offset, uint64_t size, uint32_t flags);
void vulkan_buffer_unlock_memory(VulkanContext* context, VulkanBuffer* buffer);

void vulkan_buffer_load_data(
    VulkanContext* context,
    VulkanBuffer* buffer,
    uint64_t offset,
    uint64_t size,
    uint32_t flags,
    const void* data);

void vulkan_buffer_copy_to(
    VulkanContext* context,
    VkCommandPool pool,
    VkQueue queue,
    VkBuffer source,
    uint64_t source_offset,
    VkBuffer dest,
    uint64_t dest_offset,
    uint64_t size);
