#include "backend.h"

#include "defines.h"
#include "core/logger.h"
#include "renderer/vulkan/types.h"
#include "renderer/vulkan/utils.h"
#include "renderer/vulkan/device.h"
#include "renderer/vulkan/swapchain.h"
#include "renderer/vulkan/renderpass.h"
#include "renderer/vulkan/framebuffer.h"
#include "renderer/vulkan/command_buffer.h"
#include "renderer/vulkan/buffer.h"
#include "renderer/vulkan/shaders/object.h"
#include "math/vertex3d.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vector>

VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_backend_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_types,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data);

bool VulkanBackend::init() {
    // If you want a custom allocator, fill it in here
    m_context.allocator = nullptr;

    // Get framebuffer width and height
    int window_width, window_height;
    SDL_GetWindowSize(m_window, &window_width, &window_height);
    m_context.framebuffer_width = (uint32_t)window_width;
    m_context.framebuffer_height = (uint32_t)window_height;
    m_context.framebuffer_size_generation = 0;
    m_context.framebuffer_size_last_generation = 0;

    // Vulkan application info
    VkApplicationInfo app_info {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = ZEN_APP_NAME,
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "Zen Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_4
    };

    // Extension and layer names
    std::vector<const char*> extension_names;
    std::vector<const char*> layer_names;

    // Get platform extensions from SDL
    uint32_t instance_extension_count;
    const char* const* instance_extensions = SDL_Vulkan_GetInstanceExtensions(&instance_extension_count);
    if (instance_extensions == nullptr) {
        log_error("Failed to get platform-specific Vulkan extensions: %s",
                  SDL_GetError());
        return false;
    }
    for (uint32_t instance_extension_index = 0;
         instance_extension_index < instance_extension_count;
         instance_extension_index++
    ) {
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
    VK_CHECK(vkEnumerateInstanceLayerProperties(&available_layer_count, nullptr));
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

    // Instance create info
    VkInstanceCreateInfo instance_create_info {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pApplicationInfo = &app_info,
        .enabledLayerCount = (uint32_t)layer_names.size(),
        .ppEnabledLayerNames = layer_names.data(),
        .enabledExtensionCount = (uint32_t)extension_names.size(),
        .ppEnabledExtensionNames = extension_names.data()
    };

    // Create instance
    VkResult result = vkCreateInstance(&instance_create_info, m_context.allocator, &m_context.instance);
    if (result != VK_SUCCESS) {
        log_error("vkCreateInstance failed with result %s.", vulkan_result_str(result));
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
    debug_create_info.pfnUserCallback = vulkan_backend_debug_callback;
    debug_create_info.pUserData = nullptr;

    PFN_vkCreateDebugUtilsMessengerEXT createDebugUtilsMessenger =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            m_context.instance, "vkCreateDebugUtilsMessengerEXT");
    ZEN_ASSERT_MESSAGE(createDebugUtilsMessenger, "Failed to load createDebugUtilsMessenger function pointer.");
    VK_CHECK(createDebugUtilsMessenger(
        m_context.instance, &debug_create_info, m_context.allocator, &m_context.debug_messenger));
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
    if (!vulkan_swapchain_create(&m_context, m_context.framebuffer_width, m_context.framebuffer_height,
        &m_context.swapchain)
    ) {
        return false;
    }

    // Init renderpass
    vulkan_renderpass_create(&m_context,
        // Size
        0.0f, 0.0f, m_context.framebuffer_width, m_context.framebuffer_height,
        // Color
        0.0f, 0.0f, 0.2f, 1.0f,
        // Depth / Stencil
        1.0f, 0,
        &m_context.main_renderpass);

    // Create built-in shaders
    if (!vulkan_object_shader_create(&m_context, &m_context.object_shader)) {
        log_error("Error loading built-in object shader.");
        return false;
    }

    // Swapchain dependent resources (happens after shaders because it includes shader descriptor sets?)
    if (!create_swapchain_dependent_resources()) {
        return false;
    }

    // BUFFERS

    // Create object vertex buffer
    VulkanBufferCreateParams vertex_buffer_create_params {
        .size = 1024 * 1024 * sizeof(Vertex3d),
        .usage =
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .memory_property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .bind_on_create = true
    };
    if (!vulkan_buffer_create(&m_context, &vertex_buffer_create_params, &m_context.object_vertex_buffer)) {
        log_error("Error creating object vertex buffer.");
        return false;
    }

    // Create object index buffer
    VulkanBufferCreateParams index_buffer_create_params {
        .size = 1024 * 1024 * sizeof(uint32_t),
        .usage =
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .memory_property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .bind_on_create = true
    };
    if (!vulkan_buffer_create(&m_context, &index_buffer_create_params, &m_context.object_index_buffer)) {
        log_error("Error creating object index buffer.");
        return false;
    }

    m_context.geometry_vertex_offset = 0;
    m_context.geometry_index_offset = 0;

    // Test code - square
    const float square_extent = 5.0f;
    const Vertex3d vertices[] = {
        { .position = vec3(-square_extent, -square_extent, 0.0f) },
        { .position = vec3(square_extent, square_extent, 0.0f) },
        { .position = vec3(-square_extent, square_extent, 0.0f) },
        { .position = vec3(square_extent, -square_extent, 0.0f) }
    };
    const uint32_t indices[] = { 0, 1, 2, 0, 3, 1 };

    vulkan_buffer_upload_data(
        &m_context, m_context.device.graphics_command_pool, m_context.device.graphics_queue,
        &m_context.object_vertex_buffer, 0, sizeof(vertices), (void*)vertices);
    vulkan_buffer_upload_data(
        &m_context, m_context.device.graphics_command_pool, m_context.device.graphics_queue,
        &m_context.object_index_buffer, 0, sizeof(indices), (void*)indices);
    // End test code

    log_info("Vulkan backend initialized successfully.");
    return true;
}

void VulkanBackend::quit() {
    vkDeviceWaitIdle(m_context.device.logical_device);

    // Destroy buffers
    vulkan_buffer_destroy(&m_context, &m_context.object_vertex_buffer);
    vulkan_buffer_destroy(&m_context, &m_context.object_index_buffer);

    // Destroy swapchain dependent resources
    destroy_swapchain_dependent_resources();

    // Destroy built-in shaders
    vulkan_object_shader_destroy(&m_context, &m_context.object_shader);

    // Destroy renderpass
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
            (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                m_context.instance, "vkDestroyDebugUtilsMessengerEXT");
        destroyDebugUtilsMessenger(m_context.instance, m_context.debug_messenger, nullptr);
    }
#endif

    log_info("Destroying Vulkan instance...");
    vkDestroyInstance(m_context.instance, nullptr);
}

void VulkanBackend::on_resized() {
    m_context.framebuffer_size_generation++;
}

bool VulkanBackend::begin_frame(double delta_time) {
    // If recreating swapchain, do nothing
    if (m_context.is_recreating_swapchain) {
        VkResult result = vkDeviceWaitIdle(m_context.device.logical_device);
        if (vulkan_result_is_error(result)) {
            log_error("VulkanBackend::begin_frame - vkDeviceWaitIdle (1) failed: %s", vulkan_result_str(result));
            return false;
        }

        log_info("VulkanBackend::begin_frame - Doing nothing because we are recreating swapchain.");
        return false;
    }

    // Check if the window has been resized if so, recreate swapchain
    if (m_context.framebuffer_size_generation != m_context.framebuffer_size_last_generation) {
        VkResult result = vkDeviceWaitIdle(m_context.device.logical_device);
        if (vulkan_result_is_error(result)) {
            log_error("VulkanBackend::begin_frame - vkDeviceWaitIdle (2) failed: %s", vulkan_result_str(result));
            return false;
        }

        if (!recreate_swapchain()) {
            return false;
        }

        log_info("VulkanBackend::begin_frame - Resize successful.");
        return false;
    }

    // Wait for the execution of the current fence to complete
    VkResult fence_wait_result = vkWaitForFences(
        m_context.device.logical_device,
        1, &m_context.frame_fences[m_context.frame_index],
        VK_TRUE, UINT64_MAX);
    if (fence_wait_result != VK_SUCCESS) {
        log_warn("VulkanBackend::begin_frame - Fence wait failed with code %s.", vulkan_result_str(fence_wait_result));
        return false;
    }

    // Reset fence for current frame
    VK_CHECK(vkResetFences(
        m_context.device.logical_device,
        1, &m_context.frame_fences[m_context.frame_index]));

    // Acquire next image from the swapchain
    if (!vulkan_swapchain_acquire_next_image_index(
        &m_context, &m_context.swapchain, UINT64_MAX,
        m_context.acquire_semaphores[m_context.frame_index],
        nullptr, &m_context.image_index)
    ) {
        return false;
    }

    // Begin recording commands
    VulkanCommandBuffer* command_buffer = &m_context.graphics_command_buffers[m_context.image_index];
    vulkan_command_buffer_reset(command_buffer);
    vulkan_command_buffer_begin_recording(command_buffer, 0);

    // Dynamic state
    VkViewport viewport {
        .x = 0.0f,
        .y = (float)m_context.framebuffer_height,
        .width = (float)m_context.framebuffer_width,
        .height = -(float)m_context.framebuffer_height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    vkCmdSetViewport(command_buffer->handle, 0, 1, &viewport);

    VkRect2D scissor {
        .offset = {
            .x = 0,
            .y = 0
        },
        .extent = {
            .width = m_context.framebuffer_width,
            .height = m_context.framebuffer_height
        }
    };
    vkCmdSetScissor(command_buffer->handle, 0, 1, &scissor);

    m_context.main_renderpass.w = (float)m_context.framebuffer_width;
    m_context.main_renderpass.h = (float)m_context.framebuffer_height;

    // Begin renderpass
    vulkan_command_buffer_begin_renderpass(
        command_buffer,
        &m_context.main_renderpass,
        m_context.framebuffers[m_context.image_index].handle);

    return true;
}

bool VulkanBackend::end_frame(double delta_time) {
    VulkanCommandBuffer* command_buffer = &m_context.graphics_command_buffers[m_context.image_index];

    vulkan_command_buffer_end_renderpass(command_buffer);
    vulkan_command_buffer_end_recording(command_buffer);

    VkPipelineStageFlags pipeline_stage_flags[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };

    VkSubmitInfo submit_info {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &m_context.acquire_semaphores[m_context.frame_index],
        .pWaitDstStageMask = pipeline_stage_flags,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer->handle,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &m_context.submit_semaphores[m_context.image_index]
    };

    VkResult submit_result = vkQueueSubmit(
        m_context.device.graphics_queue,
        1, &submit_info,
        m_context.frame_fences[m_context.frame_index]);
    if (submit_result != VK_SUCCESS) {
        log_error("VulkanBackned::end_frame - vkQueueSubmit failed with result %s.", vulkan_result_str(submit_result));
        return false;
    }

    vulkan_command_buffer_set_submitted(command_buffer);

    vulkan_swapchain_present(
        &m_context, &m_context.swapchain,
        m_context.device.present_queue,
        m_context.submit_semaphores[m_context.image_index],
        m_context.image_index);

    return true;
}

void VulkanBackend::update_global_state(GlobalUniformObject global_ubo) {
    vulkan_object_shader_use(&m_context, &m_context.object_shader);
    m_context.object_shader.global_ubo = global_ubo;
    vulkan_object_shader_update_global_state(&m_context, &m_context.object_shader);
}

void VulkanBackend::update_object(mat4 model) {
    VulkanCommandBuffer* command_buffer = &m_context.graphics_command_buffers[m_context.image_index];

    vulkan_object_shader_update_object(&m_context, &m_context.object_shader, model);

    // Test code - draw triangle
    vulkan_object_shader_use(&m_context, &m_context.object_shader);
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(command_buffer->handle, 0, 1, &m_context.object_vertex_buffer.handle, offsets);
    vkCmdBindIndexBuffer(command_buffer->handle, m_context.object_index_buffer.handle, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(command_buffer->handle, 6, 1, 0, 0, 0);
    // End test code
}

// PRIVATE

bool VulkanBackend::recreate_swapchain() {
    // If already recreating, don't try again
    if (m_context.is_recreating_swapchain) {
        log_warn("VulkanBackend::recreate_swapchain - called when already recreating swapchain.");
        return false;
    }

    // If window is too small, don't create
    if (m_context.framebuffer_width == 0 || m_context.framebuffer_height == 0) {
        log_warn("VulkanBackend::recreate_swapchain - framebuffer dimensions are too small.");
        return false;
    }

    // Mark as recreating
    m_context.is_recreating_swapchain = true;
    log_debug("VulkanBackend::recreate_swapchain - Begin recreating swapchain.");

    // Wait on device
    vkDeviceWaitIdle(m_context.device.logical_device);

    // Destroy dependent resources
    destroy_swapchain_dependent_resources();

    // Get updated framebuffer size
    int window_width, window_height;
    SDL_GetWindowSize(m_window, &window_width, &window_height);
    m_context.framebuffer_width = (uint32_t)window_width;
    m_context.framebuffer_height = (uint32_t)window_height;

    // Update framebuffer size generation
    m_context.framebuffer_size_last_generation = m_context.framebuffer_size_generation;

    // Update renderpass dimensions
    m_context.main_renderpass.x = 0.0f;
    m_context.main_renderpass.y = 0.0f;
    m_context.main_renderpass.w = (float)window_width;
    m_context.main_renderpass.h = (float)window_height;

    if (!vulkan_swapchain_recreate(&m_context,
        m_context.framebuffer_width, m_context.framebuffer_height,
        &m_context.swapchain
    )) {
        return false;
    }

    create_swapchain_dependent_resources();

    // Clear the recreating flag
    m_context.is_recreating_swapchain = false;
    log_debug("VulkanBackend::recreate_swapchain - success.");

    return true;
}

bool VulkanBackend::create_swapchain_dependent_resources() {
    // FRAMEBUFFERS

    // Alloc framebuffers array
    m_context.framebuffers = (VulkanFramebuffer*)malloc(m_context.swapchain.image_count * sizeof(VulkanFramebuffer));
    if (!m_context.framebuffers) {
        log_error("Failed to alloc framebuffers array");
        return false;
    }

    // Init new framebuffers
    for (uint32_t index = 0;
        index < m_context.swapchain.image_count;
        index++
    ) {
        VkImageView attachments[] = {
            m_context.swapchain.views[index],
            m_context.swapchain.depth_attachment.view
        };

        if (!vulkan_framebuffer_create(
            &m_context,
            &m_context.main_renderpass,
            m_context.framebuffer_width,
            m_context.framebuffer_height,
            ARRAY_LENGTH(attachments),
            attachments,
            &m_context.framebuffers[index])
        ) {
            log_error("Failed to create framebuffer.");
            return false;
        }
    }

    // COMMAND BUFFERS

    // Alloc command buffers array
    m_context.graphics_command_buffers = (VulkanCommandBuffer*)malloc(m_context.swapchain.image_count * sizeof(VulkanCommandBuffer));
    if (!m_context.graphics_command_buffers) {
        log_error("Failed to alloc graphics command buffers array.");
        return false;
    }

    // Alloc command buffers
    for (uint32_t index = 0; index < m_context.swapchain.image_count; index++) {
        vulkan_command_buffer_allocate(
            &m_context,
            m_context.device.graphics_command_pool,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            &m_context.graphics_command_buffers[index]);
    }

    // ACQUIRE SEMAPHORES

    // Alloc acquire semaphores array
    m_context.acquire_semaphores = (VkSemaphore*)malloc(m_context.swapchain.max_frames_in_flight * sizeof(VkSemaphore));
    if (!m_context.acquire_semaphores) {
        log_error("Failed to alloc aquire semaphores array.");
        return false;
    }

    // Create acquire semaphores
    for (uint32_t index = 0; index < m_context.swapchain.max_frames_in_flight; index++) {
        VkSemaphoreCreateInfo create_info {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
        };

        VK_CHECK(vkCreateSemaphore(
            m_context.device.logical_device,
            &create_info,
            m_context.allocator,
            &m_context.acquire_semaphores[index]));
    }

    // SUBMIT SEMAPHORES

    // Alloc submit semaphores array
    m_context.submit_semaphores = (VkSemaphore*)malloc(m_context.swapchain.image_count * sizeof(VkSemaphore));
    if (!m_context.submit_semaphores) {
        log_error("Failed to alloc submit semaphores array.");
        return false;
    }

    // Create submit semaphores
    for (uint32_t index = 0; index < m_context.swapchain.image_count; index++) {
        VkSemaphoreCreateInfo create_info {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
        };

        VK_CHECK(vkCreateSemaphore(
            m_context.device.logical_device,
            &create_info,
            m_context.allocator,
            &m_context.submit_semaphores[index]));
    }

    // FRAME FENCES

    // Alloc frame fences array
    m_context.frame_fences = (VkFence*)malloc(m_context.swapchain.max_frames_in_flight * sizeof(VkFence));
    if (!m_context.frame_fences) {
        log_error("Failed to alloc frame fences array.");
        return false;
    }

    // Create frame fences
    for (uint32_t index = 0; index < m_context.swapchain.max_frames_in_flight; index++) {
        // Fence is created in signaled state so that application does not get stuck waiting
        // forever for the 0th frame to "finish"
        VkFenceCreateInfo create_info {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };

        VK_CHECK(vkCreateFence(
            m_context.device.logical_device,
            &create_info,
            m_context.allocator,
            &m_context.frame_fences[index]));
    }

    // OBJECT SHADER DESCRIPTORS
    if (!vulkan_object_shader_alloc_descriptor_sets(&m_context, &m_context.object_shader)) {
        return false;
    }

    return true;
}

void VulkanBackend::destroy_swapchain_dependent_resources() {
    // FRAMEBUFFERS

    if (m_context.framebuffers) {
        for (uint32_t framebuffer_index = 0;
            framebuffer_index < m_context.swapchain.image_count;
            framebuffer_index++
        ) {
            vulkan_framebuffer_destroy(&m_context, &m_context.framebuffers[framebuffer_index]);
        }

        free(m_context.framebuffers);
        m_context.framebuffers = nullptr;
    }

    // COMMAND BUFFERS

    if (m_context.graphics_command_buffers) {
        for (uint32_t index = 0; index < m_context.swapchain.image_count; index++) {
            vulkan_command_buffer_free(
                &m_context,
                m_context.device.graphics_command_pool,
                &m_context.graphics_command_buffers[index]);
        }

        free(m_context.graphics_command_buffers);
        m_context.graphics_command_buffers = nullptr;
    }

    // ACQUIRE SEMAPHORES

    if (m_context.acquire_semaphores) {
        for (uint32_t index = 0; index < m_context.swapchain.max_frames_in_flight; index++) {
            vkDestroySemaphore(
                m_context.device.logical_device,
                m_context.acquire_semaphores[index],
                m_context.allocator);
        }

        free(m_context.acquire_semaphores);
        m_context.acquire_semaphores = nullptr;
    }

    // SUBMIT SEMAPHORES

    if (m_context.submit_semaphores) {
        for (uint32_t index = 0; index < m_context.swapchain.image_count; index++) {
            vkDestroySemaphore(
                m_context.device.logical_device,
                m_context.submit_semaphores[index],
                m_context.allocator);
        }

        free(m_context.submit_semaphores);
        m_context.submit_semaphores = nullptr;
    }

    // FRAME FENCES

    if (m_context.frame_fences) {
        for (uint32_t index = 0; index < m_context.swapchain.max_frames_in_flight; index++) {
            vkDestroyFence(
                m_context.device.logical_device,
                m_context.frame_fences[index],
                m_context.allocator);
        }

        free(m_context.frame_fences);
        m_context.frame_fences = nullptr;
    }

    // OBJECT SHADER DESCRIPTORS

    vulkan_object_shader_free_descriptor_sets(&m_context, &m_context.object_shader);
}

// INTERNAL

VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_backend_debug_callback(
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
