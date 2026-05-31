#include "ecs/type/atom_registry.h"
#include <imgui.h>
#include "core/string/string_intern_pool.h"
#include "ecs/type/types.h"

namespace Entelechy {

// ------------------------------------------------------------------
// Builtin draw functions (ImGui backend)
// ------------------------------------------------------------------

static void drawF32(const char* label, void* ptr) {
    ImGui::DragFloat(label, static_cast<f32*>(ptr), 0.1f);
}

static void drawBool(const char* label, void* ptr) {
    ImGui::Checkbox(label, static_cast<bool*>(ptr));
}

static void drawI32(const char* label, void* ptr) {
    ImGui::DragInt(label, static_cast<i32*>(ptr));
}

static void drawU32(const char* label, void* ptr) {
    ImGui::DragScalar(label, ImGuiDataType_U32, ptr);
}

static void drawStringId(const char* label, void* ptr) {
    StringId* id = static_cast<StringId*>(ptr);
    const char* resolved = StringInternPool::instance().resolve(*id);
    ImGui::Text("%s: %s", label, resolved ? resolved : "<unresolved>");
}

static void drawSmallString(const char* label, void* ptr) {
    SmallString* str = static_cast<SmallString*>(ptr);
    ImGui::Text("%s: %s", label, str->c_str());
}

void AtomRegistry::registerBuiltinAtoms() {
    registerAtom({"f32",     sizeof(f32),    drawF32});
    registerAtom({"float",   sizeof(f32),    drawF32});
    registerAtom({"bool",    sizeof(bool),   drawBool});
    registerAtom({"i32",     sizeof(i32),    drawI32});
    registerAtom({"int",     sizeof(i32),    drawI32});
    registerAtom({"int32_t", sizeof(i32),    drawI32});
    registerAtom({"u32",     sizeof(u32),    drawU32});
    registerAtom({"uint32_t",sizeof(u32),    drawU32});
    registerAtom({"StringId",sizeof(StringId), drawStringId});
    registerAtom({"SmallString",sizeof(SmallString), drawSmallString});
}

} // namespace Entelechy
