#pragma once
#include "small_string.h"
#include "string_format.h"
#include <vector>
#include <unordered_map>
#include <cstddef>
#include <cstdint>

namespace Entelechy {

using ComponentTypeID = uint32_t;
constexpr ComponentTypeID INVALID_COMPONENT_TYPE_ID = 0xFFFFFFFFu;

struct FieldDesc {
    SmallString name;
    SmallString type;
    size_t offset = 0;
    size_t size = 0;
};

struct ComponentDesc {
    SmallString name;
    size_t size = 0;
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
    uint32_t getMask() {
        static uint32_t mask = (1u << getTypeID<T>());
        return mask;
    }

    void registerComponent(ComponentTypeID id, uint32_t mask, const ComponentDesc& desc);
    const ComponentDesc* findComponent(ComponentTypeID id) const;
    const ComponentDesc* findComponent(const SmallString& name) const;
    ComponentTypeID findComponentID(const SmallString& name) const;
    uint32_t getComponentMask(const SmallString& name) const;

    SmallString listComponents() const;
    SmallString describeComponent(const SmallString& name) const;
    size_t componentCount() const;

private:
    TypeRegistry() = default;
    ComponentTypeID allocateNextID() { return m_nextID++; }

    std::unordered_map<ComponentTypeID, ComponentDesc> m_components;
    std::unordered_map<SmallString, ComponentTypeID> m_nameToID;
    std::unordered_map<ComponentTypeID, SmallString> m_idToName;
    std::unordered_map<SmallString, uint32_t> m_nameToMask;
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
            uint32_t mask = (1u << id); \
            Entelechy::TypeRegistry::instance().registerComponent(id, mask, Entelechy::ComponentDesc{ \
                #Name, sizeof(Name), { __VA_ARGS__ } \
            }); \
        } \
    } _auto_reg_##Name##_instance; \
    }

} // namespace Entelechy
