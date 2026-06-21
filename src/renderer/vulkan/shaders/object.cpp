#include "object.h"

#include "core/logger.h"
#include "math/vec3.h"
#include "renderer/vulkan/shader_utils.h"
#include "renderer/vulkan/pipeline.h"
#include "renderer/vulkan/command_buffer.h"

static const char* VULKAN_SHADER_NAME_OBJECT = "object";

bool vulkan_object_shader_create(VulkanContext* context, VulkanObjectShader* out_shader) {
    const char* const stage_type_strs[VULKAN_OBJECT_SHADER_STAGE_COUNT] = { "vert", "frag" };
    const VkShaderStageFlagBits stage_types[VULKAN_OBJECT_SHADER_STAGE_COUNT] = {
        VK_SHADER_STAGE_VERTEX_BIT,
        VK_SHADER_STAGE_FRAGMENT_BIT
    };

    for (uint32_t stage_index = 0; stage_index < VULKAN_OBJECT_SHADER_STAGE_COUNT; stage_index++) {
        if (!vulkan_shader_module_create(
            context, VULKAN_SHADER_NAME_OBJECT,
            stage_type_strs[stage_index], stage_types[stage_index],
            &out_shader->stages[stage_index])
        ) {
            log_error("Unable to create %s shader module for %s.",
                stage_type_strs[stage_index], VULKAN_SHADER_NAME_OBJECT);
            return false;
        }
    }

    // TODO: descriptors

    // Pipeline creation

    VkViewport viewport {
        .x = 0.0f,
        .y = (float)context->framebuffer_height,
        .width = (float)context->framebuffer_width,
        .height = -(float)context->framebuffer_height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    VkRect2D scissor {
        .offset = {
            .x = 0, .y = 0
        },
        .extent = {
            .width = context->framebuffer_width,
            .height = context->framebuffer_height
        }
    };

    // Attributes
    const uint32_t attribute_count = 1;
    VkVertexInputAttributeDescription attribute_descriptions[attribute_count];
    VkFormat formats[attribute_count];
    uint64_t attribute_sizes[attribute_count];

    // Attributes - Position
    formats[0] = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_sizes[0] = sizeof(vec3);

    uint32_t offset = 0;
    for (uint32_t attribute_index = 0; attribute_index < attribute_count; attribute_index++) {
        attribute_descriptions[attribute_index] = {
            .location = attribute_index,
            .binding = 0,
            .format = formats[attribute_index],
            .offset = offset
        };
        offset += attribute_index;
    }

    // TODO: descriptor set layouts

    // Stages
    VkPipelineShaderStageCreateInfo stage_create_infos[VULKAN_OBJECT_SHADER_STAGE_COUNT];
    for (uint32_t stage_index = 0; stage_index < VULKAN_OBJECT_SHADER_STAGE_COUNT; stage_index++) {
        stage_create_infos[stage_index] = out_shader->stages[stage_index].shader_stage_create_info;
    }

    // Create pipeline
    VulkanGraphicsPipelineCreateParams graphics_pipeline_create_params {
        .renderpass = &context->main_renderpass,
        .attribute_count = attribute_count,
        .attributes = attribute_descriptions,
        .descriptor_set_layout_count = 0,
        .descriptor_set_layouts = nullptr,
        .stage_count = VULKAN_OBJECT_SHADER_STAGE_COUNT,
        .stages = stage_create_infos,
        .viewport = viewport,
        .scissor = scissor,
        .is_wireframe = false
    };
    if (!vulkan_graphics_pipeline_create(context, &graphics_pipeline_create_params, &out_shader->pipeline)) {
        log_error("vulkan_object_shader_create - Pipeline creation failed.");
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
    vulkan_command_buffer_bind_pipeline(
        &context->graphics_command_buffers[context->image_index], VK_PIPELINE_BIND_POINT_GRAPHICS,
        &shader->pipeline);
}
