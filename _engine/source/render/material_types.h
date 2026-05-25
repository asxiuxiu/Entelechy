#pragma once
#include "foundation_types.h"
#include "string_id.h"

namespace Entelechy {

// ------------------------------------------------------------------
// Material parameter type enumeration
//
// Phase 1: simplified scalar/vector/matrix types + texture reference.
// Future: add arrays, structured buffers, sampler states.
// ------------------------------------------------------------------
enum class MaterialParamType : u8 {
    Float,
    Vec2,
    Vec3,
    Vec4,
    Mat4,
    Texture,
    Count
};

// ------------------------------------------------------------------
// Material parameter layout description
//
// Passed to Material::init() to define the CPU uniform block layout.
// Offsets are computed automatically by Material using std140 rules.
// ------------------------------------------------------------------
struct MaterialParamDesc {
    const char* name = nullptr;   // Uniform name in shader (must outlive Material)
    MaterialParamType type = MaterialParamType::Float;
};

} // namespace Entelechy
