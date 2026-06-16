#pragma once

#include "renderer/vulkan/types.h"

bool vulkan_shader_module_create(
    VulkanContext* context,
    const char* name,
    const char* type_str,
    VkShaderStageFlagBits stage_flag,
    VulkanShaderStage* out_stage);
