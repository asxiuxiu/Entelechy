#pragma once

// SPIRV-Cross C API wrapper for SPIR-V to GLSL cross-compilation.

#include <cstdint>
#include <string>
#include <vector>

namespace Entelechy
{

struct SpirvCrossResult
{
    bool success = false;
    std::string glsl_source;
    std::string error_message;
};

struct SpirvReflectionResult
{
    bool success = false;
    std::string error_message;
};

class SpirvCrossCompiler
{
public:
    SpirvCrossCompiler();
    ~SpirvCrossCompiler();

    SpirvCrossCompiler(const SpirvCrossCompiler &) = delete;
    SpirvCrossCompiler &operator=(const SpirvCrossCompiler &) = delete;

    bool isValid() const { return m_valid; }

    // Cross-compile SPIR-V bytecode to desktop GLSL 410.
    SpirvCrossResult compileToGlsl(const uint8_t *spirvData, size_t spirvSize);

    // Dump the cbuffer / texture binding layout of the SPIR-V to a JSON
    // file consumed at runtime by ShaderReflection (6e). The cbuffer member
    // offsets come from the SPIR-V (DXC HLSL packing, identical to DXIL);
    // textures carry their t-register as the binding.
    SpirvReflectionResult writeReflection(const char *jsonPath, const uint8_t *spirvData, size_t spirvSize,
                                          const char *shaderName, const char *stage);

private:
    bool m_valid = false;
};

} // namespace Entelechy
