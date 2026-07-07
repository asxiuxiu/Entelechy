#pragma once
#include "core/foundation_types.h"
#include "core/string/string_id.h"
#include "core/string/string.h"
#include "core/string/string_intern_pool.h"
#include "core/container/hash_map.h"
#include <functional>

namespace Entelechy
{

struct ToolDesc
{
    StringId name;
    String description;
    String inputSchema;
    bool isReadOnly = false;
    std::function<String(const String &json_args)> call;
};

class ToolRegistry
{
public:
    static ToolRegistry &instance();

    void registerTool(ToolDesc desc);
    const ToolDesc *findTool(StringId name) const;
    String listTools() const;
    String describeTool(StringId name) const;
    String callTool(StringId name, const String &json_args) const;
    usize toolCount() const;

private:
    ToolRegistry() = default;
    HashMap<StringId, ToolDesc> m_tools;
};

// Helper macro for auto-registering a tool to ToolRegistry
// Usage:
//   REFLECT_TOOL(my_tool_name, "description", "{}", true, [](const String&) -> String { return "{}"; })
#define REFLECT_TOOL(Name, Desc, Schema, ReadOnly, Func)                                                               \
    namespace                                                                                                          \
    {                                                                                                                  \
    struct _AutoToolReg_##Name                                                                                         \
    {                                                                                                                  \
        _AutoToolReg_##Name()                                                                                          \
        {                                                                                                              \
            Entelechy::ToolRegistry::instance().registerTool(Entelechy::ToolDesc{                                      \
                Entelechy::StringInternPool::instance().intern(#Name), Desc, Schema, ReadOnly, Func});                 \
        }                                                                                                              \
    } _auto_tool_reg_##Name##_instance;                                                                                \
    }

} // namespace Entelechy
