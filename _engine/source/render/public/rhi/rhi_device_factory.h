#pragma once
#include "render/rhi/rhi_types.h"
#include <memory>

namespace Entelechy
{

class IRHIDevice;

// Factory function for creating RHI device instances.
// Returns nullptr and logs an error for unsupported backend types.
std::unique_ptr<IRHIDevice> createRHIDevice(RenderBackendType type);

// File extension of the precompiled shader bytecode the given backend
// consumes (".glsl" / ".dxil" / ".spv"). Shader file names follow the
// convention "<name>_<stage><ext>" under the shaders/ directory.
const char *shaderFileExtensionForBackend(RenderBackendType type);

// Bytecode format matching shaderFileExtensionForBackend().
ShaderBytecodeFormat shaderFormatForBackend(RenderBackendType type);

} // namespace Entelechy
