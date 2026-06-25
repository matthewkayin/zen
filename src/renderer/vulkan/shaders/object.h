#pragma once

#include "renderer/vulkan/types.h"

bool vulkan_object_shader_create(VulkanContext* context, VulkanObjectShader* out_shader);
bool vulkan_object_shader_alloc_descriptor_sets(VulkanContext* context, VulkanObjectShader* shader);
void vulkan_object_shader_free_descriptor_sets(VulkanContext* context, VulkanObjectShader* shader);
void vulkan_object_shader_destroy(VulkanContext* context, VulkanObjectShader* shader);
void vulkan_object_shader_use(VulkanContext* context, VulkanObjectShader* shader);
void vulkan_object_shader_update_global_state(VulkanContext* context, VulkanObjectShader* shader);
void vulkan_object_shader_update_object(VulkanContext* context, VulkanObjectShader* shader, mat4 model);
