#include "dxc_compiler.h"

// Windows headers must come before DXC (provides LPCWSTR, HRESULT, etc.)
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <objbase.h>
#endif

// DXC headers — defines CROSS_PLATFORM_UUIDOF which attaches __uuidof to interfaces
#include <dxcapi.h>

#include <cstdio>
#include <cstring>
#include <vector>

namespace Entelechy
{

DxcCompiler::DxcCompiler()
{
    IDxcCompiler3 *compiler = nullptr;
    HRESULT hr = DxcCreateInstance(CLSID_DxcCompiler, __uuidof(IDxcCompiler3),
                                   reinterpret_cast<void **>(&compiler));
    if (FAILED(hr) || !compiler)
    {
        fprintf(stderr, "[ShaderCompiler] Failed to create DXC compiler: HRESULT 0x%08X\n",
                static_cast<unsigned>(hr));
        return;
    }
    m_compiler = compiler;

    IDxcUtils *utils = nullptr;
    hr = DxcCreateInstance(CLSID_DxcUtils, __uuidof(IDxcUtils),
                           reinterpret_cast<void **>(&utils));
    if (FAILED(hr) || !utils)
    {
        fprintf(stderr, "[ShaderCompiler] Failed to create DXC utils: HRESULT 0x%08X\n",
                static_cast<unsigned>(hr));
        return;
    }
    m_utils = utils;

    m_valid = true;
}

DxcCompiler::~DxcCompiler()
{
    if (m_compiler)
    {
        static_cast<IDxcCompiler3 *>(m_compiler)->Release();
        m_compiler = nullptr;
    }
    if (m_utils)
    {
        static_cast<IDxcUtils *>(m_utils)->Release();
        m_utils = nullptr;
    }
}

static DxcCompileResult compileInternal(IDxcCompiler3 *compiler,
                                        const char *source, size_t sourceLen,
                                        const char *entryPoint,
                                        const char *targetProfile,
                                        bool spirvOutput)
{
    DxcCompileResult result;

    // Build argument list as wide strings
    std::vector<std::wstring> argStorage;

    auto toWide = [](const char *str) -> std::wstring {
        int len = MultiByteToWideChar(CP_UTF8, 0, str, -1, nullptr, 0);
        std::wstring wide(static_cast<size_t>(len) - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, str, -1, &wide[0], len);
        return wide;
    };

    argStorage.push_back(toWide("-T"));
    argStorage.push_back(toWide(targetProfile));
    argStorage.push_back(toWide("-E"));
    argStorage.push_back(toWide(entryPoint));

    if (spirvOutput)
    {
        argStorage.push_back(L"-spirv");
        argStorage.push_back(L"-fspv-target-env=vulkan1.2");
    }

    // Build args array after all strings are in storage (stable pointers)
    std::vector<LPCWSTR> args;
    for (const auto &s : argStorage)
        args.push_back(s.c_str());

    // Source buffer
    DxcBuffer buf = {};
    buf.Ptr = source;
    buf.Size = sourceLen;
    buf.Encoding = DXC_CP_UTF8;

    // Compile
    IDxcResult *compileResult = nullptr;
    HRESULT hr = compiler->Compile(
        &buf,
        args.data(),
        static_cast<UINT32>(args.size()),
        nullptr, // include handler
        __uuidof(IDxcResult),
        reinterpret_cast<void **>(&compileResult));

    if (FAILED(hr))
    {
        result.error_message = "DXC Compile() failed: HRESULT 0x" + std::to_string(hr);
        return result;
    }

    // Check compilation status first (more reliable than error string parsing)
    HRESULT status = S_OK;
    compileResult->GetStatus(&status);
    if (FAILED(status))
    {
        IDxcBlobUtf8 *errors = nullptr;
        compileResult->GetOutput(DXC_OUT_ERRORS, __uuidof(IDxcBlobUtf8),
                                 reinterpret_cast<void **>(&errors), nullptr);
        result.error_message = errors ? errors->GetStringPointer() : "DXC compilation failed";
        if (errors) errors->Release();
        compileResult->Release();
        return result;
    }

    // Print any warnings (non-fatal)
    IDxcBlobUtf8 *warnings = nullptr;
    compileResult->GetOutput(DXC_OUT_ERRORS, __uuidof(IDxcBlobUtf8),
                             reinterpret_cast<void **>(&warnings), nullptr);
    if (warnings && warnings->GetStringLength() > 0)
    {
        fprintf(stderr, "  [WARN] %s\n", warnings->GetStringPointer());
    }
    if (warnings)
        warnings->Release();


    // Get compiled bytecode
    IDxcBlob *blob = nullptr;
    compileResult->GetOutput(DXC_OUT_OBJECT, __uuidof(IDxcBlob),
                             reinterpret_cast<void **>(&blob), nullptr);
    if (!blob || blob->GetBufferSize() == 0)
    {
        result.error_message = "DXC produced empty output";
        if (blob)
            blob->Release();
        compileResult->Release();
        return result;
    }

    const uint8_t *data = static_cast<const uint8_t *>(blob->GetBufferPointer());
    result.bytecode.assign(data, data + blob->GetBufferSize());
    result.success = true;

    blob->Release();
    compileResult->Release();
    return result;
}

DxcCompileResult DxcCompiler::compileToDxil(const char *source, size_t sourceLen,
                                            const char *entryPoint,
                                            const char *targetProfile,
                                            const char *fileName)
{
    if (!m_valid)
    {
        return {false, {}, "DXC compiler not initialized"};
    }
    return compileInternal(static_cast<IDxcCompiler3 *>(m_compiler),
                           source, sourceLen, entryPoint, targetProfile, false);
}

DxcCompileResult DxcCompiler::compileToSpirv(const char *source, size_t sourceLen,
                                             const char *entryPoint,
                                             const char *targetProfile,
                                             const char *fileName)
{
    if (!m_valid)
    {
        return {false, {}, "DXC compiler not initialized"};
    }
    return compileInternal(static_cast<IDxcCompiler3 *>(m_compiler),
                           source, sourceLen, entryPoint, targetProfile, true);
}

} // namespace Entelechy
