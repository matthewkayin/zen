#include "backend.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <vector>

#include "core/logger.h"
#include "defines.h"
#include "renderer/vulkan/utils.h"

VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_backend_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_types,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data);

bool VulkanBackend::init() {
    // If you want a custom allocator, fill it in here
    m_context.allocator = nullptr;

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
    const char* const* instance_extensions =
        SDL_Vulkan_GetInstanceExtensions(&instance_extension_count);
    if (instance_extensions == nullptr) {
        log_error("Failed to get platform-specific Vulkan extensions: %s",
                  SDL_GetError());
        return false;
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
    VK_CHECK(vkEnumerateInstanceLayerProperties(&available_layer_count,
                                                available_layers.data()));

    // Verify all required layers are available
    for (uint32_t index = 0; index < (uint32_t)layer_names.size(); index++) {
        uint32_t layer_index;
        for (layer_index = 0; layer_index < available_layer_count;
             layer_index++) {
            if (strcmp(layer_names[index],
                       available_layers[layer_index].layerName) == 0) {
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
    VkInstanceCreateInfo instance_create_info{};
    instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_create_info.pApplicationInfo = &app_info;
    instance_create_info.enabledExtensionCount =
        (uint32_t)extension_names.size();
    instance_create_info.ppEnabledExtensionNames = extension_names.data();
    instance_create_info.enabledLayerCount = (uint32_t)layer_names.size();
    instance_create_info.ppEnabledLayerNames = layer_names.data();

    // Create instance
    VkResult result = vkCreateInstance(
        &instance_create_info, m_context.allocator, &m_context.instance);
    if (result != VK_SUCCESS) {
        log_error("vkCreateInstance failed with result %s.",
                  vulkan_result_str(result));
        return false;
    }
    log_info("Vulkan instance created.");

#ifdef ZEN_DEBUG
    log_debug("Creating Vulkan debugger...");

    VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
    debug_create_info.sType =
        VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
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
    ZEN_ASSERT_MESSAGE(
        createDebugUtilsMessenger,
        "Failed to load createDebugUtilsMessenger function pointer.");
    VK_CHECK(createDebugUtilsMessenger(m_context.instance, &debug_create_info,
                                       m_context.allocator,
                                       &m_context.debug_messenger));
    log_debug("Vulkan debugger created.");
#endif

    // Create surface
    if (!SDL_Vulkan_CreateSurface(m_window, m_context.instance,
                                  m_context.allocator, &m_context.surface)) {
        log_error("Failed to create surface %s", SDL_GetError());
        return false;
    }

    return true;
}

void VulkanBackend::quit() {}

void VulkanBackend::on_resized(uint32_t width, uint32_t height) {}

bool VulkanBackend::begin_frame(double delta_time) { return true; }

bool VulkanBackend::end_frame(double delta_time) { return true; }

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
