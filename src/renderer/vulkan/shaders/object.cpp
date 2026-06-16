#include "object.h"

#include "renderer/vulkan/shader_utils.h"
#include "renderer/vulkan/pipeline.h"
#include "core/logger.h"
#include "renderer/vulkan/types.h"
#include "math/vec3.h"
#include <cstring>

static const char* VULKAN_SHADER_NAME_OBJECT = "object";

bool vulkan_object_shader_create(VulkanContext* context, VulkanObjectShader* out_shader) {
    const char* const stage_type_strs[VULKAN_OBJECT_SHADER_STAGE_COUNT] = { "vert", "frag" };
    const VkShaderStageFlagBits stage_types[VULKAN_OBJECT_SHADER_STAGE_COUNT] = {
        VK_SHADER_STAGE_VERTEX_BIT,
        VK_SHADER_STAGE_FRAGMENT_BIT
    };

    for (uint32_t stage_index = 0; stage_index < VULKAN_OBJECT_SHADER_STAGE_COUNT; stage_index++) {
        if (!vulkan_shader_module_create(
                context,
                VULKAN_SHADER_NAME_OBJECT,
                stage_type_strs[stage_index],
                stage_types[stage_index],
                &out_shader->stages[stage_index])) {
            log_error("Unable to create %s shader module for %s.", stage_type_strs[stage_index], VULKAN_SHADER_NAME_OBJECT);
            return false;
        }
    }

    // TODO: descriptors

    // Pipeline creation
    VkViewport viewport;
    viewport.x = 0.0f;
    viewport.y = (float)context->framebuffer_height;
    viewport.width = (float)context->framebuffer_width;
    viewport.height = -(float)context->framebuffer_height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    // Scissor
    VkRect2D scissor;
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = context->framebuffer_width;
    scissor.extent.height = context->framebuffer_height;

    // Attributes
    uint32_t offset = 0;
    const uint32_t attribute_count = 1;
    VkVertexInputAttributeDescription attribute_descriptions[attribute_count];
    VkFormat formats[attribute_count];
    uint64_t attribute_sizes[attribute_count];

    // Attributes - Position
    formats[0] = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_sizes[0] = sizeof(vec3);

    for (uint32_t attribute_index = 0; attribute_index < attribute_count; attribute_index++) {
        attribute_descriptions[attribute_index].binding = 0;
        attribute_descriptions[attribute_index].location = attribute_index;
        attribute_descriptions[attribute_index].format = formats[attribute_index];
        attribute_descriptions[attribute_index].offset = offset;
        offset += attribute_sizes[attribute_index];
    }

    // TODO: descriptor set layouts

    // Stages
    VkPipelineShaderStageCreateInfo stage_create_infos[VULKAN_OBJECT_SHADER_STAGE_COUNT];
    memset(stage_create_infos, 0, sizeof(stage_create_infos));
    for (uint32_t stage_index = 0; stage_index < VULKAN_OBJECT_SHADER_STAGE_COUNT; stage_index++) {
        stage_create_infos[stage_index] = out_shader->stages[stage_index].shader_stage_create_info;
    }

    // Create pipeline
    VulkanGraphicsPipelineCreateParams params{};
    params.context = context;
    params.renderpass = &context->main_renderpass;
    params.attribute_count = attribute_count;
    params.attributes = attribute_descriptions;
    params.descriptor_set_layout_count = 0;
    params.descriptor_set_layouts = nullptr;
    params.stage_count = VULKAN_OBJECT_SHADER_STAGE_COUNT;
    params.stages = stage_create_infos;
    params.viewport = viewport;
    params.scissor = scissor;
    params.is_wireframe = false;
    if (!vulkan_graphics_pipeline_create(params, &out_shader->pipeline)) {
        log_error("Failed to create graphcis pipeline for object shader.");
        return false;
    }

    return true;
}

void vulkan_object_shader_destroy(VulkanContext* context, VulkanObjectShader* shader) {
    vulkan_pipeline_destroy(context, &shader->pipeline);

    for (uint32_t stage_index = 0; stage_index < VULKAN_OBJECT_SHADER_STAGE_COUNT; stage_index++) {
        vkDestroyShaderModule(context->device.logical_device, shader->stages[stage_index].handle, context->allocator);
        shader->stages[stage_index].handle = VK_NULL_HANDLE;
    }
}

void vulkan_object_shader_use(VulkanContext* context, VulkanObjectShader* shader) {

}
