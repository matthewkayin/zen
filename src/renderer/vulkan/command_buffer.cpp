#include "command_buffer.h"
#include "renderer/vulkan/types.h"

void vulkan_command_buffer_allocate(
    VulkanContext* context,
    VkCommandPool pool,
    VkCommandBufferLevel level,
    VulkanCommandBuffer* out_command_buffer
) {
    memset(out_command_buffer, 0, sizeof(VulkanCommandBuffer));
    out_command_buffer->state = VulkanCommandBufferState::NOT_ALLOCATED;

    VkCommandBufferAllocateInfo allocate_info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool,
        .level = level,
        .commandBufferCount = 1,
    };

    VK_CHECK(vkAllocateCommandBuffers(
        context->device.logical_device,
        &allocate_info,
        &out_command_buffer->handle));
    out_command_buffer->state = VulkanCommandBufferState::READY;
}

void vulkan_command_buffer_free(VulkanContext* context, VkCommandPool pool, VulkanCommandBuffer* command_buffer) {
    vkFreeCommandBuffers(context->device.logical_device, pool, 1, &command_buffer->handle);
    command_buffer->handle = VK_NULL_HANDLE;
}

void vulkan_command_buffer_begin_recording(VulkanCommandBuffer* command_buffer, VkCommandBufferUsageFlags flags) {
    VkCommandBufferBeginInfo begin_info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = flags
    };

    VK_CHECK(vkBeginCommandBuffer(command_buffer->handle, &begin_info));
    command_buffer->state = VulkanCommandBufferState::RECORDING;
}

void vulkan_command_buffer_end_recording(VulkanCommandBuffer* command_buffer) {
    VK_CHECK(vkEndCommandBuffer(command_buffer->handle));
    command_buffer->state = VulkanCommandBufferState::RECORDING_ENDED;
}

void vulkan_command_buffer_set_submitted(VulkanCommandBuffer* command_buffer) {
    command_buffer->state = VulkanCommandBufferState::SUBMITTED;
}

void vulkan_command_buffer_reset(VulkanCommandBuffer* command_buffer) {
    command_buffer->state = VulkanCommandBufferState::READY;
}

void vulkan_command_buffer_begin_renderpass(
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

void vulkan_command_buffer_end_renderpass(VulkanCommandBuffer* command_buffer) {
    vkCmdEndRenderPass(command_buffer->handle);
    command_buffer->state = VulkanCommandBufferState::RECORDING;
}

void vulkan_command_buffer_bind_pipeline(
    VulkanCommandBuffer* command_buffer,
    VkPipelineBindPoint bind_point,
    VulkanPipeline* pipeline
) {
    vkCmdBindPipeline(command_buffer->handle, bind_point, pipeline->handle);
}

void vulkan_command_buffer_single_use_alloc_and_begin(
    VulkanContext* context,
    VkCommandPool pool,
    VulkanCommandBuffer* out_command_buffer
) {
    vulkan_command_buffer_allocate(context, pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, out_command_buffer);
    vulkan_command_buffer_begin_recording(out_command_buffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
}

void vulkan_command_buffer_single_use_end(
    VulkanCommandBuffer* command_buffer,
    VkQueue queue
) {
    vulkan_command_buffer_end_recording(command_buffer);

    VkSubmitInfo submit_info {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer->handle
    };

    VK_CHECK(vkQueueSubmit(queue, 1, &submit_info, nullptr));
    VK_CHECK(vkQueueWaitIdle(queue));
}
