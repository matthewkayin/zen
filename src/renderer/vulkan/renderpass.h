#pragma once

#include "renderer/vulkan/types.h"

void vulkan_renderpass_create(
    VulkanContext* context,
    float x, float y, float w, float h,
    float r, float g, float b, float a,
    float depth, uint32_t stencil,
    VulkanRenderpass* out_renderpass);
void vulkan_renderpass_destroy(VulkanContext* context, VulkanRenderpass* renderpass);
