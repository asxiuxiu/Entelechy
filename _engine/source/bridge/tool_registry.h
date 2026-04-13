#pragma once
#include <string>
#include <unordered_map>
#include <functional>

namespace Entelechy {

struct ToolDesc {
    std::string name;
    std::string description;
    std::string inputSchema;
    bool isReadOnly = false;
    std::function<std::string(const std::string& json_args)> call;
};

class ToolRegistry {
public:
    static ToolRegistry& instance();

    void registerTool(ToolDesc desc);
    const ToolDesc* findTool(const std::string& name) const;
    std::string listTools() const;
    std::string describeTool(const std::string& name) const;
    std::string callTool(const std::string& name, const std::string& json_args) const;
    size_t toolCount() const;

private:
    ToolRegistry() = default;
    std::unordered_map<std::string, ToolDesc> m_tools;
};

// Helper macro for auto-registering a tool to ToolRegistry
// Usage:
//   REFLECT_TOOL(my_tool_name, "description", "{}", true, [](const std::string&) -> std::string { return "{}"; })
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
