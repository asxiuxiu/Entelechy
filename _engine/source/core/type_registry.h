#pragma once
#include "foundation_types.h"
#include "small_string.h"
#include "string_format.h"
#include "hash_map.h"
#include "dynamic_array.h"
#include "string_id.h"
#include <cstddef>

namespace Entelechy {

using ComponentTypeID = u32;
constexpr ComponentTypeID INVALID_COMPONENT_TYPE_ID = 0xFFFFFFFFu;

// ------------------------------------------------------------------
// TypeKind — classification of field / type shape
// ------------------------------------------------------------------
enum class TypeKind { Atom, Composite, Array };

// ------------------------------------------------------------------
// FieldDesc — one field inside a composite type
// ------------------------------------------------------------------
struct FieldDesc {
    SmallString name;
    SmallString type;   // original type name (e.g. "Vec3", "f32")
    usize offset = 0;
    usize size = 0;
    TypeKind kind = TypeKind::Atom;
    SmallString subtype; // when kind == Composite: name of the composite type in TypeRegistry
};

// ------------------------------------------------------------------
// TypeDesc — runtime description of any type (component or composite)
// ------------------------------------------------------------------
struct TypeDesc {
    SmallString name;
    usize size = 0;
    usize alignment = 0;
    TypeKind kind = TypeKind::Composite;
    DynamicArray<FieldDesc> fields;
};

// ------------------------------------------------------------------
// Legacy ComponentDesc (kept for binary compatibility during transition)
// ------------------------------------------------------------------
struct ComponentDesc {
    SmallString name;
    usize size = 0;
    DynamicArray<FieldDesc> fields;
};

// Cross-translation-unit type ID storage (C++17 inline static).
template<typename T>
struct TypeIDStorage {
    static inline ComponentTypeID s_id = INVALID_COMPONENT_TYPE_ID;
};

// ------------------------------------------------------------------
// TypeRegistry — ECS component IDs + general type descriptions
// ------------------------------------------------------------------
class TypeRegistry {
public:
    static TypeRegistry& instance();

    // ----- ECS component type IDs (unchanged) -----
    template<typename T>
    ComponentTypeID getOrAllocateTypeID() {
        ComponentTypeID& id = TypeIDStorage<T>::s_id;
        if (id == INVALID_COMPONENT_TYPE_ID) {
            id = allocateNextID();
        }
        return id;
    }

    template<typename T>
    [[nodiscard]] ComponentTypeID getTypeID() const {
        return TypeIDStorage<T>::s_id;
    }

    template<typename T>
    [[nodiscard]] u32 getMask() const {
        ComponentTypeID id = getTypeID<T>();
        CHECK(id != INVALID_COMPONENT_TYPE_ID && "Component type not registered");
        return (1u << id);
    }

    void registerComponent(ComponentTypeID id, u32 mask, const ComponentDesc& desc);
    const ComponentDesc* findComponent(ComponentTypeID id) const;
    const ComponentDesc* findComponent(const SmallString& name) const;
    ComponentTypeID findComponentID(const SmallString& name) const;
    u32 getComponentMask(const SmallString& name) const;

    // ----- General type descriptions (new) -----
    void registerType(const TypeDesc& desc);
    const TypeDesc* findType(const SmallString& name) const;
    const TypeDesc* findType(StringId name) const;

    SmallString listComponents() const;
    SmallString describeComponent(const SmallString& name) const;
    usize componentCount() const;

    void registerBuiltinTypes();

private:
    TypeRegistry() = default;
    ComponentTypeID allocateNextID() {
        CHECK(m_nextID < 32 && "Exceeded maximum 32 component types");
        return m_nextID++;
    }

    // ECS component tables
    HashMap<ComponentTypeID, ComponentDesc> m_components;
    HashMap<SmallString, ComponentTypeID> m_nameToID;
    HashMap<ComponentTypeID, SmallString> m_idToName;
    HashMap<SmallString, u32> m_nameToMask;
    ComponentTypeID m_nextID = 0;

    // General type table
    HashMap<SmallString, TypeDesc> m_types;
};

// ------------------------------------------------------------------
// Helper macros
// ------------------------------------------------------------------
#define REG_FIELD(struct_type, field_name, field_type) \
    Entelechy::FieldDesc{ #field_name, #field_type, offsetof(struct_type, field_name), sizeof(struct_type::field_name), Entelechy::TypeKind::Atom, {} }

#define REG_COMPOSITE_FIELD(struct_type, field_name, field_type) \
    Entelechy::FieldDesc{ #field_name, #field_type, offsetof(struct_type, field_name), sizeof(struct_type::field_name), Entelechy::TypeKind::Composite, #field_type }

#define REFLECT_COMPONENT(Name, ...) \
    namespace { \
    struct _AutoReg_##Name { \
        _AutoReg_##Name() { \
            Entelechy::ComponentTypeID id = Entelechy::TypeRegistry::instance().getOrAllocateTypeID<Name>(); \
            u32 mask = (1u << id); \
            Entelechy::TypeRegistry::instance().registerComponent(id, mask, Entelechy::ComponentDesc{ \
                #Name, sizeof(Name), { __VA_ARGS__ } \
            }); \
        } \
    } _auto_reg_##Name##_instance; \
    }

} // namespace Entelechy
