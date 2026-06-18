#pragma once

#include "renderer/vulkan/types.h"

bool vulkan_result_is_error(VkResult result);
const char* vulkan_result_str(VkResult result);
