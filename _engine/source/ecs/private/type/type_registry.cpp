#include "ecs/type/type_registry.h"
#include "core/string/string_intern_pool.h"
#include <cstdio>

namespace Entelechy {

TypeRegistry& TypeRegistry::instance() {
    static TypeRegistry reg;
    return reg;
}

// ----- ECS component APIs -----

void TypeRegistry::registerComponent(ComponentTypeID id, u64 mask, const ComponentDesc& desc) {
    m_components.insert(id, desc);
    m_name_to_id.insert(desc.name, id);
    m_id_to_name.insert(id, desc.name);
    m_name_to_mask.insert(desc.name, mask);
}

const ComponentDesc* TypeRegistry::findComponent(ComponentTypeID id) const {
    auto* v = m_components.find(id);
    if (v) return v;
    return nullptr;
}

const ComponentDesc* TypeRegistry::findComponent(StringId name) const {
    auto* id = m_name_to_id.find(name);
    if (id) return findComponent(*id);
    return nullptr;
}

ComponentTypeID TypeRegistry::findComponentID(StringId name) const {
    auto* v = m_name_to_id.find(name);
    if (v) return *v;
    return INVALID_COMPONENT_TYPE_ID;
}

u64 TypeRegistry::getComponentMask(StringId name) const {
    auto* v = m_name_to_mask.find(name);
    if (v) return *v;
    return 0;
}

// ----- General type APIs -----

void TypeRegistry::registerType(const TypeDesc& desc) {
    m_types.insert(desc.name, desc);
}

const TypeDesc* TypeRegistry::findType(StringId name) const {
    auto* v = m_types.find(name);
    if (v) return v;
    return nullptr;
}

String TypeRegistry::listComponents() const {
    String json = "[\n";
    bool first = true;
    for (const auto& pair : m_name_to_id) {
        if (!first) json += ",\n";
        first = false;
        json += "  \"";
        const char* resolved = StringInternPool::instance().resolve(pair.first);
        if (resolved) json += resolved;
        json += "\"";
    }
    json += "\n]";
    return json;
}

usize TypeRegistry::componentCount() const {
    return m_components.size();
}

String TypeRegistry::describeComponent(StringId name) const {
    const ComponentDesc* desc = findComponent(name);
    if (!desc) {
        return String("{\"error\":\"component not found\"}");
    }

    String json = "{\n";
    json += "  \"name\": \"";
    const char* nameResolved = StringInternPool::instance().resolve(desc->name);
    if (nameResolved) json += nameResolved;
    json += "\",\n";
    json += "  \"size\": ";
    json += formatString("{0}", static_cast<int>(desc->size));
    json += ",\n";
    json += "  \"fields\": [\n";

    for (usize i = 0; i < desc->fields.size(); ++i) {
        const auto& f = desc->fields[i];
        json += "    {\n";
        json += "      \"name\": \"";
        const char* fieldName = StringInternPool::instance().resolve(f.name);
        if (fieldName) json += fieldName;
        json += "\",\n";
        json += "      \"type\": \"";
        const char* fieldType = StringInternPool::instance().resolve(f.type);
        if (fieldType) json += fieldType;
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
        .name = "Vec2"_sid,
        .size = sizeof(f32) * 2,
        .alignment = alignof(f32),
        .kind = TypeKind::Composite,
        .fields = {
            FieldDesc{"x"_sid, "f32"_sid, 0, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"y"_sid, "f32"_sid, sizeof(f32), sizeof(f32), TypeKind::Atom, {}},
        }
    });

    // Vec3
    registerType(TypeDesc{
        .name = "Vec3"_sid,
        .size = sizeof(f32) * 3,
        .alignment = alignof(f32),
        .kind = TypeKind::Composite,
        .fields = {
            FieldDesc{"x"_sid, "f32"_sid, 0, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"y"_sid, "f32"_sid, sizeof(f32), sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"z"_sid, "f32"_sid, sizeof(f32) * 2, sizeof(f32), TypeKind::Atom, {}},
        }
    });

    // Vec4
    registerType(TypeDesc{
        .name = "Vec4"_sid,
        .size = sizeof(f32) * 4,
        .alignment = alignof(f32),
        .kind = TypeKind::Composite,
        .fields = {
            FieldDesc{"x"_sid, "f32"_sid, 0, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"y"_sid, "f32"_sid, sizeof(f32), sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"z"_sid, "f32"_sid, sizeof(f32) * 2, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"w"_sid, "f32"_sid, sizeof(f32) * 3, sizeof(f32), TypeKind::Atom, {}},
        }
    });

    // Quat
    registerType(TypeDesc{
        .name = "Quat"_sid,
        .size = sizeof(f32) * 4,
        .alignment = alignof(f32),
        .kind = TypeKind::Composite,
        .fields = {
            FieldDesc{"x"_sid, "f32"_sid, 0, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"y"_sid, "f32"_sid, sizeof(f32), sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"z"_sid, "f32"_sid, sizeof(f32) * 2, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"w"_sid, "f32"_sid, sizeof(f32) * 3, sizeof(f32), TypeKind::Atom, {}},
        }
    });

    // Mat4 (4x4 floats) — show as rows for readability
    registerType(TypeDesc{
        .name = "Mat4"_sid,
        .size = sizeof(f32) * 16,
        .alignment = alignof(f32),
        .kind = TypeKind::Composite,
        .fields = {
            FieldDesc{"m00"_sid, "f32"_sid, 0, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"m10"_sid, "f32"_sid, sizeof(f32), sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"m20"_sid, "f32"_sid, sizeof(f32) * 2, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"m30"_sid, "f32"_sid, sizeof(f32) * 3, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"m01"_sid, "f32"_sid, sizeof(f32) * 4, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"m11"_sid, "f32"_sid, sizeof(f32) * 5, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"m21"_sid, "f32"_sid, sizeof(f32) * 6, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"m31"_sid, "f32"_sid, sizeof(f32) * 7, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"m02"_sid, "f32"_sid, sizeof(f32) * 8, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"m12"_sid, "f32"_sid, sizeof(f32) * 9, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"m22"_sid, "f32"_sid, sizeof(f32) * 10, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"m32"_sid, "f32"_sid, sizeof(f32) * 11, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"m03"_sid, "f32"_sid, sizeof(f32) * 12, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"m13"_sid, "f32"_sid, sizeof(f32) * 13, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"m23"_sid, "f32"_sid, sizeof(f32) * 14, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"m33"_sid, "f32"_sid, sizeof(f32) * 15, sizeof(f32), TypeKind::Atom, {}},
        }
    });

    // Entity
    registerType(TypeDesc{
        .name = "Entity"_sid,
        .size = sizeof(u32) * 2,
        .alignment = alignof(u32),
        .kind = TypeKind::Composite,
        .fields = {
            FieldDesc{"id"_sid, "u32"_sid, 0, sizeof(u32), TypeKind::Atom, {}},
            FieldDesc{"generation"_sid, "u32"_sid, sizeof(u32), sizeof(u32), TypeKind::Atom, {}},
        }
    });

    // Color
    registerType(TypeDesc{
        .name = "Color"_sid,
        .size = sizeof(f32) * 3,
        .alignment = alignof(f32),
        .kind = TypeKind::Composite,
        .fields = {
            FieldDesc{"r"_sid, "f32"_sid, 0, sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"g"_sid, "f32"_sid, sizeof(f32), sizeof(f32), TypeKind::Atom, {}},
            FieldDesc{"b"_sid, "f32"_sid, sizeof(f32) * 2, sizeof(f32), TypeKind::Atom, {}},
        }
    });

    printf("[TypeRegistry] registered %zu builtin composite types\n", m_types.size());
}

} // namespace Entelechy
