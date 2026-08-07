#include "render/rhi/rhi_device_factory.h"
#include "render/rhi/gl_rhi_device.h"
#include "render/rhi/d3d12_rhi_device.h"
#include "log/core/log_macros.h"

namespace Entelechy
{

std::unique_ptr<IRHIDevice> createRHIDevice(RenderBackendType type)
{
    switch (type)
    {
    case RenderBackendType::OpenGL:
    {
        auto device = std::make_unique<GLRHIDevice>();
        if (!device->initialize())
        {
            LOG_ERROR(LogCategories::kEngine, "createRHIDevice: GLRHIDevice initialization failed");
            return nullptr;
        }
        return device;
    }
    case RenderBackendType::D3D12:
    {
        auto device = std::make_unique<D3D12RHIDevice>();
        if (!device->initialize())
        {
            LOG_ERROR(LogCategories::kEngine, "createRHIDevice: D3D12RHIDevice initialization failed");
            return nullptr;
        }
        return device;
    }
    case RenderBackendType::Vulkan:
        LOG_ERROR(LogCategories::kEngine, "createRHIDevice: Vulkan backend not yet implemented");
        return nullptr;
    default:
        LOG_ERROR(LogCategories::kEngine, "createRHIDevice: unknown backend type %u",
                  static_cast<u32>(type));
        return nullptr;
    }
}

const char *shaderFileExtensionForBackend(RenderBackendType type)
{
    switch (type)
    {
    case RenderBackendType::OpenGL:
        return ".glsl";
    case RenderBackendType::D3D12:
        return ".dxil";
    case RenderBackendType::Vulkan:
        return ".spv";
    default:
        return ".glsl";
    }
}

ShaderBytecodeFormat shaderFormatForBackend(RenderBackendType type)
{
    switch (type)
    {
    case RenderBackendType::OpenGL:
        return ShaderBytecodeFormat::GLSL;
    case RenderBackendType::D3D12:
        return ShaderBytecodeFormat::DXIL;
    case RenderBackendType::Vulkan:
        return ShaderBytecodeFormat::SPIRV;
    default:
        return ShaderBytecodeFormat::GLSL;
    }
}

} // namespace Entelechy
