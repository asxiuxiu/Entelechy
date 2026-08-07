#include "spirv_cross_compiler.h"

#include <spirv_cross/spirv_cross_c.h>

#include <cstdio>
#include <string>

namespace Entelechy
{

SpirvCrossCompiler::SpirvCrossCompiler()
{
    m_valid = true;
}

SpirvCrossCompiler::~SpirvCrossCompiler() = default;

SpirvCrossResult SpirvCrossCompiler::compileToGlsl(const uint8_t *spirvData, size_t spirvSize)
{
    SpirvCrossResult result;

    if (!m_valid || !spirvData || spirvSize == 0)
    {
        result.error_message = "Invalid SPIR-V input";
        return result;
    }

    spvc_context context = nullptr;
    spvc_result err = spvc_context_create(&context);
    if (err != SPVC_SUCCESS)
    {
        result.error_message = "spvc_context_create failed";
        return result;
    }

    spvc_parsed_ir ir = nullptr;
    err = spvc_context_parse_spirv(context, reinterpret_cast<const SpvId *>(spirvData),
                                   spirvSize / sizeof(SpvId), &ir);
    if (err != SPVC_SUCCESS)
    {
        result.error_message = std::string("spvc_context_parse_spirv failed: ") +
                               spvc_context_get_last_error_string(context);
        spvc_context_destroy(context);
        return result;
    }

    spvc_compiler compiler = nullptr;
    err = spvc_context_create_compiler(context, SPVC_BACKEND_GLSL, ir,
                                       SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler);
    if (err != SPVC_SUCCESS)
    {
        result.error_message = std::string("spvc_context_create_compiler failed: ") +
                               spvc_context_get_last_error_string(context);
        spvc_context_destroy(context);
        return result;
    }

    // Flatten all uniform buffer blocks to plain uniforms so the existing GL
    // backend can use glGetUniformLocation/glUniform* without UBO setup.
    spvc_resources resources = nullptr;
    err = spvc_compiler_create_shader_resources(compiler, &resources);
    if (err == SPVC_SUCCESS)
    {
        const spvc_reflected_resource *ubos = nullptr;
        size_t uboCount = 0;
        err = spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER,
                                                        &ubos, &uboCount);
        if (err == SPVC_SUCCESS)
        {
            for (size_t i = 0; i < uboCount; ++i)
                spvc_compiler_flatten_buffer_block(compiler, ubos[i].id);
        }
    }

    // Configure GLSL output options
    spvc_compiler_options options = nullptr;
    err = spvc_compiler_create_compiler_options(compiler, &options);
    if (err == SPVC_SUCCESS)
    {
        // GLSL 4.10 minimum: 3.30 cannot put explicit layout(location) on
        // fragment shader inputs, and without locations the stage interface
        // links by variable NAME — SPIRV-Cross emits out_var_* (VS) vs
        // in_var_* (FS), which never match, leaving every FS input zero
        // (observed 2026-08-07: all varyings read 0, textures/normals dead).
        spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_GLSL_VERSION, 410);
        spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_ES, SPVC_FALSE);
        spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_VULKAN_SEMANTICS, SPVC_FALSE);
        spvc_compiler_install_compiler_options(compiler, options);
    }

    // Build combined image samplers for GLSL compatibility
    spvc_compiler_build_combined_image_samplers(compiler);

    // Rename each combined sampler back to its original HLSL texture name.
    // SPIRV-Cross auto-names them after SPIR-V variable IDs (e.g. "_223"),
    // which follow first-use order in the shader body — NOT the register
    // (t0/t1/t2) declaration order. Binding by those IDs from C++ is fragile
    // and already caused normal/MR textures to be swapped once.
    {
        const spvc_combined_image_sampler *samplers = nullptr;
        size_t samplerCount = 0;
        if (spvc_compiler_get_combined_image_samplers(compiler, &samplers, &samplerCount) == SPVC_SUCCESS)
        {
            for (size_t i = 0; i < samplerCount; ++i)
            {
                const char *imageName = spvc_compiler_get_name(compiler, samplers[i].image_id);
                if (imageName && imageName[0] != '\0')
                    spvc_compiler_set_name(compiler, samplers[i].combined_id, imageName);
            }
        }
    }

    const char *glslSource = nullptr;
    err = spvc_compiler_compile(compiler, &glslSource);
    if (err != SPVC_SUCCESS)
    {
        result.error_message = std::string("spvc_compiler_compile failed: ") +
                               spvc_context_get_last_error_string(context);
        spvc_context_destroy(context);
        return result;
    }

    result.glsl_source = glslSource ? glslSource : "";
    result.success = true;

    spvc_context_destroy(context);
    return result;
}

} // namespace Entelechy
