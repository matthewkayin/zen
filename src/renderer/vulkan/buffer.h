#pragma once

#include "renderer/vulkan/types.h"

struct VulkanBufferCreateParams {
    uint64_t size;
    VkBufferUsageFlags usage;
    VkMemoryPropertyFlags memory_property_flags;
    bool bind_on_create;
};

bool vulkan_buffer_create(VulkanContext* context, VulkanBufferCreateParams* params, VulkanBuffer* out_buffer);
void vulkan_buffer_destroy(VulkanContext* context, VulkanBuffer* buffer);

bool vulkan_buffer_resize(
    VulkanContext* context, VkCommandPool pool, VkQueue queue,
    VulkanBuffer* buffer, uint64_t new_size);
void vulkan_buffer_bind(VulkanContext* context, VulkanBuffer* buffer, uint64_t offset);

void* vulkan_buffer_lock_memory(
    VulkanContext* context, VulkanBuffer* buffer,
    uint64_t offset, uint64_t size, uint32_t flags);
void vulkan_buffer_unlock_memory(VulkanContext* context, VulkanBuffer* buffer);

void vulkan_buffer_load_data(
    VulkanContext* context, VulkanBuffer* buffer,
    uint64_t offset, uint64_t size, uint32_t flags, const void* data);
void vulkan_buffer_copy(
    VulkanContext* context, VkCommandPool pool, VkQueue queue,
    VkBuffer dst, uint64_t dst_offset, VkBuffer src, uint64_t src_offset, uint64_t size);
void vulkan_buffer_upload_data(
    VulkanContext* context, VkCommandPool pool, VkQueue queue,
    VulkanBuffer* buffer, uint64_t offset, uint64_t size, void* data);
