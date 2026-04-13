#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstddef>
#include <cstdint>

namespace Entelechy {

struct FieldDesc {
    std::string name;
    std::string type;
    size_t offset = 0;
    size_t size = 0;
};

struct ComponentDesc {
    std::string name;
    size_t size = 0;
    std::vector<FieldDesc> fields;
};

class TypeRegistry {
public:
    static TypeRegistry& instance();

    void registerComponent(const ComponentDesc& desc);
    const ComponentDesc* findComponent(const std::string& name) const;

    std::string listComponents() const;
    std::string describeComponent(const std::string& name) const;
    size_t componentCount() const;

private:
    TypeRegistry() = default;
    std::unordered_map<std::string, ComponentDesc> m_components;
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
            Entelechy::TypeRegistry::instance().registerComponent(Entelechy::ComponentDesc{ \
                #Name, sizeof(Name), { __VA_ARGS__ } \
            }); \
        } \
    } _auto_reg_##Name##_instance; \
    }

} // namespace Entelechy
