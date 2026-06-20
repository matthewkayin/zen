#include "framebuffer.h"

#include "core/logger.h"

bool vulkan_framebuffer_create(
    VulkanContext* context,
    VulkanRenderpass* renderpass,
    uint32_t width, uint32_t height,
    uint32_t attachment_count,
    VkImageView* attachments,
    VulkanFramebuffer* out_framebuffer
) {
    // Copy the attachments
    out_framebuffer->attachment_count = attachment_count;
    out_framebuffer->attachments = (VkImageView*)malloc(out_framebuffer->attachment_count * sizeof(VkImageView));
    if (!out_framebuffer->attachments) {
        log_error("Failed to alloc framebuffer attachments.");
        return false;
    }
    memcpy(out_framebuffer->attachments, attachments, attachment_count * sizeof(VkImageView));

    out_framebuffer->renderpass = renderpass;

    // Create info
    VkFramebufferCreateInfo framebuffer_create_info {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = renderpass->handle,
        .attachmentCount = attachment_count,
        .pAttachments = out_framebuffer->attachments,
        .width = width,
        .height = height,
        .layers = 1
    };

    VK_CHECK(vkCreateFramebuffer(
        context->device.logical_device,
        &framebuffer_create_info,
        context->allocator,
        &out_framebuffer->handle));

    return true;
}

void vulkan_framebuffer_destroy(VulkanContext* context, VulkanFramebuffer* framebuffer) {
    vkDestroyFramebuffer(context->device.logical_device, framebuffer->handle, context->allocator);

    if (framebuffer->attachments) {
        free(framebuffer->attachments);
        framebuffer->attachments = nullptr;
    }

    framebuffer->handle = VK_NULL_HANDLE;
    framebuffer->attachment_count = 0;
    framebuffer->renderpass = nullptr;
}
