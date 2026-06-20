#include "renderpass.h"

void vulkan_renderpass_create(
    VulkanContext* context,
    float x, float y, float w, float h,
    float r, float g, float b, float a,
    float depth, uint32_t stencil,
    VulkanRenderpass* out_renderpass
) {
    out_renderpass->x = x;
    out_renderpass->y = y;
    out_renderpass->w = w;
    out_renderpass->h = h;

    out_renderpass->r = r;
    out_renderpass->g = g;
    out_renderpass->b = b;
    out_renderpass->a = a;

    out_renderpass->depth = depth;
    out_renderpass->stencil = stencil;

    // Color attachment
    VkAttachmentDescription color_attachment {
        .flags = 0,
        .format = context->swapchain.image_format.format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };

    VkAttachmentReference color_attachment_reference {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    // Depth attachment
    VkAttachmentDescription depth_attachment {
        .flags = 0,
        .format = context->device.depth_format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };

    VkAttachmentReference depth_attachment_reference = {
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };

    VkAttachmentDescription attachment_descriptions[] = {
        color_attachment,
        depth_attachment
    };
    const uint32_t attachment_descriptions_count = ARRAY_LENGTH(attachment_descriptions);

    // Main subpass
    VkSubpassDescription subpass {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = 0,
        .pInputAttachments = nullptr,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_reference,
        .pResolveAttachments = nullptr,
        .pDepthStencilAttachment = &depth_attachment_reference
    };

    // Render pass dependency
    VkSubpassDependency dependency {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dependencyFlags = 0
    };

    // Create info
    VkRenderPassCreateInfo renderpass_create_info {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = attachment_descriptions_count,
        .pAttachments = attachment_descriptions,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency
    };

    VK_CHECK(vkCreateRenderPass(
        context->device.logical_device, &renderpass_create_info, context->allocator, &out_renderpass->handle));
}

void vulkan_renderpass_destroy(VulkanContext* context, VulkanRenderpass* renderpass) {
    if (renderpass && renderpass->handle) {
        vkDestroyRenderPass(context->device.logical_device, renderpass->handle, context->allocator);
        renderpass->handle = nullptr;
    }
}

void vulkan_renderpass_begin(
    VulkanCommandBuffer* command_buffer,
    VulkanRenderpass* renderpass,
    VkFramebuffer framebuffer
) {
    VkClearValue clear_values[] = {
        { .color = { .float32 = { renderpass->r, renderpass->g, renderpass->b, renderpass->a }}},
        { .depthStencil = { .depth = renderpass->depth, .stencil = renderpass->stencil }},
    };

    VkRenderPassBeginInfo begin_info {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = renderpass->handle,
        .framebuffer = framebuffer,
        .renderArea = {
            .offset = {
                .x = (int)renderpass->x,
                .y = (int)renderpass->y
            },
            .extent = {
                .width = (uint32_t)renderpass->w,
                .height = (uint32_t)renderpass->h
            }
        },
        .clearValueCount = ARRAY_LENGTH(clear_values),
        .pClearValues = clear_values
    };

    vkCmdBeginRenderPass(command_buffer->handle, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
    command_buffer->state = VulkanCommandBufferState::IN_RENDER_PASS;
}

void vulkan_renderpass_end(VulkanCommandBuffer* command_buffer) {
    vkCmdEndRenderPass(command_buffer->handle);
    command_buffer->state = VulkanCommandBufferState::RECORDING;
}
