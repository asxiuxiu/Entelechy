#pragma once

// DXC library API wrapper for HLSL compilation to DXIL and SPIR-V.

#include <cstdint>
#include <string>
#include <vector>

namespace Entelechy
{

struct DxcCompileResult
{
    bool success = false;
    std::vector<uint8_t> bytecode;
    std::string error_message;
};

class DxcCompiler
{
public:
    DxcCompiler();
    ~DxcCompiler();

    DxcCompiler(const DxcCompiler &) = delete;
    DxcCompiler &operator=(const DxcCompiler &) = delete;

    bool isValid() const { return m_valid; }

    // Compile HLSL source to DXIL bytecode.
    // targetProfile: e.g. "vs_6_0", "ps_6_0"
    DxcCompileResult compileToDxil(const char *source, size_t sourceLen,
                                   const char *entryPoint,
                                   const char *targetProfile,
                                   const char *fileName = nullptr);

    // Compile HLSL source to SPIR-V bytecode.
    // targetProfile: e.g. "vs_6_0", "ps_6_0"
    DxcCompileResult compileToSpirv(const char *source, size_t sourceLen,
                                    const char *entryPoint,
                                    const char *targetProfile,
                                    const char *fileName = nullptr);

private:
    bool m_valid = false;
    void *m_compiler = nullptr;  // IDxcCompiler3*
    void *m_utils = nullptr;     // IDxcUtils*
};

} // namespace Entelechy
