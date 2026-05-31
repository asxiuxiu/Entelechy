#include "bridge/tool_registry.h"
#include "ecs/type/type_registry.h"
#include <cstdio>
#include <cstring>

namespace Entelechy {

namespace {
    bool jsonExtractString(const SmallString& json, const SmallString& key, SmallString& out) {
        SmallString pattern = SmallString("\"") + key + "\"";
        usize pos = json.find(pattern.c_str());
        if (pos == SmallString::npos) return false;
        pos = json.find(':', pos + pattern.size());
        if (pos == SmallString::npos) return false;
        ++pos;
        while (pos < json.size() && (json.c_str()[pos] == ' ' || json.c_str()[pos] == '\t' || json.c_str()[pos] == '"')) ++pos;
        usize end = pos;
        while (end < json.size() && json.c_str()[end] != '"' && json.c_str()[end] != ',' && json.c_str()[end] != '}') ++end;
        out = json.substr(pos, end - pos);
        return true;
    }
}

// Auto-registered tools via REFLECT_TOOL macro
REFLECT_TOOL(listTools,
    "List all registered tools",
    "{}",
    true,
    [](const SmallString&) -> SmallString {
        return Entelechy::ToolRegistry::instance().listTools();
    }
)

REFLECT_TOOL(describeTool,
    "Describe a tool by name",
    "{\"name\": \"string\"}",
    true,
    [](const SmallString& json) -> SmallString {
        SmallString name;
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
    [](const SmallString&) -> SmallString {
        return Entelechy::TypeRegistry::instance().listComponents().c_str();
    }
)

REFLECT_TOOL(describeComponent,
    "Describe a component type by name",
    "{\"name\": \"string\"}",
    true,
    [](const SmallString& json) -> SmallString {
        SmallString name;
        if (!jsonExtractString(json, "name", name)) {
            return "{\"error\":\"missing name\"}";
        }
        return Entelechy::TypeRegistry::instance().describeComponent(name.c_str()).c_str();
    }
)

ToolRegistry& ToolRegistry::instance() {
    static ToolRegistry reg;
    return reg;
}

void ToolRegistry::registerTool(ToolDesc desc) {
    m_tools.insert(desc.name, std::move(desc));
}

const ToolDesc* ToolRegistry::findTool(const SmallString& name) const {
    auto* v = m_tools.find(name);
    return v ? v : nullptr;
}

usize ToolRegistry::toolCount() const {
    return m_tools.size();
}

SmallString ToolRegistry::listTools() const {
    SmallString json = "[\n";
    bool first = true;
    for (const auto& kv : m_tools) {
        if (!first) json += ",\n";
        first = false;
        json += "  {\n";
        json += "    \"name\": \"" + kv.first + "\",\n";
        json += "    \"description\": \"" + kv.second.description + "\",\n";
        json += "    \"isReadOnly\": " + SmallString(kv.second.isReadOnly ? "true" : "false") + "\n";
        json += "  }";
    }
    json += "\n]";
    return json;
}

SmallString ToolRegistry::describeTool(const SmallString& name) const {
    const ToolDesc* desc = findTool(name);
    if (!desc) {
        return "{\"error\":\"tool not found\"}";
    }

    SmallString json = "{\n";
    json += "  \"name\": \"" + desc->name + "\",\n";
    json += "  \"description\": \"" + desc->description + "\",\n";
    json += "  \"inputSchema\": \"" + desc->inputSchema + "\",\n";
    json += "  \"isReadOnly\": " + SmallString(desc->isReadOnly ? "true" : "false") + "\n";
    json += "}";
    return json;
}

SmallString ToolRegistry::callTool(const SmallString& name, const SmallString& json_args) const {
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
