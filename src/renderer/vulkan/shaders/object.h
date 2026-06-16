#pragma once

#include "renderer/vulkan/types.h"

bool vulkan_object_shader_create(VulkanContext* context, VulkanObjectShader* out_shader);
void vulkan_object_shader_destroy(VulkanContext* context, VulkanObjectShader* shader);
void vulkan_object_shader_use(VulkanContext* context, VulkanObjectShader* shader);
