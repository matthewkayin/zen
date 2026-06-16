#include "shader_utils.h"

#include "core/logger.h"
#include <cstdio>
#include <cstring>

bool vulkan_shader_module_create(
        VulkanContext* context,
        const char* name,
        const char* type_str,
        VkShaderStageFlagBits stage_flag,
        VulkanShaderStage* out_stage) {

    // Determine shader path
    char filename[512];
    sprintf(filename, "res/shader/%s.%s.spv", name, type_str);

    // Open shader file
    FILE* shader_file = fopen(filename, "rb");
    if (!shader_file) {
        log_error("Unable to open shader module %s", filename);
        return false;
    }

    // Alloc buffer for shader contents
    fseek(shader_file, 0L, SEEK_END);
    size_t shader_file_size = ftell(shader_file);
    uint8_t* shader_file_data = (uint8_t*)malloc(shader_file_size);
    if (!shader_file_data) {
        log_error("Failed to alloc buffer for shader contents.");
        fclose(shader_file);
        return false;
    }

    // Copy shader contents into buffer
    clearerr(shader_file);
    fseek(shader_file, 0L, SEEK_SET);
    size_t bytes_read = fread(shader_file_data, 1, shader_file_size, shader_file);
    if (bytes_read != shader_file_size) {
        log_error("Failed to read shader file contents. Bytes read %zu vs file size %zu", bytes_read, shader_file_size);
        fclose(shader_file);
        free(shader_file_data);
        return false;
    }

    // Close the file
    fclose(shader_file);

    // Fill out shader create info
    memset(&out_stage->create_info, 0, sizeof(out_stage->create_info));
    out_stage->create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    out_stage->create_info.codeSize = shader_file_size;
    out_stage->create_info.pCode = (uint32_t*)shader_file_data;

    VK_CHECK(vkCreateShaderModule(
        context->device.logical_device,
        &out_stage->create_info,
        context->allocator,
        &out_stage->handle));

    // Shader stage info
    memset(&out_stage->shader_stage_create_info, 0, sizeof(out_stage->shader_stage_create_info));
    out_stage->shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    out_stage->shader_stage_create_info.stage = stage_flag;
    out_stage->shader_stage_create_info.module = out_stage->handle;
    out_stage->shader_stage_create_info.pName = "main";

    free(shader_file_data);

    return true;
}
