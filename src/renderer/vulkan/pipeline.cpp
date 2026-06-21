#include "pipeline.h"

#include "core/logger.h"
#include "math/vertex3d.h"
#include "renderer/vulkan/utils.h"

bool vulkan_graphics_pipeline_create(
    VulkanContext* context,
    VulkanGraphicsPipelineCreateParams* params,
    VulkanPipeline* out_pipeline
) {
    // Viewport state
    VkPipelineViewportStateCreateInfo viewport_state_create_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &params->viewport,
        .scissorCount = 1,
        .pScissors = &params->scissor
    };

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterization_state_create_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = params->is_wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasClamp = 0.0f,
        .depthBiasSlopeFactor = 0.0f,
        .lineWidth = 1.0f
    };

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisample_state_create_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 1.0f,
        .pSampleMask = nullptr,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE
    };

    // Depth and stencil testing
    VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE
    };

    // Color blend attachment
    VkPipelineColorBlendAttachmentState color_blend_attachment_state {
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };

    // Color blend state
    VkPipelineColorBlendStateCreateInfo color_blend_state_create_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment_state
    };

    // Dynamic state
    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_LINE_WIDTH
    };

    VkPipelineDynamicStateCreateInfo dynamic_state_create_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = ARRAY_LENGTH(dynamic_states),
        .pDynamicStates = dynamic_states
    };

    // Vertex input
    VkVertexInputBindingDescription vertex_input_binding_description {
        .binding = 0,
        .stride = sizeof(Vertex3d),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    // Attributes
    VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vertex_input_binding_description,
        .vertexAttributeDescriptionCount = params->attribute_count,
        .pVertexAttributeDescriptions = params->attributes
    };

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipeline_layout_create_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = params->descriptor_set_layout_count,
        .pSetLayouts = params->descriptor_set_layouts
    };

    VK_CHECK(vkCreatePipelineLayout(
        context->device.logical_device,
        &pipeline_layout_create_info,
        context->allocator,
        &out_pipeline->pipeline_layout));

    // Pipeline create info
    VkGraphicsPipelineCreateInfo pipeline_create_info {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = params->stage_count,
        .pStages = params->stages,
        .pVertexInputState = &vertex_input_state_create_info,
        .pInputAssemblyState = &input_assembly_create_info,
        .pTessellationState = nullptr,
        .pViewportState = &viewport_state_create_info,
        .pRasterizationState = &rasterization_state_create_info,
        .pMultisampleState = &multisample_state_create_info,
        .pDepthStencilState = &depth_stencil_create_info,
        .pColorBlendState = &color_blend_state_create_info,
        .pDynamicState = &dynamic_state_create_info,
        .layout = out_pipeline->pipeline_layout,
        .renderPass = params->renderpass->handle,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };

    VkResult result = vkCreateGraphicsPipelines(
        context->device.logical_device,
        VK_NULL_HANDLE, 1, &pipeline_create_info,
        context->allocator,
        &out_pipeline->handle);
    if (vulkan_result_is_error(result)) {
        log_error("Failed to create graphics pipeline: %s.", vulkan_result_str(result));
        return false;
    }

    log_info("Vulkan graphics pipeline created.");
    return true;
}

void vulkan_pipeline_destroy(VulkanContext* context, VulkanPipeline* pipeline) {
    if (!pipeline) {
        return;
    }

    // Destroy pipeline
    if (pipeline->handle) {
        vkDestroyPipeline(context->device.logical_device, pipeline->handle, context->allocator);
        pipeline->handle = nullptr;
    }

    // Destroy layout
    if (pipeline->pipeline_layout) {
        vkDestroyPipelineLayout(context->device.logical_device, pipeline->pipeline_layout, context->allocator);
        pipeline->pipeline_layout = nullptr;
    }
}
