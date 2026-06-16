#pragma once

#include "renderer/vulkan/types.h"

struct VulkanGraphicsPipelineCreateParams {
    VulkanContext* context;
    VulkanRenderpass* renderpass;
    uint32_t attribute_count;
    VkVertexInputAttributeDescription* attributes;
    uint32_t descriptor_set_layout_count;
    VkDescriptorSetLayout* descriptor_set_layouts;
    uint32_t stage_count;
    VkPipelineShaderStageCreateInfo* stages;
    VkViewport viewport;
    VkRect2D scissor;
    bool is_wireframe;
};

bool vulkan_graphics_pipeline_create(VulkanGraphicsPipelineCreateParams params, VulkanPipeline* out_pipeline);
void vulkan_pipeline_destroy(VulkanContext* context, VulkanPipeline* pipeline);
void vulkan_pipeline_bind(VulkanCommandBuffer* command_buffer, VkPipelineBindPoint bind_point, VulkanPipeline* pipeline);
