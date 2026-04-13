#include "tool_registry.h"
#include "type_registry.h"
#include <cstdio>
#include <string>

namespace Entelechy {

namespace {
    bool jsonExtractString(const std::string& json, const std::string& key, std::string& out) {
        std::string pattern = "\"" + key + "\"";
        size_t pos = json.find(pattern);
        if (pos == std::string::npos) return false;
        pos = json.find(':', pos + pattern.length());
        if (pos == std::string::npos) return false;
        ++pos;
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '"')) ++pos;
        size_t end = pos;
        while (end < json.size() && json[end] != '"' && json[end] != ',' && json[end] != '}') ++end;
        out = json.substr(pos, end - pos);
        return true;
    }
}

// Auto-registered tools via REFLECT_TOOL macro
REFLECT_TOOL(listTools,
    "List all registered tools",
    "{}",
    true,
    [](const std::string&) -> std::string {
        return Entelechy::ToolRegistry::instance().listTools();
    }
)

REFLECT_TOOL(describeTool,
    "Describe a tool by name",
    "{\"name\": \"string\"}",
    true,
    [](const std::string& json) -> std::string {
        std::string name;
        if (!jsonExtractString(json, "name", name)) {
            return "{\"error\":\"missing name\"}";
        }
        return Entelechy::ToolRegistry::instance().describeTool(name);
    }
)

REFLECT_TOOL(listComponents,
    "List all registered component types",
    "{}",
    true,
    [](const std::string&) -> std::string {
        return Entelechy::TypeRegistry::instance().listComponents();
    }
)

REFLECT_TOOL(describeComponent,
    "Describe a component type by name",
    "{\"name\": \"string\"}",
    true,
    [](const std::string& json) -> std::string {
        std::string name;
        if (!jsonExtractString(json, "name", name)) {
            return "{\"error\":\"missing name\"}";
        }
        return Entelechy::TypeRegistry::instance().describeComponent(name);
    }
)

ToolRegistry& ToolRegistry::instance() {
    static ToolRegistry reg;
    return reg;
}

void ToolRegistry::registerTool(ToolDesc desc) {
    m_tools[desc.name] = std::move(desc);
}

const ToolDesc* ToolRegistry::findTool(const std::string& name) const {
    auto it = m_tools.find(name);
    if (it != m_tools.end()) {
        return &it->second;
    }
    return nullptr;
}

size_t ToolRegistry::toolCount() const {
    return m_tools.size();
}

std::string ToolRegistry::listTools() const {
    std::string json = "[\n";
    bool first = true;
    for (const auto& pair : m_tools) {
        if (!first) json += ",\n";
        first = false;
        json += "  {\n";
        json += "    \"name\": \"" + pair.first + "\",\n";
        json += "    \"description\": \"" + pair.second.description + "\",\n";
        json += "    \"isReadOnly\": " + std::string(pair.second.isReadOnly ? "true" : "false") + "\n";
        json += "  }";
    }
    json += "\n]";
    return json;
}

std::string ToolRegistry::describeTool(const std::string& name) const {
    const ToolDesc* desc = findTool(name);
    if (!desc) {
        return "{\"error\":\"tool not found\"}";
    }

    std::string json = "{\n";
    json += "  \"name\": \"" + desc->name + "\",\n";
    json += "  \"description\": \"" + desc->description + "\",\n";
    json += "  \"inputSchema\": \"" + desc->inputSchema + "\",\n";
    json += "  \"isReadOnly\": " + std::string(desc->isReadOnly ? "true" : "false") + "\n";
    json += "}";
    return json;
}

std::string ToolRegistry::callTool(const std::string& name, const std::string& json_args) const {
    const ToolDesc* desc = findTool(name);
    if (!desc) {
        return "{\"error\":\"tool not found\"}";
    }
    if (desc->call) {
        return desc->call(json_args);
    }
    return "{\"error\":\"tool has no implementation\"}";
}

} // namespace Entelechy
