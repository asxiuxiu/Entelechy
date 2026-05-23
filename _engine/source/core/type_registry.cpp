#include "type_registry.h"
#include <cstdio>

namespace Entelechy {

TypeRegistry& TypeRegistry::instance() {
    static TypeRegistry reg;
    return reg;
}

// ----- ECS component APIs -----

void TypeRegistry::registerComponent(ComponentTypeID id, u32 mask, const ComponentDesc& desc) {
    m_components.insert(id, desc);
    m_nameToID.insert(desc.name, id);
    m_idToName.insert(id, desc.name);
    m_nameToMask.insert(desc.name, mask);
}

const ComponentDesc* TypeRegistry::findComponent(ComponentTypeID id) const {
    auto* v = m_components.find(id);
    if (v) return v;
    return nullptr;
}

const ComponentDesc* TypeRegistry::findComponent(const SmallString& name) const {
    auto* id = m_nameToID.find(name);
    if (id) return findComponent(*id);
    return nullptr;
}

ComponentTypeID TypeRegistry::findComponentID(const SmallString& name) const {
    auto* v = m_nameToID.find(name);
    if (v) return *v;
    return INVALID_COMPONENT_TYPE_ID;
}

u32 TypeRegistry::getComponentMask(const SmallString& name) const {
    auto* v = m_nameToMask.find(name);
    if (v) return *v;
    return 0;
}

// ----- General type APIs -----

void TypeRegistry::registerType(const TypeDesc& desc) {
    m_types.insert(desc.name, desc);
}

const TypeDesc* TypeRegistry::findType(const SmallString& name) const {
    auto* v = m_types.find(name);
    if (v) return v;
    return nullptr;
}

const TypeDesc* TypeRegistry::findType(StringId name) const {
    // StringId doesn't directly map to SmallString in HashMap; fallback to linear scan
    // (types table is small, so this is acceptable for now)
    for (const auto& pair : m_types) {
        if (StringId(pair.first.c_str()) == name) {
            return &pair.second;
        }
    }
    return nullptr;
}

SmallString TypeRegistry::listComponents() const {
    SmallString json = "[\n";
    bool first = true;
    for (const auto& pair : m_nameToID) {
        if (!first) json += ",\n";
        first = false;
        json += "  \"";
        json += pair.first;
        json += "\"";
    }
    json += "\n]";
    return json;
}

usize TypeRegistry::componentCount() const {
    return m_components.size();
}

SmallString TypeRegistry::describeComponent(const SmallString& name) const {
    const ComponentDesc* desc = findComponent(name);
    if (!desc) {
        return SmallString("{\"error\":\"component not found\"}");
    }

    SmallString json = "{\n";
    json += "  \"name\": \"";
    json += desc->name;
    json += "\",\n";
    json += "  \"size\": ";
    json += formatString("{0}", static_cast<int>(desc->size));
    json += ",\n";
    json += "  \"fields\": [\n";

    for (usize i = 0; i < desc->fields.size(); ++i) {
        const auto& f = desc->fields[i];
        json += "    {\n";
        json += "      \"name\": \"";
        json += f.name;
        json += "\",\n";
        json += "      \"type\": \"";
        json += f.type;
        json += "\",\n";
        json += "      \"offset\": ";
        json += formatString("{0}", static_cast<int>(f.offset));
        json += ",\n";
        json += "      \"size\": ";
        json += formatString("{0}", static_cast<int>(f.size));
        json += "\n";
        json += "    }";
        if (i + 1 < desc->fields.size()) {
            json += ",";
        }
        json += "\n";
    }

    json += "  ]\n";
    json += "}";
    return json;
}

// ------------------------------------------------------------------
// Builtin composite types (Vec3, Quat, Mat4, Entity, Color, ...)
// ------------------------------------------------------------------
void TypeRegistry::registerBuiltinTypes() {
    // Vec2
    registerType(TypeDesc{
        .name = "Vec2",
        .size = sizeof(f32) * 2,
        .alignment = alignof(f32),
        .kind = TypeKind::Composite,
        .fields = {
            FieldDesc{"x", "f32", 0, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"y", "f32", sizeof(f32), sizeof(f32), TypeKind::Atom, {}},
        }
    });

    // Vec3
    registerType(TypeDesc{
        .name = "Vec3",
        .size = sizeof(f32) * 3,
        .alignment = alignof(f32),
        .kind = TypeKind::Composite,
        .fields = {
            FieldDesc{"x", "f32", 0, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"y", "f32", sizeof(f32), sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"z", "f32", sizeof(f32) * 2, sizeof(f32), TypeKind::Atom, {}},
        }
    });

    // Vec4
    registerType(TypeDesc{
        .name = "Vec4",
        .size = sizeof(f32) * 4,
        .alignment = alignof(f32),
        .kind = TypeKind::Composite,
        .fields = {
            FieldDesc{"x", "f32", 0, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"y", "f32", sizeof(f32), sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"z", "f32", sizeof(f32) * 2, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"w", "f32", sizeof(f32) * 3, sizeof(f32), TypeKind::Atom, {}},
        }
    });

    // Quat
    registerType(TypeDesc{
        .name = "Quat",
        .size = sizeof(f32) * 4,
        .alignment = alignof(f32),
        .kind = TypeKind::Composite,
        .fields = {
            FieldDesc{"x", "f32", 0, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"y", "f32", sizeof(f32), sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"z", "f32", sizeof(f32) * 2, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"w", "f32", sizeof(f32) * 3, sizeof(f32), TypeKind::Atom, {}},
        }
    });

    // Mat4 (4x4 floats) — show as rows for readability
    registerType(TypeDesc{
        .name = "Mat4",
        .size = sizeof(f32) * 16,
        .alignment = alignof(f32),
        .kind = TypeKind::Composite,
        .fields = {
            FieldDesc{"m00", "f32", 0, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"m10", "f32", sizeof(f32), sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"m20", "f32", sizeof(f32) * 2, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"m30", "f32", sizeof(f32) * 3, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"m01", "f32", sizeof(f32) * 4, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"m11", "f32", sizeof(f32) * 5, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"m21", "f32", sizeof(f32) * 6, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"m31", "f32", sizeof(f32) * 7, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"m02", "f32", sizeof(f32) * 8, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"m12", "f32", sizeof(f32) * 9, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"m22", "f32", sizeof(f32) * 10, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"m32", "f32", sizeof(f32) * 11, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"m03", "f32", sizeof(f32) * 12, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"m13", "f32", sizeof(f32) * 13, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"m23", "f32", sizeof(f32) * 14, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"m33", "f32", sizeof(f32) * 15, sizeof(f32), TypeKind::Atom, {}},
        }
    });

    // Entity
    registerType(TypeDesc{
        .name = "Entity",
        .size = sizeof(u32) * 2,
        .alignment = alignof(u32),
        .kind = TypeKind::Composite,
        .fields = {
            FieldDesc{"id", "u32", 0, sizeof(u32), TypeKind::Atom, {}},
            FieldDesc{"generation", "u32", sizeof(u32), sizeof(u32), TypeKind::Atom, {}},
        }
    });

    // Color
    registerType(TypeDesc{
        .name = "Color",
        .size = sizeof(f32) * 3,
        .alignment = alignof(f32),
        .kind = TypeKind::Composite,
        .fields = {
            FieldDesc{"r", "f32", 0, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"g", "f32", sizeof(f32), sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"b", "f32", sizeof(f32) * 2, sizeof(f32), TypeKind::Atom, {}},
        }
    });

    printf("[TypeRegistry] registered %zu builtin composite types\n", m_types.size());
}

} // namespace Entelechy
