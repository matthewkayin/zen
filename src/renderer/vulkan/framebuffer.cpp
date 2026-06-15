#include "framebuffer.h"

void vulkan_framebuffer_create(
        VulkanContext* context,
        VulkanRenderpass* renderpass,
        uint32_t width,
        uint32_t height,
        uint32_t attachment_count,
        VkImageView* attachments,
        VulkanFramebuffer* out_framebuffer) {

    // Copy the attachments, renderpass, and attachment count
    out_framebuffer->attachments = (VkImageView*)malloc(attachment_count * sizeof(VkImageView));
    for (uint32_t index = 0; index < attachment_count; index++) {
        out_framebuffer->attachments[index] = attachments[index];
    }

    out_framebuffer->renderpass = renderpass;
    out_framebuffer->attachment_count = attachment_count;

    // Create info
    VkFramebufferCreateInfo framebuffer_create_info{};
    framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_create_info.renderPass = renderpass->handle;
    framebuffer_create_info.attachmentCount = attachment_count;
    framebuffer_create_info.pAttachments = out_framebuffer->attachments;
    framebuffer_create_info.width = width;
    framebuffer_create_info.height = height;
    framebuffer_create_info.layers = 1;

    VK_CHECK(vkCreateFramebuffer(
        context->device.logical_device,
        &framebuffer_create_info,
        context->allocator,
        &out_framebuffer->handle));
}

void vulkan_framebuffer_destroy(VulkanContext* context, VulkanFramebuffer* framebuffer) {
    vkDestroyFramebuffer(context->device.logical_device, framebuffer->handle, context->allocator);
    if (framebuffer->attachments) {
        free(framebuffer->attachments);
        framebuffer->attachments = nullptr;
    }

    framebuffer->handle = nullptr;
    framebuffer->attachment_count = 0;
    framebuffer->renderpass = nullptr;
}
