#pragma once
#include "foundation_types.h"
#include "small_string.h"
#include "string_format.h"
#include "hash_map.h"
#include <vector>
#include <cstddef>

namespace Entelechy {

using ComponentTypeID = u32;
constexpr ComponentTypeID INVALID_COMPONENT_TYPE_ID = 0xFFFFFFFFu;

struct FieldDesc {
    SmallString name;
    SmallString type;
    usize offset = 0;
    usize size = 0;
};

struct ComponentDesc {
    SmallString name;
    usize size = 0;
    std::vector<FieldDesc> fields;
};

class TypeRegistry {
public:
    static TypeRegistry& instance();

    template<typename T>
    ComponentTypeID getTypeID() {
        static ComponentTypeID id = allocateNextID();
        return id;
    }

    template<typename T>
    u32 getMask() {
        static u32 mask = (1u << getTypeID<T>());
        return mask;
    }

    void registerComponent(ComponentTypeID id, u32 mask, const ComponentDesc& desc);
    const ComponentDesc* findComponent(ComponentTypeID id) const;
    const ComponentDesc* findComponent(const SmallString& name) const;
    ComponentTypeID findComponentID(const SmallString& name) const;
    u32 getComponentMask(const SmallString& name) const;

    SmallString listComponents() const;
    SmallString describeComponent(const SmallString& name) const;
    usize componentCount() const;

private:
    TypeRegistry() = default;
    ComponentTypeID allocateNextID() {
        CHECK(m_nextID < 32 && "Exceeded maximum 32 component types");
        return m_nextID++;
    }

    HashMap<ComponentTypeID, ComponentDesc> m_components;
    HashMap<SmallString, ComponentTypeID> m_nameToID;
    HashMap<ComponentTypeID, SmallString> m_idToName;
    HashMap<SmallString, u32> m_nameToMask;
    ComponentTypeID m_nextID = 0;
};

// Helper macro for registering fields
#define REG_FIELD(struct_type, field_name, field_type) \
    Entelechy::FieldDesc{ #field_name, #field_type, offsetof(struct_type, field_name), sizeof(struct_type::field_name) }

// Helper macro for auto-registering a component to TypeRegistry
// Usage:
//   struct MyComp { float x; };
//   REFLECT_COMPONENT(MyComp, REG_FIELD(MyComp, x, float))
#define REFLECT_COMPONENT(Name, ...) \
    namespace { \
    struct _AutoReg_##Name { \
        _AutoReg_##Name() { \
            Entelechy::ComponentTypeID id = Entelechy::TypeRegistry::instance().getTypeID<Name>(); \
            u32 mask = (1u << id); \
            Entelechy::TypeRegistry::instance().registerComponent(id, mask, Entelechy::ComponentDesc{ \
                #Name, sizeof(Name), { __VA_ARGS__ } \
            }); \
        } \
    } _auto_reg_##Name##_instance; \
    }

} // namespace Entelechy
