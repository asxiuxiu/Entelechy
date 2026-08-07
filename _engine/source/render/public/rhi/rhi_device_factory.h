#pragma once
#include "render/rhi/rhi_types.h"
#include <memory>

namespace Entelechy
{

class IRHIDevice;

// Factory function for creating RHI device instances.
// Returns nullptr and logs an error for unsupported backend types.
std::unique_ptr<IRHIDevice> createRHIDevice(RenderBackendType type);

} // namespace Entelechy
