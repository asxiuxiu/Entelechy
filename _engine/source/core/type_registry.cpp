#include "type_registry.h"
#include <cstdio>

namespace Entelechy {

TypeRegistry& TypeRegistry::instance() {
    static TypeRegistry reg;
    return reg;
}

void TypeRegistry::registerComponent(ComponentTypeID id, u32 mask, const ComponentDesc& desc) {
    m_components.insert(id, desc);
    m_nameToID.insert(desc.name, id);
    m_idToName.insert(id, desc.name);
    m_nameToMask.insert(desc.name, mask);
}

const ComponentDesc* TypeRegistry::findComponent(ComponentTypeID id) const {
    auto* v = m_components.find(id);
    if (v) {
        return v;
    }
    return nullptr;
}

const ComponentDesc* TypeRegistry::findComponent(const SmallString& name) const {
    auto* id = m_nameToID.find(name);
    if (id) {
        return findComponent(*id);
    }
    return nullptr;
}

ComponentTypeID TypeRegistry::findComponentID(const SmallString& name) const {
    auto* v = m_nameToID.find(name);
    if (v) {
        return *v;
    }
    return INVALID_COMPONENT_TYPE_ID;
}

u32 TypeRegistry::getComponentMask(const SmallString& name) const {
    auto* v = m_nameToMask.find(name);
    if (v) {
        return *v;
    }
    return 0;
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

} // namespace Entelechy
