#pragma once

#include "renderer/vulkan/types.h"

const uint32_t VULKAN_MEMORY_TYPE_NOT_FOUND = UINT32_MAX;

uint32_t vulkan_find_memory_index(VulkanContext* context, uint32_t type_filter, uint32_t property_flags);
