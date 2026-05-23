#pragma once
#include "foundation_types.h"
#include "small_string.h"
#include "string_id.h"
#include "dynamic_array.h"
#include "hash_map.h"

namespace Entelechy {

// ------------------------------------------------------------------
// Atom type registry — one function pointer per atomic type for
// Inspector drawing, JSON serialisation, etc.
// ------------------------------------------------------------------

using AtomInspectorFn = void(*)(const char* label, void* ptr);
using AtomSerializeFn = void(*)(void* ptr, SmallString& outJson);
using AtomDeserializeFn = void(*)(void* ptr, const char* json);
using AtomCopyFn = void(*)(void* dst, const void* src);

struct AtomType {
    SmallString name;
    usize size = 0;
    AtomInspectorFn inspectorDraw = nullptr;
    AtomSerializeFn serialize = nullptr;
    AtomDeserializeFn deserialize = nullptr;
    AtomCopyFn copy = nullptr;
};

class AtomRegistry {
public:
    static AtomRegistry& instance();

    void registerAtom(const AtomType& type);
    const AtomType* find(const SmallString& name) const;
    const AtomType* find(StringId name) const;

    // Convenience: draw a field if its type is registered as an atom
    bool tryDraw(const SmallString& typeName, const char* label, void* ptr) const;

    void registerBuiltinAtoms();

private:
    AtomRegistry() = default;
    HashMap<SmallString, AtomType> m_atoms;
};

} // namespace Entelechy
