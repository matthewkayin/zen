#pragma once

#include <vulkan/vulkan.h>

bool vulkan_result_is_error(VkResult result);
const char* vulkan_result_str(VkResult result);
