#pragma once
#include "core/foundation_types.h"
#include "core/string/string.h"
#include "core/string/string_format.h"
#include "core/string/string_id.h"
#include "core/container/hash_map.h"
#include "core/container/dynamic_array.h"
#include <cstddef>
#include <type_traits>
#include <utility>

namespace Entelechy
{

using ComponentTypeID = u32;
constexpr ComponentTypeID INVALID_COMPONENT_TYPE_ID = 0xFFFFFFFFu;

// ------------------------------------------------------------------
// TypeKind — classification of field / type shape
// ------------------------------------------------------------------
enum class TypeKind
{
    Atom,
    Composite,
    Array
};

// ------------------------------------------------------------------
// FieldDesc — one field inside a composite type
// ------------------------------------------------------------------
struct FieldDesc
{
    StringId name;
    StringId type; // original type name (e.g. "Vec3"_sid, "f32"_sid)
    usize offset = 0;
    usize size = 0;
    TypeKind kind = TypeKind::Atom;
    StringId subtype; // when kind == Composite: name of the composite type in TypeRegistry
};

// ------------------------------------------------------------------
// TypeDesc — runtime description of any type (component or composite)
// ------------------------------------------------------------------
struct TypeDesc
{
    StringId name;
    usize size = 0;
    usize alignment = 0;
    TypeKind kind = TypeKind::Composite;
    DynamicArray<FieldDesc> fields;
};

using ComponentDtor = void (*)(void *);
using ComponentMoveCtor = void (*)(void *dest, void *src);
using ComponentCopyCtor = void (*)(void *dest, const void *src);

// ------------------------------------------------------------------
// ComponentDesc — runtime description of an ECS component type
// ------------------------------------------------------------------
struct ComponentDesc
{
    StringId name;
    usize size = 0;
    usize alignment = 0;
    ComponentDtor dtor = nullptr;
    ComponentMoveCtor moveCtor = nullptr;
    ComponentCopyCtor copyCtor = nullptr;
    bool isTriviallyCopyable = true;
    DynamicArray<FieldDesc> fields;
};

template <typename T>
ComponentDesc makeComponentDesc(StringId name, DynamicArray<FieldDesc> fields)
{
    ComponentDesc desc;
    desc.name = name;
    desc.size = sizeof(T);
    desc.alignment = alignof(T);
    desc.isTriviallyCopyable = std::is_trivially_copyable_v<T>;
    desc.fields = fields;

    if constexpr (!std::is_trivially_destructible_v<T>)
    {
        desc.dtor = [](void *p)
        {
            static_cast<T *>(p)->~T();
        };
    }

    if constexpr (!std::is_trivially_copyable_v<T>)
    {
        if constexpr (std::is_move_constructible_v<T>)
        {
            desc.moveCtor = [](void *dest, void *src)
            {
                new (dest) T(std::move(*static_cast<T *>(src)));
            };
        }
        if constexpr (std::is_copy_constructible_v<T>)
        {
            desc.copyCtor = [](void *dest, const void *src)
            {
                new (dest) T(*static_cast<const T *>(src));
            };
        }
    }
    return desc;
}

// Cross-translation-unit type ID storage (C++17 inline static).
template <typename T>
struct TypeIDStorage
{
    static inline ComponentTypeID s_id = INVALID_COMPONENT_TYPE_ID;
};

// ------------------------------------------------------------------
// TypeRegistry — ECS component IDs + general type descriptions
// ------------------------------------------------------------------
class TypeRegistry
{
public:
    static TypeRegistry &instance();

    // ----- ECS component type IDs (unchanged) -----
    template <typename T>
    ComponentTypeID getOrAllocateTypeID()
    {
        ComponentTypeID &id = TypeIDStorage<T>::s_id;
        if (id == INVALID_COMPONENT_TYPE_ID)
        {
            id = allocateNextID();
        }
        return id;
    }

    template <typename T>
    [[nodiscard]] ComponentTypeID getTypeID() const
    {
        return TypeIDStorage<T>::s_id;
    }

    template <typename T>
    [[nodiscard]] u64 getMask() const
    {
        ComponentTypeID id = getTypeID<T>();
        CHECK(id != INVALID_COMPONENT_TYPE_ID && "Component type not registered");
        return (1ull << id);
    }

    void registerComponent(ComponentTypeID id, u64 mask, const ComponentDesc &desc);
    const ComponentDesc *findComponent(ComponentTypeID id) const;
    const ComponentDesc *findComponent(StringId name) const;
    ComponentTypeID findComponentID(StringId name) const;
    u64 getComponentMask(StringId name) const;

    // ----- General type descriptions (new) -----
    void registerType(const TypeDesc &desc);
    const TypeDesc *findType(StringId name) const;

    String listComponents() const;
    String describeComponent(StringId name) const;
    usize componentCount() const;

    void registerBuiltinTypes();

private:
    TypeRegistry() = default;
    ComponentTypeID allocateNextID()
    {
        CHECK(m_next_id < 64 && "Exceeded maximum 64 component types");
        return m_next_id++;
    }

    // ECS component tables
    HashMap<ComponentTypeID, ComponentDesc> m_components;
    HashMap<StringId, ComponentTypeID> m_name_to_id;
    HashMap<ComponentTypeID, StringId> m_id_to_name;
    HashMap<StringId, u64> m_name_to_mask;
    ComponentTypeID m_next_id = 0;

    // General type table
    HashMap<StringId, TypeDesc> m_types;
};

// ------------------------------------------------------------------
// Helper macros
// ------------------------------------------------------------------
#define REG_FIELD(struct_type, field_name, field_type)                                                                 \
    Entelechy::FieldDesc                                                                                               \
    {                                                                                                                  \
        #field_name##_sid, #field_type##_sid, offsetof(struct_type, field_name), sizeof(struct_type::field_name),      \
            Entelechy::TypeKind::Atom,                                                                                 \
        {                                                                                                              \
        }                                                                                                              \
    }

#define REG_COMPOSITE_FIELD(struct_type, field_name, field_type)                                                       \
    Entelechy::FieldDesc                                                                                               \
    {                                                                                                                  \
        #field_name##_sid, #field_type##_sid, offsetof(struct_type, field_name), sizeof(struct_type::field_name),      \
            Entelechy::TypeKind::Composite, #field_type##_sid                                                          \
    }

#define REFLECT_COMPONENT(Name, ...)                                                                                   \
    namespace                                                                                                          \
    {                                                                                                                  \
    struct _AutoReg_##Name                                                                                             \
    {                                                                                                                  \
        _AutoReg_##Name()                                                                                              \
        {                                                                                                              \
            Entelechy::ComponentTypeID id = Entelechy::TypeRegistry::instance().getOrAllocateTypeID<Name>();           \
            u64 mask = (1ull << id);                                                                                   \
            Entelechy::TypeRegistry::instance().registerComponent(                                                     \
                id, mask, Entelechy::makeComponentDesc<Name>(#Name##_sid, {__VA_ARGS__}));                             \
        }                                                                                                              \
    } _auto_reg_##Name##_instance;                                                                                     \
    }

} // namespace Entelechy
