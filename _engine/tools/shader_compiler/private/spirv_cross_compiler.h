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

class SpirvCrossCompiler
{
public:
    SpirvCrossCompiler();
    ~SpirvCrossCompiler();

    SpirvCrossCompiler(const SpirvCrossCompiler &) = delete;
    SpirvCrossCompiler &operator=(const SpirvCrossCompiler &) = delete;

    bool isValid() const { return m_valid; }

    // Cross-compile SPIR-V bytecode to desktop GLSL 450.
    SpirvCrossResult compileToGlsl(const uint8_t *spirvData, size_t spirvSize);

private:
    bool m_valid = false;
};

} // namespace Entelechy
