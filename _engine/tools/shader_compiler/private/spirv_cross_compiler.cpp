#include "spirv_cross_compiler.h"

#include <spirv_cross/spirv_cross_c.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace Entelechy
{

namespace
{

// A fresh SPIRV-Cross context + compiler for one SPIR-V blob.
// The context is destroyed when the session goes out of scope.
struct SpvcSession
{
    spvc_context context = nullptr;
    spvc_compiler compiler = nullptr;

    ~SpvcSession()
    {
        if (context)
            spvc_context_destroy(context);
    }
};

bool createSession(const uint8_t *spirvData, size_t spirvSize, SpvcSession &out, std::string &err)
{
    if (!spirvData || spirvSize == 0)
    {
        err = "Invalid SPIR-V input";
        return false;
    }

    spvc_context context = nullptr;
    if (spvc_context_create(&context) != SPVC_SUCCESS)
    {
        err = "spvc_context_create failed";
        return false;
    }
    out.context = context;

    spvc_parsed_ir ir = nullptr;
    if (spvc_context_parse_spirv(context, reinterpret_cast<const SpvId *>(spirvData), spirvSize / sizeof(SpvId),
                                 &ir) != SPVC_SUCCESS)
    {
        err = std::string("spvc_context_parse_spirv failed: ") + spvc_context_get_last_error_string(context);
        return false;
    }

    spvc_compiler compiler = nullptr;
    if (spvc_context_create_compiler(context, SPVC_BACKEND_GLSL, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP,
                                     &compiler) != SPVC_SUCCESS)
    {
        err = std::string("spvc_context_create_compiler failed: ") + spvc_context_get_last_error_string(context);
        return false;
    }
    out.compiler = compiler;
    return true;
}

// After build_combined_image_samplers, each synthesized combined sampler has
// no Binding decoration of its own. Inherit it from the source image so the
// emitted GLSL sampler carries `layout(binding = N)` (N == the HLSL t-register
// == the texture unit the C++ side binds). Also rename the combined sampler
// back to the original HLSL texture name.
void bindAndRenameCombinedSamplers(spvc_compiler compiler)
{
    const spvc_combined_image_sampler *samplers = nullptr;
    size_t samplerCount = 0;
    if (spvc_compiler_get_combined_image_samplers(compiler, &samplers, &samplerCount) != SPVC_SUCCESS)
        return;

    for (size_t i = 0; i < samplerCount; ++i)
    {
        const char *imageName = spvc_compiler_get_name(compiler, samplers[i].image_id);
        if (imageName && imageName[0] != '\0')
            spvc_compiler_set_name(compiler, samplers[i].combined_id, imageName);

        const unsigned binding =
            spvc_compiler_get_decoration(compiler, samplers[i].image_id, SpvDecorationBinding);
        spvc_compiler_set_decoration(compiler, samplers[i].combined_id, SpvDecorationBinding, binding);
    }
}

// Classify a SPIR-V member type into the reflection "type" string and its
// padded size in bytes (std140 / HLSL constant-buffer packing).
const char *memberTypeInfo(spvc_compiler compiler, spvc_type_id typeId, unsigned &sizeOut)
{
    spvc_type type = spvc_compiler_get_type_handle(compiler, typeId);
    const spvc_basetype base = spvc_type_get_basetype(type);
    const unsigned vec = spvc_type_get_vector_size(type);
    const unsigned cols = spvc_type_get_columns(type);

    if (base == SPVC_BASETYPE_FP32)
    {
        if (cols >= 2)
        {
            if (cols == 3)
            {
                sizeOut = 48; // mat3: 3 columns of vec3, each 16 bytes
                return "mat3";
            }
            if (cols == 4)
            {
                sizeOut = 64;
                return "mat4";
            }
        }
        switch (vec)
        {
        case 1:
            sizeOut = 4;
            return "float";
        case 2:
            sizeOut = 8;
            return "vec2";
        case 3:
            sizeOut = 16; // vec3 padded to 16 in std140
            return "vec3";
        case 4:
            sizeOut = 16;
            return "vec4";
        default:
            break;
        }
    }
    else if (base == SPVC_BASETYPE_INT32)
    {
        sizeOut = 4;
        return "int";
    }
    else if (base == SPVC_BASETYPE_UINT32)
    {
        sizeOut = 4;
        return "uint";
    }
    else if (base == SPVC_BASETYPE_BOOLEAN)
    {
        sizeOut = 4;
        return "bool";
    }

    sizeOut = 4;
    return "unknown";
}

} // namespace

SpirvCrossCompiler::SpirvCrossCompiler()
{
    m_valid = true;
}

SpirvCrossCompiler::~SpirvCrossCompiler() = default;

SpirvCrossResult SpirvCrossCompiler::compileToGlsl(const uint8_t *spirvData, size_t spirvSize)
{
    SpirvCrossResult result;

    std::string sessionErr;
    SpvcSession session;
    if (!createSession(spirvData, spirvSize, session, sessionErr))
    {
        result.error_message = sessionErr;
        return result;
    }
    spvc_compiler compiler = session.compiler;

    // Configure GLSL output options
    spvc_compiler_options options = nullptr;
    if (spvc_compiler_create_compiler_options(compiler, &options) != SPVC_SUCCESS)
    {
        result.error_message = "spvc_compiler_create_compiler_options failed";
        return result;
    }
    // GLSL 4.10 minimum: 3.30 cannot put explicit layout(location) on
    // fragment shader inputs, and without locations the stage interface
    // links by variable NAME — SPIRV-Cross emits out_var_* (VS) vs
    // in_var_* (FS), which never match, leaving every FS input zero
    // (observed 2026-08-07: all varyings read 0, textures/normals dead).
    spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_GLSL_VERSION, 410);
    // Enable the 420pack extension so UBOs and samplers get explicit
    // `layout(binding = N)` (needs 420 core or the ARB extension).
    spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_ENABLE_420PACK_EXTENSION, SPVC_TRUE);
    spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_ES, SPVC_FALSE);
    spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_VULKAN_SEMANTICS, SPVC_FALSE);
    spvc_compiler_install_compiler_options(compiler, options);

    // cbuffers stay as real UBO blocks (layout(binding = N, std140)) — the
    // runtime binds them with glBindBufferRange at the binding point, so no
    // flattened `uniform vec4 type_PerFrame[N]` names are ever looked up.
    // (Pre-6e this flattened every block; see RENDER_LAYER_PROGRESS 5.12.)

    // Build combined image samplers for GLSL compatibility
    spvc_compiler_build_combined_image_samplers(compiler);
    bindAndRenameCombinedSamplers(compiler);

    const char *glslSource = nullptr;
    if (spvc_compiler_compile(compiler, &glslSource) != SPVC_SUCCESS)
    {
        result.error_message = std::string("spvc_compiler_compile failed: ") +
                               spvc_context_get_last_error_string(session.context);
        return result;
    }

    result.glsl_source = glslSource ? glslSource : "";
    result.success = true;
    return result;
}

SpirvReflectionResult SpirvCrossCompiler::writeReflection(const char *jsonPath, const uint8_t *spirvData,
                                                          size_t spirvSize, const char *shaderName,
                                                          const char *stage)
{
    SpirvReflectionResult result;

    std::string sessionErr;
    SpvcSession session;
    if (!createSession(spirvData, spirvSize, session, sessionErr))
    {
        result.error_message = sessionErr;
        return result;
    }
    spvc_compiler compiler = session.compiler;

    spvc_resources resources = nullptr;
    if (spvc_compiler_create_shader_resources(compiler, &resources) != SPVC_SUCCESS)
    {
        result.error_message = "spvc_compiler_create_shader_resources failed";
        return result;
    }

    std::string json;
    json += "{\n";
    json += "  \"name\": \"";
    json += shaderName;
    json += "\",\n";
    json += "  \"stage\": \"";
    json += stage;
    json += "\",\n";
    json += "  \"cbuffers\": [";

    const spvc_reflected_resource *ubos = nullptr;
    size_t uboCount = 0;
    bool firstCbuffer = true;
    if (spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, &ubos, &uboCount) ==
        SPVC_SUCCESS)
    {
        for (size_t i = 0; i < uboCount; ++i)
        {
            const unsigned binding = spvc_compiler_get_decoration(compiler, ubos[i].id, SpvDecorationBinding);
            spvc_type structType = spvc_compiler_get_type_handle(compiler, ubos[i].base_type_id);
            size_t structSize = 0;
            spvc_compiler_get_declared_struct_size(compiler, structType, &structSize);

            if (!firstCbuffer)
                json += ",";
            firstCbuffer = false;
            // Prefer the instance (variable) name ("PerFrame") over the
            // DXC type name ("type.PerFrame") for readability.
            const char *cbufferName = spvc_compiler_get_name(compiler, ubos[i].id);
            if (!cbufferName || cbufferName[0] == '\0')
                cbufferName = ubos[i].name ? ubos[i].name : "";
            json += "\n    { \"name\": \"";
            json += cbufferName;
            json += "\", \"binding\": ";
            json += std::to_string(binding);
            json += ", \"size\": ";
            json += std::to_string(structSize);
            json += ", \"members\": [";

            const unsigned memberCount = spvc_type_get_num_member_types(structType);
            for (unsigned m = 0; m < memberCount; ++m)
            {
                const char *memberName = spvc_compiler_get_member_name(compiler, ubos[i].base_type_id, m);
                // 1.4.350 C API exposes member layout only via the Offset
                // decoration (DXC emits it for every cbuffer member).
                const size_t memberOffset =
                    spvc_compiler_get_member_decoration(compiler, ubos[i].base_type_id, m, SpvDecorationOffset);
                unsigned memberSize = 0;
                const char *memberType =
                    memberTypeInfo(compiler, spvc_type_get_member_type(structType, m), memberSize);

                if (m > 0)
                    json += ",";
                json += "\n        { \"name\": \"";
                json += memberName ? memberName : "";
                json += "\", \"type\": \"";
                json += memberType;
                json += "\", \"offset\": ";
                json += std::to_string(memberOffset);
                json += ", \"size\": ";
                json += std::to_string(memberSize);
                json += " }";
            }
            json += "\n      ] }";
        }
    }

    json += "\n  ],\n  \"textures\": [";

    const spvc_reflected_resource *images = nullptr;
    size_t imageCount = 0;
    bool firstTexture = true;
    if (spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_SEPARATE_IMAGE, &images,
                                                  &imageCount) == SPVC_SUCCESS)
    {
        for (size_t i = 0; i < imageCount; ++i)
        {
            const unsigned binding = spvc_compiler_get_decoration(compiler, images[i].id, SpvDecorationBinding);

            if (!firstTexture)
                json += ",";
            firstTexture = false;
            json += "\n    { \"name\": \"";
            json += images[i].name ? images[i].name : "";
            json += "\", \"binding\": ";
            json += std::to_string(binding);
            json += " }";
        }
    }

    json += "\n  ]\n}\n";

    FILE *f = std::fopen(jsonPath, "wb");
    if (!f)
    {
        result.error_message = std::string("Cannot open reflection file for write: ") + jsonPath;
        return result;
    }
    const bool written = std::fwrite(json.data(), 1, json.size(), f) == json.size();
    std::fclose(f);
    if (!written)
    {
        result.error_message = std::string("Failed to write reflection file: ") + jsonPath;
        return result;
    }

    result.success = true;
    return result;
}

} // namespace Entelechy
