#include "object.h"

#include "core/logger.h"
#include "math/vec3.h"
#include "renderer/vulkan/shader_utils.h"
#include "renderer/vulkan/pipeline.h"
#include "renderer/vulkan/command_buffer.h"
#include "renderer/vulkan/buffer.h"

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

    // Global UBO - Descriptor Set Layout
    VkDescriptorSetLayoutBinding global_ubo_layout_binding {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .pImmutableSamplers = 0
    };
    VkDescriptorSetLayoutCreateInfo global_ubo_layout_create_info {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &global_ubo_layout_binding
    };
    VK_CHECK(vkCreateDescriptorSetLayout(
        context->device.logical_device, &global_ubo_layout_create_info,
        context->allocator, &out_shader->global_descriptor_set_layout));

    // Global UBO - Descriptor Pool
    VkDescriptorPoolSize global_pool_size {
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = context->swapchain.image_count
    };
    VkDescriptorPoolCreateInfo global_pool_create_info {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = context->swapchain.image_count,
        .poolSizeCount = 1,
        .pPoolSizes = &global_pool_size
    };
    VK_CHECK(vkCreateDescriptorPool(
        context->device.logical_device, &global_pool_create_info,
        context->allocator, &out_shader->global_descriptor_pool));

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

    // Descriptor set layouts
    VkDescriptorSetLayout descriptor_set_layouts[] = {
        out_shader->global_descriptor_set_layout
    };

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
        .descriptor_set_layout_count = ARRAY_LENGTH(descriptor_set_layouts),
        .descriptor_set_layouts = descriptor_set_layouts,
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

    // Create global uniform buffer
    VulkanBufferCreateParams global_uniform_buffer_create_params {
        .size = context->swapchain.image_count * sizeof(GlobalUniformObject),
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .memory_property_flags =
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        .bind_on_create = true
    };
    if (!vulkan_buffer_create(context, &global_uniform_buffer_create_params, &out_shader->global_uniform_buffer)) {
        log_error("vulkan_object_shader_create() - Failed to create global uniform buffer.");
        return false;
    }

    return true;
}

bool vulkan_object_shader_alloc_descriptor_sets(VulkanContext* context, VulkanObjectShader* shader) {
    // Alloc global descriptor sets
    std::vector<VkDescriptorSetLayout> global_descriptor_set_layouts(
        context->swapchain.image_count, shader->global_descriptor_set_layout);

    shader->global_descriptor_sets =
        (VkDescriptorSet*)malloc(global_descriptor_set_layouts.size() * sizeof(VkDescriptorSet));
    if (!shader->global_descriptor_sets) {
        log_error("vulkan_object_shader_create() - Failed to alloc global_descritor_sets.");
        return false;
    }

    VkDescriptorSetAllocateInfo global_descriptor_set_allocate_info {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = shader->global_descriptor_pool,
        .descriptorSetCount = (uint32_t)global_descriptor_set_layouts.size(),
        .pSetLayouts = global_descriptor_set_layouts.data()
    };
    VK_CHECK(vkAllocateDescriptorSets(
        context->device.logical_device, &global_descriptor_set_allocate_info, shader->global_descriptor_sets));

    return true;
}

void vulkan_object_shader_free_descriptor_sets(VulkanContext* context, VulkanObjectShader* shader) {
    vkResetDescriptorPool(context->device.logical_device, shader->global_descriptor_pool, 0);
    free(shader->global_descriptor_sets);
}

void vulkan_object_shader_destroy(VulkanContext* context, VulkanObjectShader* shader) {
    vulkan_buffer_destroy(context, &shader->global_uniform_buffer);
    vulkan_pipeline_destroy(context, &shader->pipeline);

    vkDestroyDescriptorPool(context->device.logical_device, shader->global_descriptor_pool, context->allocator);
    vkDestroyDescriptorSetLayout(
        context->device.logical_device, shader->global_descriptor_set_layout, context->allocator);

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

void vulkan_object_shader_update_global_state(VulkanContext* context, VulkanObjectShader* shader) {
    VkCommandBuffer command_buffer = context->graphics_command_buffers[context->image_index].handle;
    VkDescriptorSet global_descriptor = shader->global_descriptor_sets[context->image_index];

    // Configure the descriptors for the given index
    uint32_t range = sizeof(GlobalUniformObject);
    uint64_t offset = context->image_index * sizeof(GlobalUniformObject);

    // Copy data to the buffer
    vulkan_buffer_load_data(context, &shader->global_uniform_buffer, offset, range, 0, &shader->global_ubo);

    VkDescriptorBufferInfo descriptor_buffer_info {
        .buffer = shader->global_uniform_buffer.handle,
        .offset = offset,
        .range = range
    };

    VkWriteDescriptorSet descriptor_write {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = shader->global_descriptor_sets[context->image_index],
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &descriptor_buffer_info
    };

    vkUpdateDescriptorSets(context->device.logical_device, 1, &descriptor_write, 0, nullptr);

    vkCmdBindDescriptorSets(
        command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        shader->pipeline.pipeline_layout, 0, 1, &global_descriptor, 0, nullptr);
}
