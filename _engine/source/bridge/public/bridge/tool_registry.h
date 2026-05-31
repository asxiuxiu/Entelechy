#pragma once
#include "core/foundation_types.h"
#include "core/string/small_string.h"
#include "core/container/hash_map.h"
#include <functional>

namespace Entelechy {

struct ToolDesc {
    SmallString name;
    SmallString description;
    SmallString inputSchema;
    bool isReadOnly = false;
    std::function<SmallString(const SmallString& json_args)> call;
};

class ToolRegistry {
public:
    static ToolRegistry& instance();

    void registerTool(ToolDesc desc);
    const ToolDesc* findTool(const SmallString& name) const;
    SmallString listTools() const;
    SmallString describeTool(const SmallString& name) const;
    SmallString callTool(const SmallString& name, const SmallString& json_args) const;
    usize toolCount() const;

private:
    ToolRegistry() = default;
    HashMap<SmallString, ToolDesc> m_tools;
};

// Helper macro for auto-registering a tool to ToolRegistry
// Usage:
//   REFLECT_TOOL(my_tool_name, "description", "{}", true, [](const SmallString&) -> SmallString { return "{}"; })
#define REFLECT_TOOL(Name, Desc, Schema, ReadOnly, Func) \
    namespace { \
    struct _AutoToolReg_##Name { \
        _AutoToolReg_##Name() { \
            Entelechy::ToolRegistry::instance().registerTool(Entelechy::ToolDesc{ \
                #Name, Desc, Schema, ReadOnly, Func \
            }); \
        } \
    } _auto_tool_reg_##Name##_instance; \
    }

} // namespace Entelechy
