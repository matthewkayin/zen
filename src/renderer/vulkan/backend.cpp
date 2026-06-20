#include "backend.h"

#include "defines.h"
#include "core/logger.h"
#include "renderer/vulkan/utils.h"
#include "renderer/vulkan/device.h"
#include "renderer/vulkan/swapchain.h"
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

    return true;
}

void VulkanBackend::quit() {
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

void VulkanBackend::on_resized(uint32_t width, uint32_t height) {}

bool VulkanBackend::begin_frame(double delta_time) { return true; }

bool VulkanBackend::end_frame(double delta_time) { return true; }

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
