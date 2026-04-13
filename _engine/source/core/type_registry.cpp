#include "type_registry.h"
#include <cstdio>

namespace Entelechy {

TypeRegistry& TypeRegistry::instance() {
    static TypeRegistry reg;
    return reg;
}

void TypeRegistry::registerComponent(const ComponentDesc& desc) {
    m_components[desc.name] = desc;
}

const ComponentDesc* TypeRegistry::findComponent(const std::string& name) const {
    auto it = m_components.find(name);
    if (it != m_components.end()) {
        return &it->second;
    }
    return nullptr;
}

std::string TypeRegistry::listComponents() const {
    std::string json = "[\n";
    bool first = true;
    for (const auto& pair : m_components) {
        if (!first) json += ",\n";
        first = false;
        json += "  \"" + pair.first + "\"";
    }
    json += "\n]";
    return json;
}

size_t TypeRegistry::componentCount() const {
    return m_components.size();
}

std::string TypeRegistry::describeComponent(const std::string& name) const {
    const ComponentDesc* desc = findComponent(name);
    if (!desc) {
        return "{\"error\":\"component not found\"}";
    }

    std::string json = "{\n";
    json += "  \"name\": \"" + desc->name + "\",\n";
    json += "  \"size\": " + std::to_string(desc->size) + ",\n";
    json += "  \"fields\": [\n";

    for (size_t i = 0; i < desc->fields.size(); ++i) {
        const auto& f = desc->fields[i];
        json += "    {\n";
        json += "      \"name\": \"" + f.name + "\",\n";
        json += "      \"type\": \"" + f.type + "\",\n";
        json += "      \"offset\": " + std::to_string(f.offset) + ",\n";
        json += "      \"size\": " + std::to_string(f.size) + "\n";
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
