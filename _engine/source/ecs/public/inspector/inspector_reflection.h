#pragma once
#include "core/foundation_types.h"

namespace Entelechy {

struct ComponentDesc;

// Draw an arbitrary component using AtomRegistry + ComponentDesc.
// Recursively expands composite types (Vec3, Quat, Mat4, Entity, ...).
void inspectorDrawComponent(const char* componentName, void* componentPtr, const ComponentDesc* desc);

// Draw a single atom or composite field.
// Returns true if the field was recognised and drawn.
bool inspectorDrawField(const SmallString& typeName, const char* label, void* fieldPtr);

} // namespace Entelechy
