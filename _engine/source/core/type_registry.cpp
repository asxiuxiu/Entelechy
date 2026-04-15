#include "type_registry.h"
#include <cstdio>

namespace Entelechy {

TypeRegistry& TypeRegistry::instance() {
    static TypeRegistry reg;
    return reg;
}

void TypeRegistry::registerComponent(ComponentTypeID id, uint32_t mask, const ComponentDesc& desc) {
    m_components[id] = desc;
    m_nameToID[desc.name] = id;
    m_idToName[id] = desc.name;
    m_nameToMask[desc.name] = mask;
}

const ComponentDesc* TypeRegistry::findComponent(ComponentTypeID id) const {
    auto it = m_components.find(id);
    if (it != m_components.end()) {
        return &it->second;
    }
    return nullptr;
}

const ComponentDesc* TypeRegistry::findComponent(const SmallString& name) const {
    auto it = m_nameToID.find(name);
    if (it != m_nameToID.end()) {
        return findComponent(it->second);
    }
    return nullptr;
}

ComponentTypeID TypeRegistry::findComponentID(const SmallString& name) const {
    auto it = m_nameToID.find(name);
    if (it != m_nameToID.end()) {
        return it->second;
    }
    return INVALID_COMPONENT_TYPE_ID;
}

uint32_t TypeRegistry::getComponentMask(const SmallString& name) const {
    auto it = m_nameToMask.find(name);
    if (it != m_nameToMask.end()) {
        return it->second;
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

size_t TypeRegistry::componentCount() const {
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

    for (size_t i = 0; i < desc->fields.size(); ++i) {
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
