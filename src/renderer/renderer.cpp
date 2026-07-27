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
#include "renderer/command_buffer.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <ctime>
#include <random>

const uint32_t PARTICLE_COUNT = 8192U;

struct RendererState {
    SDL_Window* window;
    VulkanContext context;
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
void renderer_create_shader_objects();
void renderer_destroy_shader_objects();
void renderer_recreate_swapchain();

bool renderer_init(SDL_Window* window) {
    state.window = window;
    state.context.allocator = nullptr;

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
        .pNext = nullptr,
        .flags = 0,
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
    if (!vulkan_pipeline_create_graphics(&state.context, &state.context.graphics_pipeline)) {
        return false;
    }
    if (!vulkan_pipeline_create_compute(&state.context, &state.context.compute_pipeline)) {
        return false;
    }

    // Create graphics command buffers
    VkCommandBufferAllocateInfo graphics_command_buffer_allocate_info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = state.context.device.graphics_command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = ARRAY_LENGTH(state.context.graphics_command_buffers)
    };
    VK_CHECK(vkAllocateCommandBuffers(
        state.context.device.logical_device,
        &graphics_command_buffer_allocate_info,
        state.context.graphics_command_buffers));

    // Create compute command buffers
    VkCommandBufferAllocateInfo compute_command_buffer_allocate_info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = state.context.device.graphics_command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = ARRAY_LENGTH(state.context.compute_command_buffers)
    };
    VK_CHECK(vkAllocateCommandBuffers(
        state.context.device.logical_device,
        &compute_command_buffer_allocate_info,
        state.context.compute_command_buffers));

    renderer_create_sync_objects();
    renderer_create_shader_objects();

    state.context.frame_index = 0;

    log_info("Renderer initialized successfully.");
    return true;
}

void renderer_quit() {
    vkDeviceWaitIdle(state.context.device.logical_device);

    renderer_destroy_shader_objects();
    renderer_destroy_sync_objects();
    vkFreeCommandBuffers(
        state.context.device.logical_device,
        state.context.device.graphics_command_pool,
        ARRAY_LENGTH(state.context.graphics_command_buffers),
        state.context.graphics_command_buffers);
    vulkan_pipeline_destroy(&state.context, &state.context.graphics_pipeline);
    vulkan_pipeline_destroy(&state.context, &state.context.compute_pipeline);
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
}

void renderer_draw_frame(double delta_time) {
    // Acquire next image
    VkResult acquire_result = vkAcquireNextImageKHR(
        state.context.device.logical_device,
        state.context.swapchain.handle,
        UINT64_MAX,
        VK_NULL_HANDLE,
        state.context.frame_fences[state.context.frame_index],
        &state.context.image_index);
    if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
        log_info("vkAcquireNextImageKHR - Swapchain is out of date. Recreating swapchain...");
        renderer_recreate_swapchain();
        return;
    } else if (acquire_result != VK_SUCCESS) {
        log_error("Error acquiring next image: %s.", vulkan_result_str(acquire_result));
        return;
    }

    // Wait for current frame fence
    VkResult fence_result = vkWaitForFences(
        state.context.device.logical_device,
        1, &state.context.frame_fences[state.context.frame_index], VK_TRUE, UINT64_MAX);
    if (fence_result != VK_SUCCESS) {
        log_error("Error waiting for fence: %s.", vulkan_result_str(fence_result));
        return;
    }

    // Reset current frame fence
    vkResetFences(state.context.device.logical_device, 1, &state.context.frame_fences[state.context.frame_index]);

    // Update timeline for this frame
    const uint64_t compute_wait_value = state.context.semaphore_timeline_value;
    const uint64_t compute_signal_value = compute_wait_value + 1;
    const uint64_t graphics_wait_value = compute_signal_value;
    const uint64_t graphics_signal_value = graphics_wait_value + 1;
    state.context.semaphore_timeline_value += 2;

    // COMPUTE
    {
        // Update uniform buffer
        VulkanUniformBufferObject ubo {};
        ubo.delta_time = delta_time;
        memcpy(state.context.uniform_buffer_data[state.context.frame_index], &ubo, sizeof(ubo));

        // Record compute command buffer
        VkCommandBuffer command_buffer = state.context.compute_command_buffers[state.context.frame_index];
        vkResetCommandBuffer(command_buffer, 0);
        VkCommandBufferBeginInfo begin_info {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = 0,
            .pInheritanceInfo = nullptr
        };
        vkBeginCommandBuffer(command_buffer, &begin_info);
        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, state.context.compute_pipeline.handle);
        vkCmdBindDescriptorSets(
            command_buffer,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            state.context.compute_pipeline.layout,
            0, 1, &state.context.compute_descriptor_sets[state.context.frame_index],
            0, nullptr);
        vkCmdDispatch(command_buffer, PARTICLE_COUNT / 256, 1, 1);
        vkEndCommandBuffer(command_buffer);

        // Submit compute work
        VkTimelineSemaphoreSubmitInfo compute_timeline_submit_info {
            .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreValueCount = 1,
            .pWaitSemaphoreValues = &compute_wait_value,
            .signalSemaphoreValueCount = 1,
            .pSignalSemaphoreValues = &compute_signal_value
        };
        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        VkSubmitInfo compute_submit_info {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = &compute_timeline_submit_info,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &state.context.semaphore,
            .pWaitDstStageMask = &wait_stage,
            .commandBufferCount = 1,
            .pCommandBuffers = &command_buffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &state.context.semaphore
        };
        VkResult submit_result =
            vkQueueSubmit(state.context.device.graphics_queue, 1, &compute_submit_info, nullptr);
        if (submit_result != VK_SUCCESS) {
            log_error("renderer_draw_frame() - Graphics vkQueueSubmit failed with result %s.", vulkan_result_str(submit_result));
            return;
        }
    }

    // GRAPHICS
    {
        // Record graphics command buffer
        VkCommandBuffer command_buffer = state.context.graphics_command_buffers[state.context.frame_index];
        vkResetCommandBuffer(command_buffer, 0);
        VkCommandBufferBeginInfo begin_info {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = 0,
            .pInheritanceInfo = nullptr
        };
        vkBeginCommandBuffer(command_buffer, &begin_info);

        // Transition swapchain image to COLOR_ATTACHMENT_OPTIMAL
        vulkan_image_transition_layout_ext({
            .command_buffer = command_buffer,
            .image = state.context.swapchain.images[state.context.image_index],
            .image_aspect = VK_IMAGE_ASPECT_COLOR_BIT,
            .base_mip_level = 0,
            .mip_levels = 1,
            .old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
            .new_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .src_access_mask = VK_ACCESS_2_NONE,
            .dst_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            .src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dst_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
        });

        // Begin rendering
        VkRenderingAttachmentInfo color_attachment_info {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext = nullptr,
            .imageView = state.context.swapchain.image_views[state.context.image_index],
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .resolveMode = VK_RESOLVE_MODE_NONE,
            .resolveImageView = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = {
                .color = {
                    .float32 = { 0.0f, 0.0f, 0.0f, 0.0f }
                },
            }
        };
        VkRenderingInfo rendering_info {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderArea = {
                .offset = {
                    .x = 0, .y = 0
                },
                .extent = state.context.swapchain.extent,
            },
            .layerCount = 1,
            .viewMask = 0,
            .colorAttachmentCount = 1,
            .pColorAttachments = &color_attachment_info,
            .pDepthAttachment = nullptr,
            .pStencilAttachment = nullptr
        };
        vkCmdBeginRendering(command_buffer, &rendering_info);

        // Bind pipeline
        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state.context.graphics_pipeline.handle);

        // Viewport
        VkViewport viewport {
            .x = 0,
            .y = 0,
            .width = (float)state.context.swapchain.extent.width,
            .height = (float)state.context.swapchain.extent.height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };
        vkCmdSetViewport(state.context.graphics_command_buffers[state.context.frame_index], 0, 1, &viewport);

        // Scissor
        VkRect2D scissor = {
            .offset = { .x = 0, .y = 0 },
            .extent = state.context.swapchain.extent
        };
        vkCmdSetScissor(state.context.graphics_command_buffers[state.context.frame_index], 0, 1, &scissor);

        // Draw particles
        VkDeviceSize vertex_buffer_offset = 0;
        vkCmdBindVertexBuffers(
            command_buffer,
            0, 1,
            &state.context.shader_storage_buffers[state.context.frame_index].handle,
            &vertex_buffer_offset);
        vkCmdDraw(command_buffer, PARTICLE_COUNT, 1, 0, 0);

        // End rendering
        vkCmdEndRendering(state.context.graphics_command_buffers[state.context.frame_index]);

        // Transition the swapchain image to PRESENT_SRC
        vulkan_image_transition_layout_ext({
            .command_buffer = command_buffer,
            .image = state.context.swapchain.images[state.context.image_index],
            .image_aspect = VK_IMAGE_ASPECT_COLOR_BIT,
            .base_mip_level = 0,
            .mip_levels = 1,
            .old_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .new_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .src_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            .dst_access_mask = VK_ACCESS_2_NONE,
            .src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dst_stage_mask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT
        });

        vkEndCommandBuffer(command_buffer);

        // Submit graphics work
        VkTimelineSemaphoreSubmitInfo graphics_timeline_submit_info {
            .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreValueCount = 1,
            .pWaitSemaphoreValues = &graphics_wait_value,
            .signalSemaphoreValueCount = 1,
            .pSignalSemaphoreValues = &graphics_signal_value
        };
        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        VkSubmitInfo graphics_submit_info {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = &graphics_timeline_submit_info,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &state.context.semaphore,
            .pWaitDstStageMask = &wait_stage,
            .commandBufferCount = 1,
            .pCommandBuffers = &command_buffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &state.context.semaphore
        };
        VkResult submit_result =
            vkQueueSubmit(state.context.device.graphics_queue, 1, &graphics_submit_info, nullptr);
        if (submit_result != VK_SUCCESS) {
            log_error("renderer_draw_frame() - Graphics vkQueueSubmit failed with result %s.", vulkan_result_str(submit_result));
            return;
        }

        // Wait before presenting
        // TODO - why do this instead of just chaining it into present?
        VkSemaphoreWaitInfo present_wait_info {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
            .pNext = nullptr,
            .flags = 0,
            .semaphoreCount = 1,
            .pSemaphores = &state.context.semaphore,
            .pValues = &graphics_signal_value
        };
        VkResult wait_result = vkWaitSemaphores(state.context.device.logical_device, &present_wait_info, UINT64_MAX);
        if (wait_result != VK_SUCCESS) {
            log_error("Failed to wait for semaphore. (Error code %s)", vulkan_result_str(wait_result));
            return;
        }
    }

    VkPresentInfoKHR present_info {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
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
        .pNext = nullptr,
        .flags = 0,
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
    state.context.semaphore_timeline_value = 0;
    VkSemaphoreTypeCreateInfo semaphore_type_create_info {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .pNext = nullptr,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
        .initialValue = state.context.semaphore_timeline_value
    };
    VkSemaphoreCreateInfo semaphore_create_info {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &semaphore_type_create_info,
        .flags = 0
    };
    VK_CHECK(vkCreateSemaphore(
        state.context.device.logical_device,
        &semaphore_create_info,
        state.context.allocator,
        &state.context.semaphore));

    // Frame fences
    for (uint32_t index = 0; index < ARRAY_LENGTH(state.context.frame_fences); index++) {
        VkFenceCreateInfo fence_create_info {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0
        };
        VK_CHECK(vkCreateFence(
            state.context.device.logical_device,
            &fence_create_info,
            state.context.allocator,
            &state.context.frame_fences[index]));
    }
    vkResetFences(state.context.device.logical_device,
        ARRAY_LENGTH(state.context.frame_fences), state.context.frame_fences);
}

void renderer_destroy_sync_objects() {
    vkDestroySemaphore(
        state.context.device.logical_device,
        state.context.semaphore,
        state.context.allocator);

    // Frame fences
    for (uint32_t index = 0; index < ARRAY_LENGTH(state.context.frame_fences); index++) {
        vkDestroyFence(
            state.context.device.logical_device,
            state.context.frame_fences[index],
            state.context.allocator);
    }
}

void renderer_create_shader_objects() {
    srand(time(NULL));

    // Init particles
    std::vector<VulkanParticle> particles;
    std::default_random_engine random_engine(time(NULL));
    std::uniform_real_distribution random_distribution(0.0f, 1.0f);

    particles.reserve(PARTICLE_COUNT);
    for (uint32_t index = 0; index < PARTICLE_COUNT; index++) {
        // float random_number = (float)(rand() % 1000) / 1000.0f;
        float r = 0.25f * sqrtf(random_distribution(random_engine));

        float theta = random_distribution(random_engine) * 2.0f * ZEN_PI;
        float x = r * cosf(theta) * (float)state.context.window_height / (float)state.context.window_width;
        float y = r * sinf(theta);

        float color_red = random_distribution(random_engine);
        float color_green = random_distribution(random_engine);
        float color_blue = random_distribution(random_engine);

        particles.push_back({
            .position = vec2(x, y),
            .velocity = vec2(x, y).normalized() * 0.25f,
            .color = vec4(color_red, color_green, color_blue, 1.0f)
        });
    }

    // Create a staging buffer to upload data to the GPU
    VulkanBuffer staging_buffer;
    vulkan_buffer_create(&state.context, {
        .size = PARTICLE_COUNT * sizeof(VulkanParticle),
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .memory_properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    }, &staging_buffer);
    vulkan_buffer_bind(&state.context, &staging_buffer, 0);
    vulkan_buffer_load_data(&state.context, &staging_buffer, {
        .offset = 0,
        .size = PARTICLE_COUNT * sizeof(VulkanParticle),
        .data = particles.data()
    });

    // Create the storage buffers
    for (uint32_t index = 0; index < VULKAN_MAX_FRAMES_IN_FLIGHT; index++) {
        vulkan_buffer_create(&state.context, {
            .size = PARTICLE_COUNT * sizeof(VulkanParticle),
            .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            .memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        }, &state.context.shader_storage_buffers[index]);
        vulkan_buffer_bind(&state.context, &state.context.shader_storage_buffers[index], 0);
    }

    // Copy from the staging buffers to the storage buffers
    VkCommandBuffer temp_command_buffer;
    vulkan_command_buffer_begin_single_use(&state.context, &temp_command_buffer);

    for (uint32_t index = 0; index < VULKAN_MAX_FRAMES_IN_FLIGHT; index++) {
        VkBufferCopy copy_region {
            .srcOffset = 0,
            .dstOffset = 0,
            .size = PARTICLE_COUNT * sizeof(VulkanParticle)
        };
        vkCmdCopyBuffer(
            temp_command_buffer,
            staging_buffer.handle,
            state.context.shader_storage_buffers[index].handle,
            1, &copy_region);
    }

    vulkan_command_buffer_end_single_use(&state.context, &temp_command_buffer);
    vulkan_buffer_destroy(&state.context, &staging_buffer);

    // Create uniform buffers
    for (uint32_t index = 0; index < VULKAN_MAX_FRAMES_IN_FLIGHT; index++) {
        vulkan_buffer_create(&state.context, {
            .size = sizeof(VulkanUniformBufferObject),
            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            .memory_properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        }, &state.context.uniform_buffers[index]);
        vulkan_buffer_bind(&state.context, &state.context.uniform_buffers[index], 0);
        state.context.uniform_buffer_data[index] = vulkan_buffer_map_memory(&state.context, &state.context.uniform_buffers[index], {
            .offset = 0,
            .size = sizeof(VulkanUniformBufferObject)
        });
    }

    // Create descriptor pool
    VkDescriptorPoolSize descriptor_pool_sizes[] = {
        {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = VULKAN_MAX_FRAMES_IN_FLIGHT
        },
        {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = VULKAN_MAX_FRAMES_IN_FLIGHT * 2
        },
    };
    VkDescriptorPoolCreateInfo descriptor_pool_create_info {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
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

    // Create compute descriptor sets
    VkDescriptorSetLayout layouts[VULKAN_MAX_FRAMES_IN_FLIGHT] = {
        state.context.compute_pipeline.descriptor_set_layout,
        state.context.compute_pipeline.descriptor_set_layout
    };
    VkDescriptorSetAllocateInfo descriptor_set_allocate_info {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = state.context.descriptor_pool,
        .descriptorSetCount = ARRAY_LENGTH(layouts),
        .pSetLayouts = layouts
    };
    VK_CHECK(vkAllocateDescriptorSets(
        state.context.device.logical_device, &descriptor_set_allocate_info, state.context.compute_descriptor_sets));

    // Write descriptor sets
    VkDescriptorBufferInfo descriptor_buffer_infos[VULKAN_MAX_FRAMES_IN_FLIGHT * 3];
    VkWriteDescriptorSet descriptor_writes[VULKAN_MAX_FRAMES_IN_FLIGHT * 3];
    uint32_t write_index = 0;
    for (uint32_t frame_index = 0; frame_index < VULKAN_MAX_FRAMES_IN_FLIGHT; frame_index++) {
        // Descriptor write for uniform buffer
        descriptor_buffer_infos[write_index] = {
            .buffer = state.context.uniform_buffers[frame_index].handle,
            .offset = 0,
            .range = sizeof(VulkanUniformBufferObject)
        };
        descriptor_writes[write_index] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = state.context.compute_descriptor_sets[frame_index],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pImageInfo = nullptr,
            .pBufferInfo = &descriptor_buffer_infos[write_index],
            .pTexelBufferView = nullptr
        };
        write_index++;

        // Descriptor write for previous storage buffer
        uint32_t previous_index = frame_index == 0
            ? 1
            : 0;
        descriptor_buffer_infos[write_index] = {
            .buffer = state.context.shader_storage_buffers[previous_index].handle,
            .offset = 0,
            .range = PARTICLE_COUNT * sizeof(VulkanParticle)
        };
        descriptor_writes[write_index] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = state.context.compute_descriptor_sets[frame_index],
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pImageInfo = nullptr,
            .pBufferInfo = &descriptor_buffer_infos[write_index],
            .pTexelBufferView = nullptr
        };
        write_index++;

        // Descriptor write for current storage buffer
        descriptor_buffer_infos[write_index] = {
            .buffer = state.context.shader_storage_buffers[frame_index].handle,
            .offset = 0,
            .range = PARTICLE_COUNT * sizeof(VulkanParticle)
        };
        descriptor_writes[write_index] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = state.context.compute_descriptor_sets[frame_index],
            .dstBinding = 2,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pImageInfo = nullptr,
            .pBufferInfo = &descriptor_buffer_infos[write_index],
            .pTexelBufferView = nullptr
        };
        write_index++;
    }
    vkUpdateDescriptorSets(
        state.context.device.logical_device,
        ARRAY_LENGTH(descriptor_writes), descriptor_writes, 0, nullptr);
}

void renderer_destroy_shader_objects() {
    for (uint32_t index = 0; index < VULKAN_MAX_FRAMES_IN_FLIGHT; index++) {
        vulkan_buffer_destroy(&state.context, &state.context.shader_storage_buffers[index]);
    }

    vkDestroyDescriptorPool(state.context.device.logical_device, state.context.descriptor_pool, state.context.allocator);

    for (uint32_t index = 0; index < VULKAN_MAX_FRAMES_IN_FLIGHT; index++) {
        vulkan_buffer_destroy(&state.context, &state.context.uniform_buffers[index]);
    }
}

void renderer_recreate_swapchain() {
    vkDeviceWaitIdle(state.context.device.logical_device);

    renderer_destroy_sync_objects();
    vulkan_swapchain_destroy(&state.context);
    vulkan_swapchain_create(&state.context);
    renderer_create_sync_objects();

    log_info("Swapchain recreated successfully.");
}
