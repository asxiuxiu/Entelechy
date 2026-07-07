#include "bridge/tool_registry.h"
#include "ecs/type/type_registry.h"
#include "core/string/string_intern_pool.h"
#include <cstdio>
#include <cstring>

namespace Entelechy
{

namespace
{
bool jsonExtractString(const String &json, const String &key, String &out)
{
    String pattern = String("\"") + key + "\"";
    usize pos = json.find(pattern.c_str());
    if (pos == String::npos)
        return false;
    pos = json.find(':', pos + pattern.size());
    if (pos == String::npos)
        return false;
    ++pos;
    while (pos < json.size() && (json.c_str()[pos] == ' ' || json.c_str()[pos] == '\t' || json.c_str()[pos] == '"'))
        ++pos;
    usize end = pos;
    while (end < json.size() && json.c_str()[end] != '"' && json.c_str()[end] != ',' && json.c_str()[end] != '}')
        ++end;
    out = json.substr(pos, end - pos);
    return true;
}
} // namespace

// Auto-registered tools via REFLECT_TOOL macro
REFLECT_TOOL(listTools, "List all registered tools", "{}", true,
             [](const String &) -> String { return Entelechy::ToolRegistry::instance().listTools(); })

REFLECT_TOOL(describeTool, "Describe a tool by name", "{\"name\": \"string\"}", true,
             [](const String &json) -> String
             {
                 String name;
                 if (!jsonExtractString(json, "name", name))
                 {
                     return "{\"error\":\"missing name\"}";
                 }
                 return Entelechy::ToolRegistry::instance().describeTool(
                     Entelechy::StringInternPool::instance().intern(name.c_str()));
             })

REFLECT_TOOL(listComponents, "List all registered component types", "{}", true,
             [](const String &) -> String { return Entelechy::TypeRegistry::instance().listComponents().c_str(); })

REFLECT_TOOL(describeComponent, "Describe a component type by name", "{\"name\": \"string\"}", true,
             [](const String &json) -> String
             {
                 String name;
                 if (!jsonExtractString(json, "name", name))
                 {
                     return "{\"error\":\"missing name\"}";
                 }
                 return Entelechy::TypeRegistry::instance()
                     .describeComponent(Entelechy::StringInternPool::instance().intern(name.c_str()))
                     .c_str();
             })

ToolRegistry &ToolRegistry::instance()
{
    static ToolRegistry reg;
    return reg;
}

void ToolRegistry::registerTool(ToolDesc desc)
{
    m_tools.insert(desc.name, std::move(desc));
}

const ToolDesc *ToolRegistry::findTool(StringId name) const
{
    auto *v = m_tools.find(name);
    return v ? v : nullptr;
}

usize ToolRegistry::toolCount() const
{
    return m_tools.size();
}

String ToolRegistry::listTools() const
{
    String json = "[\n";
    bool first = true;
    for (const auto &kv : m_tools)
    {
        if (!first)
            json += ",\n";
        first = false;
        json += "  {\n";
        const char *nameResolved = StringInternPool::instance().resolve(kv.first);
        json += "    \"name\": \"" + String(nameResolved ? nameResolved : "") + "\",\n";
        json += "    \"description\": \"" + kv.second.description + "\",\n";
        json += "    \"isReadOnly\": " + String(kv.second.isReadOnly ? "true" : "false") + "\n";
        json += "  }";
    }
    json += "\n]";
    return json;
}

String ToolRegistry::describeTool(StringId name) const
{
    const ToolDesc *desc = findTool(name);
    if (!desc)
    {
        return "{\"error\":\"tool not found\"}";
    }

    String json = "{\n";
    const char *nameResolved = StringInternPool::instance().resolve(desc->name);
    json += "  \"name\": \"" + String(nameResolved ? nameResolved : "") + "\",\n";
    json += "  \"description\": \"" + desc->description + "\",\n";
    json += "  \"inputSchema\": \"" + desc->inputSchema + "\",\n";
    json += "  \"isReadOnly\": " + String(desc->isReadOnly ? "true" : "false") + "\n";
    json += "}";
    return json;
}

String ToolRegistry::callTool(StringId name, const String &json_args) const
{
    const ToolDesc *desc = findTool(name);
    if (!desc)
    {
        return "{\"error\":\"tool not found\"}";
    }
    if (desc->call)
    {
        return desc->call(json_args);
    }
    return "{\"error\":\"tool has no implementation\"}";
}

} // namespace Entelechy
