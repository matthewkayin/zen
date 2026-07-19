#include "renderer.h"

#include "defines.h"
#include "core/logger.h"
#include "core/asserts.h"
#include "renderer/util.h"
#include "renderer/types.h"
#include "renderer/device.h"
#include "renderer/swapchain.h"
#include "renderer/pipeline.h"
#include "renderer/image.h"
#include "renderer/buffer.h"
#include "math/vertex3d.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <vector>

static const Vertex3d vertices[] = {
    { .position = vec3(-0.5f, -0.5f, 0.0f), .tex_coord = vec2(1.0f, 0.0f) },
    { .position = vec3(0.5f, -0.5f, 0.0f), .tex_coord = vec2(0.0f, 0.0f) },
    { .position = vec3(0.5f, 0.5f, 0.0f), .tex_coord = vec2(0.0f, 1.0f) },
    { .position = vec3(-0.5f, 0.5f, 0.0f), .tex_coord = vec2(1.0f, 1.0f) },

    { .position = vec3(-0.5f, -0.5f, -0.5f), .tex_coord = vec2(1.0f, 0.0f) },
    { .position = vec3(0.5f, -0.5f, -0.5f), .tex_coord = vec2(0.0f, 0.0f) },
    { .position = vec3(0.5f, 0.5f, -0.5f), .tex_coord = vec2(0.0f, 1.0f) },
    { .position = vec3(-0.5f, 0.5f, -0.5f), .tex_coord = vec2(1.0f, 1.0f) },
};
static const uint32_t indices[] = {
    0, 1, 2,
    2, 3, 0,

    4, 5, 6,
    6, 7, 4
};

struct RendererState {
    SDL_Window* window;
    VulkanContext context;
    VulkanBuffer vertex_buffer;
    VulkanBuffer index_buffer;
    VulkanImage texture;
    VkSampler texutre_sampler;
};
static RendererState state;

// Debug
bool renderer_get_debug_extension_names(
    std::vector<const char*>& extension_names,
    std::vector<const char*>& layer_names);
void renderer_create_debugger();

// Internal
void renderer_create_sync_objects();
void renderer_destroy_sync_objects();
void renderer_create_uniform_objects();
void renderer_destroy_uniform_objects();
void renderer_create_texture_sampler();
void renderer_destroy_texture_sampler();
void renderer_recreate_swapchain();

bool renderer_init(SDL_Window* window) {
    state.window = window;
    state.context.allocator = nullptr;

    VkApplicationInfo app_info {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
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
        log_error("Failed to get platform-specific Vulkan extensions: %s", SDL_GetError());
        return false;
    }
    for (uint32_t extension_index = 0; extension_index < instance_extension_count; extension_index++) {
        extension_names.push_back(instance_extensions[extension_index]);
    }

    // Get debug extensions
    if (!renderer_get_debug_extension_names(extension_names, layer_names)) {
        return false;
    }

    VkInstanceCreateInfo instance_create_info {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledLayerCount = (uint32_t)layer_names.size(),
        .ppEnabledLayerNames = layer_names.data(),
        .enabledExtensionCount = (uint32_t)extension_names.size(),
        .ppEnabledExtensionNames = extension_names.data()
    };

    // Create instance
    VkResult result = vkCreateInstance(&instance_create_info, state.context.allocator, &state.context.instance);
    if (result != VK_SUCCESS) {
        log_error("vkCreateInstance failed with result %s.", vulkan_result_str(result));
        return false;
    }

    // Create debugger
    renderer_create_debugger();

    // Create surface
    if (!SDL_Vulkan_CreateSurface(state.window, state.context.instance, state.context.allocator, &state.context.surface)) {
        log_error("Failed to create surface %s.", SDL_GetError());
        return false;
    }

    // Store surface size
    int window_width, window_height;
    SDL_GetWindowSize(state.window, &window_width, &window_height);
    state.context.window_width = (uint32_t)window_width;
    state.context.window_height = (uint32_t)window_height;

    if (!vulkan_device_create(&state.context)) {
        return false;
    }
    vulkan_swapchain_create(&state.context);
    if (!vulkan_pipeline_create(&state.context)) {
        return false;
    }

    // Create graphics command buffer
    VkCommandBufferAllocateInfo graphics_command_buffer_allocate_info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = state.context.device.graphics_command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = ARRAY_LENGTH(state.context.graphics_command_buffers)
    };
    VK_CHECK(vkAllocateCommandBuffers(
        state.context.device.logical_device,
        &graphics_command_buffer_allocate_info,
        state.context.graphics_command_buffers));

    renderer_create_sync_objects();

    // Create texture
    if (!vulkan_image_create_texture(&state.context, "../res/texture/texture.png", &state.texture)) {
        return false;
    }
    renderer_create_texture_sampler();
    renderer_create_uniform_objects();

    // Create vertex buffer
    vulkan_buffer_create(&state.context, {
        .size = sizeof(vertices),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    }, &state.vertex_buffer);
    vulkan_buffer_bind(&state.context, &state.vertex_buffer, 0);
    vulkan_buffer_upload_data(&state.context, &state.vertex_buffer, {
        .offset = 0,
        .size = sizeof(vertices),
        .data = (void*)vertices
    });

    // Create index buffer
    vulkan_buffer_create(&state.context, {
        .size = sizeof(indices),
        .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    }, &state.index_buffer);
    vulkan_buffer_bind(&state.context, &state.index_buffer, 0);
    vulkan_buffer_upload_data(&state.context, &state.index_buffer, {
        .offset = 0,
        .size = sizeof(indices),
        .data = (void*)indices
    });

    state.context.frame_index = 0;

    log_info("Renderer initialized successfully.");
    return true;
}

void renderer_quit() {
    vkDeviceWaitIdle(state.context.device.logical_device);

    renderer_destroy_texture_sampler();
    vulkan_image_destroy(&state.context, &state.texture);

    vulkan_buffer_destroy(&state.context, &state.vertex_buffer);
    vulkan_buffer_destroy(&state.context, &state.index_buffer);

    renderer_destroy_uniform_objects();
    renderer_destroy_sync_objects();
    vkFreeCommandBuffers(
        state.context.device.logical_device,
        state.context.device.graphics_command_pool,
        ARRAY_LENGTH(state.context.graphics_command_buffers),
        state.context.graphics_command_buffers);
    vulkan_pipeline_destroy(&state.context);
    vulkan_swapchain_destroy(&state.context);
    vulkan_device_destroy(&state.context);

    log_debug("Destroying vulkan surface...");
    SDL_Vulkan_DestroySurface(state.context.instance, state.context.surface, state.context.allocator);

    // Destroy debug messenger
    log_debug("Destroying debug messenger...");
    if (state.context.debug_messenger != VK_NULL_HANDLE) {
        PFN_vkDestroyDebugUtilsMessengerEXT destroyDebugUtilsMessenger =
            (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(state.context.instance, "vkDestroyDebugUtilsMessengerEXT");
        destroyDebugUtilsMessenger(state.context.instance, state.context.debug_messenger, state.context.allocator);
    }

    log_debug("Destroying vulkan instance...");
    vkDestroyInstance(state.context.instance, nullptr);

    log_info("Successfully quit renderer.");
}

void renderer_on_resized() {
    // Store surface size
    int window_width, window_height;
    SDL_GetWindowSize(state.window, &window_width, &window_height);
    state.context.window_width = (uint32_t)window_width;
    state.context.window_height = (uint32_t)window_height;

    renderer_recreate_swapchain();
}

void renderer_begin_frame() {
    // Wait for current frame fence
    VkResult fence_result = vkWaitForFences(
        state.context.device.logical_device,
        1, &state.context.frame_fences[state.context.frame_index], VK_TRUE, UINT64_MAX);
    if (fence_result != VK_SUCCESS) {
        log_error("Error waiting for fence: %s.", vulkan_result_str(fence_result));
        return;
    }

    // Acquire next image
    VkResult acquire_result = vkAcquireNextImageKHR(
        state.context.device.logical_device,
        state.context.swapchain.handle,
        UINT64_MAX,
        state.context.acquire_semaphores[state.context.frame_index],
        VK_NULL_HANDLE,
        &state.context.image_index);
    if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
        log_info("vkAcquireNextImageKHR - Swapchain is out of date. Recreating swapchain...");
        renderer_recreate_swapchain();
        return;
    } else if (acquire_result != VK_SUCCESS) {
        log_error("Error acquiring next image: %s.", vulkan_result_str(acquire_result));
        return;
    }

    // Reset current frame fence (only done after we have successfully acquired image to avoid deadlock)
    vkResetFences(state.context.device.logical_device, 1, &state.context.frame_fences[state.context.frame_index]);

    // Begin command buffer
    vkResetCommandBuffer(state.context.graphics_command_buffers[state.context.frame_index], 0);
    VkCommandBufferBeginInfo command_buffer_begin_info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = 0
    };
    vkBeginCommandBuffer(state.context.graphics_command_buffers[state.context.frame_index], &command_buffer_begin_info);

    // Transition swapchain image to color attachment optimal
    vulkan_image_transition_layout_ext({
        .command_buffer = state.context.graphics_command_buffers[state.context.frame_index],
        .image = state.context.swapchain.images[state.context.image_index],
        .image_aspect = VK_IMAGE_ASPECT_COLOR_BIT,
        .old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
        .new_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .src_access_mask = VK_ACCESS_2_NONE,
        .dst_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dst_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
    });

    // Transition depth attachment to depth attachment optimal
    vulkan_image_transition_layout_ext({
        .command_buffer = state.context.graphics_command_buffers[state.context.frame_index],
        .image = state.context.swapchain.depth_attachment.handle,
        .image_aspect = VK_IMAGE_ASPECT_DEPTH_BIT,
        .old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
        .new_layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .src_access_mask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dst_access_mask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .src_stage_mask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
        .dst_stage_mask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
    });

    VkRenderingAttachmentInfo rendering_attachment_info {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = state.context.swapchain.image_views[state.context.image_index],
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {
            .color = { .float32 = { 0.0f, 0.0f, 0.0f, 1.0f }}
        }
    };
    VkRenderingAttachmentInfo depth_attachment_info {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = state.context.swapchain.depth_attachment.view,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .clearValue = {
            .depthStencil = {
                .depth = 1.0f,
                .stencil = 0
            }
        }
    };

    VkRenderingInfo rendering_info {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {
            .offset = { .x = 0, .y = 0 },
            .extent = state.context.swapchain.extent
        },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &rendering_attachment_info,
        .pDepthAttachment = &depth_attachment_info
    };
    vkCmdBeginRendering(state.context.graphics_command_buffers[state.context.frame_index], &rendering_info);
    vkCmdBindPipeline(state.context.graphics_command_buffers[state.context.frame_index],
        VK_PIPELINE_BIND_POINT_GRAPHICS, state.context.graphics_pipeline.handle);

    VkViewport viewport {
        .x = 0,
        .y = 0,
        .width = (float)state.context.swapchain.extent.width,
        .height = (float)state.context.swapchain.extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    vkCmdSetViewport(state.context.graphics_command_buffers[state.context.frame_index], 0, 1, &viewport);

    VkRect2D scissor = {
        .offset = { .x = 0, .y = 0 },
        .extent = state.context.swapchain.extent
    };
    vkCmdSetScissor(state.context.graphics_command_buffers[state.context.frame_index], 0, 1, &scissor);
}

void renderer_end_frame() {
    vkCmdEndRendering(state.context.graphics_command_buffers[state.context.frame_index]);

    // Transition the swapchain image to PRESENT
    vulkan_image_transition_layout_ext({
        .command_buffer = state.context.graphics_command_buffers[state.context.frame_index],
        .image = state.context.swapchain.images[state.context.image_index],
        .image_aspect = VK_IMAGE_ASPECT_COLOR_BIT,
        .old_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .new_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .src_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .dst_access_mask = VK_ACCESS_2_NONE,
        .src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dst_stage_mask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT
    });

    vkEndCommandBuffer(state.context.graphics_command_buffers[state.context.frame_index]);

    VkPipelineStageFlags pipeline_stage_flags[] = {
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
    };
    VkSubmitInfo submit_info {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &state.context.acquire_semaphores[state.context.frame_index],
        .pWaitDstStageMask = pipeline_stage_flags,
        .commandBufferCount = 1,
        .pCommandBuffers = &state.context.graphics_command_buffers[state.context.frame_index],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &state.context.submit_semaphores[state.context.image_index]
    };
    VkResult submit_result = vkQueueSubmit(state.context.device.graphics_queue,
        1, &submit_info, state.context.frame_fences[state.context.frame_index]);
    if (submit_result != VK_SUCCESS) {
        log_error("renderer_draw_frame() - vkQueueSubmit failed with result %s.", vulkan_result_str(submit_result));
        return;
    }

    VkPresentInfoKHR present_info {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &state.context.submit_semaphores[state.context.image_index],
        .swapchainCount = 1,
        .pSwapchains = &state.context.swapchain.handle,
        .pImageIndices = &state.context.image_index,
        .pResults = nullptr
    };
    VkResult present_result = vkQueuePresentKHR(state.context.device.present_queue, &present_info);
    if (present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR) {
        log_info("vkQueuePresentKHR returned result %s. Recreating swapchain...", vulkan_result_str(present_result));
        renderer_recreate_swapchain();
    } else if (present_result != VK_SUCCESS) {
        log_error("Failed to present swapchain image: %s.", vulkan_result_str(present_result));
    }

    state.context.frame_index = (state.context.frame_index + 1) % VULKAN_MAX_FRAMES_IN_FLIGHT;
}

void renderer_draw_frame(double delta) {
    renderer_begin_frame();

    VulkanUniformBufferObject ubo {
        .model = mat4::identity(),
        .view = mat4::look_at(vec3(2.0f, 2.0f, 2.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 1.0f)),
        .projection = mat4::perspective(
            45.0f * ZEN_DEG_TO_RAD,
            (float)state.context.window_width / (float)state.context.window_height,
            0.1f, 1000.0f)
    };
    vulkan_buffer_load_data(&state.context, &state.context.uniform_buffer, {
        .offset = state.context.frame_index * sizeof(VulkanUniformBufferObject),
        .size = sizeof(VulkanUniformBufferObject),
        .data = &ubo
    });

    VkDeviceSize offsets = 0;
    vkCmdBindVertexBuffers(
        state.context.graphics_command_buffers[state.context.frame_index],
        0, 1, &state.vertex_buffer.handle, &offsets);
    vkCmdBindIndexBuffer(
        state.context.graphics_command_buffers[state.context.frame_index],
        state.index_buffer.handle, 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindDescriptorSets(
        state.context.graphics_command_buffers[state.context.frame_index],
        VK_PIPELINE_BIND_POINT_GRAPHICS, state.context.graphics_pipeline.layout,
        0, 1, &state.context.descriptor_sets[state.context.frame_index], 0, nullptr);

    vkCmdDrawIndexed(state.context.graphics_command_buffers[state.context.frame_index], ARRAY_LENGTH(indices), 1, 0, 0, 0);

    renderer_end_frame();
}


// DEBUG

#ifdef ZEN_DEBUG

bool renderer_get_debug_extension_names(
    std::vector<const char*>& extension_names,
    std::vector<const char*>& layer_names
) {
    extension_names.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    // Print list of extensions
    log_debug("Required Vulkan extensions:");
    for (uint32_t index = 0; index < extension_names.size(); index++) {
        log_debug("%s", extension_names[index]);
    }

    // Debug layers
    layer_names.push_back("VK_LAYER_KHRONOS_validation");

    // Get a list of available validation layers
    uint32_t available_layer_count;
    VK_CHECK(vkEnumerateInstanceLayerProperties(&available_layer_count, nullptr));
    std::vector<VkLayerProperties> available_layers(available_layer_count);
    VK_CHECK(vkEnumerateInstanceLayerProperties(&available_layer_count, available_layers.data()));

    // Verify that all required layers are available
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

    return true;
}

VKAPI_ATTR VkBool32 VKAPI_CALL renderer_debug_callback(
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

void renderer_create_debugger() {
    VkDebugUtilsMessengerCreateInfoEXT debug_create_info {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
        .messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
        .pfnUserCallback = renderer_debug_callback,
        .pUserData = nullptr
    };

    PFN_vkCreateDebugUtilsMessengerEXT createDebugUtilsMessenger =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(state.context.instance, "vkCreateDebugUtilsMessengerEXT");
    ZEN_ASSERT_MESSAGE(createDebugUtilsMessenger, "Failed to load createDebugUtilsMessenger function pointer.");

    VK_CHECK(createDebugUtilsMessenger(
        state.context.instance, &debug_create_info, state.context.allocator, &state.context.debug_messenger));

    log_info("Vulkan debugger created.");
}

#else

bool renderer_get_debug_extension_names(std::vector<const char*>&, std::vector<const char*>&) {
    return true;
}

void renderer_create_debugger() {
    state.debug_messenger = VK_NULL_HANDLE;
}

#endif

// INTERNAL

void renderer_create_sync_objects() {
    // Acquire semaphores
    for (uint32_t index = 0; index < ARRAY_LENGTH(state.context.acquire_semaphores); index++) {
        VkSemaphoreCreateInfo semaphore_create_info {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
        };
        VK_CHECK(vkCreateSemaphore(
            state.context.device.logical_device,
            &semaphore_create_info,
            state.context.allocator,
            &state.context.acquire_semaphores[index]));
    }

    // Submit semaphores
    state.context.submit_semaphores = std::vector<VkSemaphore>(state.context.swapchain.images.size());
    for (uint32_t index = 0; index < (uint32_t)state.context.swapchain.images.size(); index++) {
        VkSemaphoreCreateInfo semaphore_create_info {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
        };
        VK_CHECK(vkCreateSemaphore(
            state.context.device.logical_device,
            &semaphore_create_info,
            state.context.allocator,
            &state.context.submit_semaphores[index]));
    }

    // Frame fences
    for (uint32_t index = 0; index < ARRAY_LENGTH(state.context.frame_fences); index++) {
        VkFenceCreateInfo fence_create_info {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };
        VK_CHECK(vkCreateFence(
            state.context.device.logical_device,
            &fence_create_info,
            state.context.allocator,
            &state.context.frame_fences[index]));
    }
}

void renderer_destroy_sync_objects() {
    // Acquire semaphores
    for (uint32_t index = 0; index < ARRAY_LENGTH(state.context.acquire_semaphores); index++) {
        vkDestroySemaphore(
            state.context.device.logical_device,
            state.context.acquire_semaphores[index],
            state.context.allocator);
    }

    // Submit semaphores
    for (uint32_t index = 0; index < (uint32_t)state.context.submit_semaphores.size(); index++) {
        vkDestroySemaphore(
            state.context.device.logical_device,
            state.context.submit_semaphores[index],
            state.context.allocator);
    }

    // Frame fences
    for (uint32_t index = 0; index < ARRAY_LENGTH(state.context.frame_fences); index++) {
        vkDestroyFence(
            state.context.device.logical_device,
            state.context.frame_fences[index],
            state.context.allocator);
    }
}

void renderer_create_uniform_objects() {
    // Create uniform buffers
    vulkan_buffer_create(&state.context, {
        .size = VULKAN_MAX_FRAMES_IN_FLIGHT * sizeof(VulkanUniformBufferObject),
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .memory_properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    }, &state.context.uniform_buffer);
    vulkan_buffer_bind(&state.context, &state.context.uniform_buffer, 0);

    // Create descriptor pool
    VkDescriptorPoolSize descriptor_pool_sizes[] = {
        {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = VULKAN_MAX_FRAMES_IN_FLIGHT
        },
        {
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = VULKAN_MAX_FRAMES_IN_FLIGHT
        },
    };
    VkDescriptorPoolCreateInfo descriptor_pool_create_info {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = 0,
        .maxSets = VULKAN_MAX_FRAMES_IN_FLIGHT,
        .poolSizeCount = ARRAY_LENGTH(descriptor_pool_sizes),
        .pPoolSizes = descriptor_pool_sizes
    };
    VK_CHECK(vkCreateDescriptorPool(
        state.context.device.logical_device,
        &descriptor_pool_create_info,
        state.context.allocator,
        &state.context.descriptor_pool));

    // Create descriptor sets
    VkDescriptorSetLayout layouts[VULKAN_MAX_FRAMES_IN_FLIGHT] = {
        state.context.graphics_pipeline.descriptor_set_layout,
        state.context.graphics_pipeline.descriptor_set_layout
    };
    VkDescriptorSetAllocateInfo descriptor_set_allocate_info {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = state.context.descriptor_pool,
        .descriptorSetCount = ARRAY_LENGTH(layouts),
        .pSetLayouts = layouts
    };
    VK_CHECK(vkAllocateDescriptorSets(
        state.context.device.logical_device, &descriptor_set_allocate_info, state.context.descriptor_sets));

    // Write descriptor sets
    VkDescriptorBufferInfo descriptor_buffer_infos[VULKAN_MAX_FRAMES_IN_FLIGHT];
    VkDescriptorImageInfo descriptor_image_infos[VULKAN_MAX_FRAMES_IN_FLIGHT];
    VkWriteDescriptorSet descriptor_writes[VULKAN_MAX_FRAMES_IN_FLIGHT * 2];
    for (uint32_t index = 0; index < VULKAN_MAX_FRAMES_IN_FLIGHT; index++) {
        descriptor_buffer_infos[index] = {
            .buffer = state.context.uniform_buffer.handle,
            .offset = index * sizeof(VulkanUniformBufferObject),
            .range = sizeof(VulkanUniformBufferObject)
        };
        descriptor_writes[index] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = state.context.descriptor_sets[index],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &descriptor_buffer_infos[index]
        };

        descriptor_image_infos[index] = {
            .sampler = state.texutre_sampler,
            .imageView = state.texture.view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        descriptor_writes[VULKAN_MAX_FRAMES_IN_FLIGHT + index] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = state.context.descriptor_sets[index],
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &descriptor_image_infos[index]
        };
    }
    vkUpdateDescriptorSets(
        state.context.device.logical_device,
        ARRAY_LENGTH(descriptor_writes), descriptor_writes, 0, nullptr);
}

void renderer_destroy_uniform_objects() {
    vkDestroyDescriptorPool(state.context.device.logical_device, state.context.descriptor_pool, state.context.allocator);
    vulkan_buffer_destroy(&state.context, &state.context.uniform_buffer);
}

void renderer_create_texture_sampler() {
    // Create texture sample
    VkSamplerCreateInfo sampler_create_info {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_TRUE,
        .maxAnisotropy = state.context.device.properties.limits.maxSamplerAnisotropy,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };
    VK_CHECK(vkCreateSampler(
        state.context.device.logical_device,
        &sampler_create_info,
        state.context.allocator,
        &state.texutre_sampler));
}

void renderer_destroy_texture_sampler() {
    vkDestroySampler(
        state.context.device.logical_device, state.texutre_sampler, state.context.allocator);
}

void renderer_recreate_swapchain() {
    vkDeviceWaitIdle(state.context.device.logical_device);

    renderer_destroy_sync_objects();
    vulkan_swapchain_destroy(&state.context);
    vulkan_swapchain_create(&state.context);
    renderer_create_sync_objects();

    log_info("Swapchain recreated successfully.");
}
