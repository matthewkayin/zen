#include "backend.h"

#include "defines.h"
#include "core/logger.h"
#include "renderer/vulkan/device.h"
#include "renderer/vulkan/swapchain.h"
#include "renderer/vulkan/renderpass.h"
#include "renderer/vulkan/command_buffer.h"
#include "renderer/vulkan/framebuffer.h"
#include "renderer/vulkan/utils.h"
#include "vulkan/vulkan_core.h"
#include <SDL3/SDL_vulkan.h>
#include <vector>
#include <cstring>

VKAPI_ATTR VkBool32 VKAPI_CALL renderer_backend_vulkan_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_types,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data);

bool RendererBackendVulkan::init() {
    // I'm probably not going to use this,
    // but it doesn't hurt to have it in place
    m_context.allocator = nullptr;

    // Get framebuffer width and height
    int window_width, window_height;
    SDL_GetWindowSize(m_window, &window_width, &window_height);
    m_cached_framebuffer_width = (uint32_t)window_width;
    m_cached_framebuffer_height = (uint32_t)window_height;
    m_context.framebuffer_width = (uint32_t)window_width;
    m_context.framebuffer_height = (uint32_t)window_height;

    // Vulkan application info
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = ZEN_APP_NAME;
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "Zen Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_4;

    // Extension and layer names
    std::vector<const char*> extension_names;
    std::vector<const char*> layer_names;

    // Get platform extensions from SDL
    uint32_t instance_extension_count;
    const char* const* instance_extensions = SDL_Vulkan_GetInstanceExtensions(&instance_extension_count);
    if (instance_extensions == nullptr) {
        log_error("Failed to get platform-specific Vulkan extensions: %s", SDL_GetError());
        return false;
    }
    for (uint32_t instance_extension_index = 0; instance_extension_index < instance_extension_count; instance_extension_index++) {
        extension_names.push_back(instance_extensions[instance_extension_index]);
    }

#ifdef ZEN_DEBUG
    // Debug extensions
    extension_names.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    // Print list of extensions
    log_debug("Required Vulkan extensions:");
    for (uint32_t index = 0; index < extension_names.size(); index++) {
        log_debug("%s", extension_names[index]);
    }

    // Debug layers
    layer_names.push_back("VK_LAYER_KHRONOS_validation");

    // Get a list of all available validation layers
    uint32_t available_layer_count;
    VK_CHECK(vkEnumerateInstanceLayerProperties(&available_layer_count, 0));
    std::vector<VkLayerProperties> available_layers(available_layer_count);
    VK_CHECK(vkEnumerateInstanceLayerProperties(&available_layer_count, available_layers.data()));

    // Verify all required layers are available
    for (uint32_t index = 0; index < (uint32_t)layer_names.size(); index++) {
        uint32_t layer_index;
        for (layer_index = 0; layer_index < available_layer_count; layer_index++) {
            if (strcmp(layer_names[index], available_layers[layer_index].layerName) == 0) {
                break;
            }
        }

        if (layer_index < available_layer_count) {
            log_info("Found layer %s.", layer_names[index]);
        } else {
            log_error("Missing required layer %s.", layer_names[index]);
            return false;
        }
    }
    log_info("All required layers are available.");
#endif

    // Fill out Vulkan create info
    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = (uint32_t)extension_names.size();
    create_info.ppEnabledExtensionNames = extension_names.data();
    create_info.enabledLayerCount = (uint32_t)layer_names.size();
    create_info.ppEnabledLayerNames = layer_names.data();

    // Create instance
    VkResult result = vkCreateInstance(&create_info, m_context.allocator, &m_context.instance);
    if (result != VK_SUCCESS) {
        log_error("vkCreateInstance failed with result %u", result);
        return false;
    }
    log_info("Vulkan instance created.");

#ifdef ZEN_DEBUG
    log_debug("Creating Vulkan debugger...");

    VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
    debug_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug_create_info.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    debug_create_info.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    debug_create_info.pfnUserCallback = renderer_backend_vulkan_debug_callback;
    debug_create_info.pUserData = nullptr;

    PFN_vkCreateDebugUtilsMessengerEXT createDebugUtilsMessenger =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_context.instance, "vkCreateDebugUtilsMessengerEXT");
    ZEN_ASSERT_MESSAGE(createDebugUtilsMessenger, "Failed to load createDebugUtilsMessenger function pointer.");
    VK_CHECK(createDebugUtilsMessenger(m_context.instance, &debug_create_info, m_context.allocator, &m_context.debug_messenger));
    log_debug("Vulkan debugger created.");
#endif

    // Create surface
    if (!SDL_Vulkan_CreateSurface(m_window, m_context.instance, m_context.allocator, &m_context.surface)) {
        log_error("Failed to create surface %s", SDL_GetError());
        return false;
    }

    // Init device
    if (!vulkan_device_create(&m_context)) {
        log_error("Failed to create device.");
        return false;
    }

    // Init swapchain
    vulkan_swapchain_create(
        &m_context,
        m_context.framebuffer_width,
        m_context.framebuffer_height,
        &m_context.swapchain);

    // Renderpass
    vulkan_renderpass_create(
        &m_context,
        &m_context.main_renderpass,
        0.0f, 0.0f, m_context.framebuffer_width, m_context.framebuffer_height,
        0.0f, 0.0f, 0.2f, 1.0f,
        1.0f,
        0);

    // Framebuffer
    m_context.swapchain.framebuffers = (VulkanFramebuffer*)malloc(m_context.swapchain.image_count * sizeof(VulkanFramebuffer));
    regenerate_framebuffers(&m_context.swapchain, &m_context.main_renderpass);

    // Create command buffers
    create_command_buffers();

    // Create acquire semaphores
    m_context.acquire_semaphores = (VkSemaphore*)malloc(m_context.swapchain.max_frames_in_flight * sizeof(VkSemaphore));
    for (uint32_t index = 0; index < m_context.swapchain.max_frames_in_flight; index++) {
        VkSemaphoreCreateInfo semaphore_create_info{};
        semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VK_CHECK(vkCreateSemaphore(
            m_context.device.logical_device,
            &semaphore_create_info,
            m_context.allocator,
            &m_context.acquire_semaphores[index]));
    }

    // Create submit semaphores
    m_context.submit_semaphores = (VkSemaphore*)malloc(m_context.swapchain.image_count * sizeof(VkSemaphore));
    for (uint32_t index = 0; index < m_context.swapchain.image_count; index++) {
        VkSemaphoreCreateInfo semaphore_create_info{};
        semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VK_CHECK(vkCreateSemaphore(
            m_context.device.logical_device,
            &semaphore_create_info,
            m_context.allocator,
            &m_context.submit_semaphores[index]));
    }

    // Create frame fences
    m_context.frame_fences = (VkFence*)malloc(m_context.swapchain.max_frames_in_flight * sizeof(VkFence));
    for (uint32_t index = 0; index < m_context.swapchain.max_frames_in_flight; index++) {
        // Create the fence in a signaled state, indicating that the first frame has already been renderered
        // This prevents the application from waiting indefinitely for the first frame to render since it
        // cannot be rendered until a frame is renderered before it
        VkFenceCreateInfo fence_create_info{};
        fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VK_CHECK(vkCreateFence(
            m_context.device.logical_device,
            &fence_create_info,
            m_context.allocator,
            &m_context.frame_fences[index]));
    }

    log_info("Vulkan backend initialized successfully.");
    return true;
}

void RendererBackendVulkan::quit() {
    // Wait for the device to finish before shutting down
    vkDeviceWaitIdle(m_context.device.logical_device);

    // Destroy acquire semaphores
    for (uint32_t index = 0; index < m_context.swapchain.max_frames_in_flight; index++) {
        vkDestroySemaphore(
            m_context.device.logical_device,
            m_context.acquire_semaphores[index],
            m_context.allocator);
        m_context.acquire_semaphores[index] = nullptr;
    }
    free(m_context.acquire_semaphores);
    m_context.acquire_semaphores = nullptr;

    // Destroy submit semaphores
    for (uint32_t index = 0; index < m_context.swapchain.image_count; index++) {
        vkDestroySemaphore(
            m_context.device.logical_device,
            m_context.submit_semaphores[index],
            m_context.allocator);
        m_context.submit_semaphores[index] = nullptr;
    }
    free(m_context.submit_semaphores);
    m_context.submit_semaphores = nullptr;

    // Destroy frame fences
    for (uint32_t index = 0; index < m_context.swapchain.max_frames_in_flight; index++) {
        vkDestroyFence(m_context.device.logical_device, m_context.frame_fences[index], m_context.allocator);
    }
    free(m_context.frame_fences);
    m_context.frame_fences = nullptr;

    // Command buffers
    for (uint32_t index = 0; index < m_context.swapchain.image_count; index++) {
        if (m_context.graphics_command_buffers[index].handle) {
            vulkan_command_buffer_free(
                &m_context,
                m_context.device.graphics_command_pool,
                &m_context.graphics_command_buffers[index]);
            m_context.graphics_command_buffers[index].handle = nullptr;
        }
    }
    free(m_context.graphics_command_buffers);
    m_context.graphics_command_buffers = nullptr;

    // Free framebuffers
    for (uint32_t index = 0; index < m_context.swapchain.image_count; index++) {
        vulkan_framebuffer_destroy(&m_context, &m_context.swapchain.framebuffers[index]);
    }
    free(m_context.swapchain.framebuffers);
    m_context.swapchain.framebuffers = nullptr;

    // Renderpass
    log_info("Destroying Vulkan renderpass...");
    vulkan_renderpass_destroy(&m_context, &m_context.main_renderpass);

    log_info("Destroying Vulkan swapchain...");
    vulkan_swapchain_destroy(&m_context, &m_context.swapchain);

    log_info("Destroying Vulkan device...");
    vulkan_device_destroy(&m_context);

    log_info("Destroying Vulkan surface...");
    SDL_Vulkan_DestroySurface(m_context.instance, m_context.surface, m_context.allocator);
    m_context.surface = nullptr;

#ifdef ZEN_DEBUG
    log_info("Destroying Vulkan debugger...");
    if (m_context.debug_messenger) {
        PFN_vkDestroyDebugUtilsMessengerEXT destroyDebugUtilsMessenger =
            (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_context.instance, "vkDestroyDebugUtilsMessengerEXT");
        destroyDebugUtilsMessenger(m_context.instance, m_context.debug_messenger, nullptr);
    }
#endif

    log_info("Destroying Vulkan instance...");
    vkDestroyInstance(m_context.instance, nullptr);
}

void RendererBackendVulkan::on_resized(uint32_t width, uint32_t height) {
    m_cached_framebuffer_width = width;
    m_cached_framebuffer_height = height;
    m_context.framebuffer_size_generation++;

    log_debug("RendererBackendVulkan::on_resized - %ux%u generation %llu", width, height, m_context.framebuffer_size_generation);
}

bool RendererBackendVulkan::begin_frame(double delta_time) {
    VulkanDevice* device = &m_context.device;

    // If recreating swapchain, do nothing
    if (m_context.is_recreating_swapchain) {
        VkResult result = vkDeviceWaitIdle(device->logical_device);
        if (!vulkan_result_is_success(result)) {
            log_error("RendererBackendVulkan::begin_frame - vkDeviceWaitIdle (1) failed: %s", vulkan_result_str(result));
            return false;
        }

        log_info("RendererBackendVulkan::begin_frame - doing nothing because we are recreating swapchain.");
        return false;
    }

    // Check if the framebuffer has been resized. If so, a new swapchain must be recreated.
    if (m_context.framebuffer_size_generation != m_context.framebuffer_size_last_generation) {
        VkResult result = vkDeviceWaitIdle(device->logical_device);
        if (!vulkan_result_is_success(result)) {
            log_error("RendererBackendVulkan::begin_frame - vkDeviceWaitIdle (2) failed: %s", vulkan_result_str(result));
            return false;
        }

        // Recreate swapchain
        // On failure, boot out before unsetting the flag
        if (!recreate_swapchain()) {
            return false;
        }

        log_info("RendererBackendVulkan::begin_frame - Resize successful.");
        return false;
    }

    // Wait for execution of the current frame to complete
    VkResult fence_wait_result = vkWaitForFences(
        m_context.device.logical_device,
        1,
        &m_context.frame_fences[m_context.frame_index],
        VK_TRUE,
        UINT64_MAX);
    if (fence_wait_result != VK_SUCCESS) {
        log_warn("RendererBackendVulkan::begin_frame - Fence wait failed with code %s", vulkan_result_str(fence_wait_result));
        return false;
    }
    VK_CHECK(vkResetFences(
        m_context.device.logical_device, 1, &m_context.frame_fences[m_context.frame_index]));

    // Acquire the next image from the swap chain
    // Pass along the semaphore that should be signaled when this completes
    // This same semaphore will later be waited on by queue submission to ensure this image is available
    if (!vulkan_swapchain_acquire_next_image_index(
            &m_context,
            &m_context.swapchain,
            UINT64_MAX,
            m_context.acquire_semaphores[m_context.frame_index],
            nullptr,
            &m_context.image_index)) {
        return false;
    }

    // Begin recording commands
    VulkanCommandBuffer* command_buffer = &m_context.graphics_command_buffers[m_context.image_index];
    vulkan_command_buffer_reset(command_buffer);
    vulkan_command_buffer_begin(command_buffer, false, false, false);

    // Dynamic state
    VkViewport viewport;
    viewport.x = 0.0f;
    viewport.y = (float)m_context.framebuffer_height;
    viewport.width = (float)m_context.framebuffer_width;
    viewport.height = -(float)m_context.framebuffer_height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    // Scissor
    VkRect2D scissor;
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = m_context.framebuffer_width;
    scissor.extent.height = m_context.framebuffer_height;

    vkCmdSetViewport(command_buffer->handle, 0, 1, &viewport);
    vkCmdSetScissor(command_buffer->handle, 0, 1, &scissor);

    m_context.main_renderpass.w = m_context.framebuffer_width;
    m_context.main_renderpass.h = m_context.framebuffer_height;

    // Begin the renderpass
    vulkan_renderpass_begin(
        command_buffer,
        &m_context.main_renderpass,
        m_context.swapchain.framebuffers[m_context.image_index].handle);

    return true;
}

bool RendererBackendVulkan::end_frame(double delta_time) {
    VulkanCommandBuffer* command_buffer = &m_context.graphics_command_buffers[m_context.image_index];

    vulkan_renderpass_end(command_buffer, &m_context.main_renderpass);
    vulkan_command_buffer_end(command_buffer);

    VkPipelineStageFlags pipeline_stage_flags[1] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer->handle;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &m_context.acquire_semaphores[m_context.frame_index];
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &m_context.submit_semaphores[m_context.image_index];
    submit_info.pWaitDstStageMask = pipeline_stage_flags;

    VkResult result = vkQueueSubmit(
        m_context.device.graphics_queue,
        1,
        &submit_info,
        m_context.frame_fences[m_context.frame_index]);
    if (result != VK_SUCCESS) {
        log_error("RendererBackendVulkan::end_frame - vkQueueSubmit failed with result %s", vulkan_result_str(result));
        return false;
    }

    vulkan_command_buffer_update_submitted(command_buffer);
    // End queue submission

    vulkan_swapchain_present(
        &m_context,
        &m_context.swapchain,
        m_context.device.present_queue,
        m_context.submit_semaphores[m_context.image_index],
        m_context.image_index);

    return true;
}

// PRIVATE

void RendererBackendVulkan::create_command_buffers() {
    if (!m_context.graphics_command_buffers) {
        m_context.graphics_command_buffers = (VulkanCommandBuffer*)malloc(m_context.swapchain.image_count * sizeof(VulkanCommandBuffer));
        memset(m_context.graphics_command_buffers, 0, m_context.swapchain.image_count * sizeof(VulkanCommandBuffer));
    }

    for (uint32_t index = 0; index < m_context.swapchain.image_count; index++) {
        if (m_context.graphics_command_buffers[index].handle) {
            vulkan_command_buffer_free(
                &m_context,
                m_context.device.graphics_command_pool,
                &m_context.graphics_command_buffers[index]);
        }
        memset(&m_context.graphics_command_buffers[index], 0, sizeof(VulkanCommandBuffer));
        vulkan_command_buffer_allocate(
            &m_context,
            m_context.device.graphics_command_pool,
            true,
            &m_context.graphics_command_buffers[index]);
    }

    log_info("Vulkan command buffers created.");
}

void RendererBackendVulkan::regenerate_framebuffers(VulkanSwapchain* swapchain, VulkanRenderpass* renderpass) {
    for (uint32_t index = 0; index < m_context.swapchain.image_count; index++) {
        const uint32_t attachment_count = 2;
        VkImageView attachments[attachment_count] = {
            swapchain->views[index],
            swapchain->depth_attachment.view
        };

        vulkan_framebuffer_create(
            &m_context,
            renderpass,
            m_context.framebuffer_width,
            m_context.framebuffer_height,
            attachment_count,
            attachments,
            &m_context.swapchain.framebuffers[index]);
    }
}

bool RendererBackendVulkan::recreate_swapchain() {
    // If already being recreated, don't try again
    if (m_context.is_recreating_swapchain) {
        log_warn("RendererBackendVulkan::recreate_swapchain called when already recreating.");
        return false;
    }

    // Detect if window is too small to be drawn to
    if (m_context.framebuffer_width == 0 || m_context.framebuffer_height == 0) {
        log_warn("RendererBackendVulkan::recreate_swapchain called when window dimensions are 0.");
        return false;
    }

    // Mark as recreating
    m_context.is_recreating_swapchain = true;

    // Wait for any operations to complete
    vkDeviceWaitIdle(m_context.device.logical_device);

    // Requery support
    vulkan_device_query_swapchain_support(
        m_context.device.physical_device,
        m_context.surface,
        &m_context.device.swapchain_support_info);
    vulkan_device_detect_depth_format(&m_context.device);

    vulkan_swapchain_recreate(
        &m_context,
        m_cached_framebuffer_width,
        m_cached_framebuffer_height,
        &m_context.swapchain);

    // Sync the framebuffer with the cached sizes
    m_context.framebuffer_width = m_cached_framebuffer_width;
    m_context.framebuffer_height = m_cached_framebuffer_height;
    m_context.main_renderpass.w = m_context.framebuffer_width;
    m_context.main_renderpass.h = m_context.framebuffer_height;
    m_cached_framebuffer_width = 0;
    m_cached_framebuffer_height = 0;

    // Update framebuffer size generation
    m_context.framebuffer_size_last_generation = m_context.framebuffer_size_generation;

    // Cleanup swapchain
    for (uint32_t index = 0; index < m_context.swapchain.image_count; index++) {
        vulkan_command_buffer_free(
            &m_context,
            m_context.device.graphics_command_pool,
            &m_context.graphics_command_buffers[index]);
    }

    // Destroy framebuffers
    for (uint32_t index = 0; index < m_context.swapchain.image_count; index++) {
        vulkan_framebuffer_destroy(&m_context, &m_context.swapchain.framebuffers[index]);
    }

    m_context.main_renderpass.x = 0;
    m_context.main_renderpass.y = 0;
    m_context.main_renderpass.w = m_context.framebuffer_width;
    m_context.main_renderpass.h = m_context.framebuffer_height;

    regenerate_framebuffers(&m_context.swapchain, &m_context.main_renderpass);
    create_command_buffers();

    // Clear the recreating flag
    m_context.is_recreating_swapchain = false;

    return true;
}

// INTERNAL

VKAPI_ATTR VkBool32 VKAPI_CALL renderer_backend_vulkan_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT /*message_types*/,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* /*user_data*/
) {
    switch (message_severity) {
        default:
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: {
            log_error(callback_data->pMessage);
            break;
        }
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: {
            log_warn(callback_data->pMessage);
            break;
        }
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: {
            log_info(callback_data->pMessage);
            break;
        }
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: {
            log_debug(callback_data->pMessage);
            break;
        }
    }

    return VK_FALSE;
}
