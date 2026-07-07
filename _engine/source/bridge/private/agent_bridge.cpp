#include "bridge/agent_bridge.h"
#include "ecs/type/type_registry.h"
#include "log/core/log_macros.h"
#include "core/string/string_intern_pool.h"
#include <cstdio>
#include "core/string/string_format.h"
#include <cstring>

namespace Entelechy
{

namespace
{
inline String toString(u32 v)
{
    char buf[32];
    toStringBuf(buf, sizeof(buf), v);
    return String(buf);
}
inline String toString(usize v)
{
    char buf[32];
    toStringBuf(buf, sizeof(buf), static_cast<u64>(v));
    return String(buf);
}
inline String toString(f32 v)
{
    char buf[32];
    toStringBuf(buf, sizeof(buf), v);
    return String(buf);
}

bool jsonHasKey(const String &json, const String &key)
{
    String pattern = "\"" + key + "\"";
    return json.find(pattern) != String::npos;
}

bool jsonParseFloat(const String &json, const String &key, f32 &out)
{
    String pattern = "\"" + key + "\"";
    usize pos = json.find(pattern);
    if (pos == String::npos)
        return false;

    pos = json.find(':', pos + pattern.length());
    if (pos == String::npos)
        return false;

    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '"'))
        ++pos;

    const char *str = json.c_str() + pos;
    char *end = nullptr;
    f32 val = std::strtof(str, &end);
    if (end == str)
        return false;

    out = val;
    return true;
}

bool jsonParseUint32(const String &json, const String &key, u32 &out)
{
    String pattern = "\"" + key + "\"";
    usize pos = json.find(pattern);
    if (pos == String::npos)
        return false;

    pos = json.find(':', pos + pattern.length());
    if (pos == String::npos)
        return false;

    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t'))
        ++pos;

    const char *str = json.c_str() + pos;
    char *end = nullptr;
    unsigned long val = std::strtoul(str, &end, 10);
    if (end == str)
        return false;

    out = static_cast<u32>(val);
    return true;
}

bool jsonParseString(const String &json, const String &key, String &out)
{
    String pattern = "\"" + key + "\"";
    usize pos = json.find(pattern);
    if (pos == String::npos)
        return false;

    pos = json.find(':', pos + pattern.length());
    if (pos == String::npos)
        return false;

    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t'))
        ++pos;
    if (pos >= json.size() || json[pos] != '"')
        return false;
    ++pos;
    usize end = pos;
    while (end < json.size() && json[end] != '"')
        ++end;
    out = json.substr(pos, end - pos);
    return true;
}
} // namespace

void initBridge()
{
    LOG_INFO(LogCategories::kEngine, "Bridge initialized");
}

void AgentBridge::init()
{
    m_scheduler.registerSystem(&m_movement_system);

    AgentBridge *self = this;

    ToolRegistry::instance().registerTool(ToolDesc{
        StringInternPool::instance().intern("queryEntities"), "Query all alive entities that have a given component",
        "{\"comp_name\": \"string\"}", true, [self](const String &json_args) -> String
        {
            String comp_name;
            usize pos = json_args.find("\"comp_name\"");
            if (pos != String::npos)
            {
                pos = json_args.find(':', pos + 11);
                if (pos != String::npos)
                {
                    ++pos;
                    while (pos < json_args.size() &&
                           (json_args[pos] == ' ' || json_args[pos] == '\t' || json_args[pos] == '"'))
                        ++pos;
                    usize end = pos;
                    while (end < json_args.size() && json_args[end] != '"' && json_args[end] != ',' &&
                           json_args[end] != '}')
                        ++end;
                    comp_name = json_args.substr(pos, end - pos);
                }
            }
            return self->queryEntities(comp_name);
        }});

    ToolRegistry::instance().registerTool(
        ToolDesc{StringInternPool::instance().intern("getComponent"), "Get component data for a specific entity",
                 "{\"entity\": uint32, \"comp_name\": \"string\"}", true, [self](const String &json_args) -> String
                 {
                     u32 entity_id = 0;
                     String comp_name;
                     jsonParseUint32(json_args, "entity", entity_id);
                     usize pos = json_args.find("\"comp_name\"");
                     if (pos != String::npos)
                     {
                         pos = json_args.find(':', pos + 11);
                         if (pos != String::npos)
                         {
                             ++pos;
                             while (pos < json_args.size() &&
                                    (json_args[pos] == ' ' || json_args[pos] == '\t' || json_args[pos] == '"'))
                                 ++pos;
                             usize end = pos;
                             while (end < json_args.size() && json_args[end] != '"' && json_args[end] != ',' &&
                                    json_args[end] != '}')
                                 ++end;
                             comp_name = json_args.substr(pos, end - pos);
                         }
                     }
                     Entity e{entity_id, self->m_world.getEntityGeneration(entity_id)};
                     return self->getComponent(e, comp_name);
                 }});

    ToolRegistry::instance().registerTool(
        ToolDesc{StringInternPool::instance().intern("setComponent"), "Set component data for a specific entity",
                 "{\"entity\": uint32, \"comp_name\": \"string\", \"values\": {...}}", false,
                 [self](const String &json_args) -> String
                 {
                     u32 entity_id = 0;
                     String comp_name;
                     String values_json;
                     jsonParseUint32(json_args, "entity", entity_id);
                     usize pos = json_args.find("\"comp_name\"");
                     if (pos != String::npos)
                     {
                         pos = json_args.find(':', pos + 11);
                         if (pos != String::npos)
                         {
                             ++pos;
                             while (pos < json_args.size() &&
                                    (json_args[pos] == ' ' || json_args[pos] == '\t' || json_args[pos] == '"'))
                                 ++pos;
                             usize end = pos;
                             while (end < json_args.size() && json_args[end] != '"' && json_args[end] != ',' &&
                                    json_args[end] != '}')
                                 ++end;
                             comp_name = json_args.substr(pos, end - pos);
                         }
                     }
                     pos = json_args.find("\"values\"");
                     if (pos != String::npos)
                     {
                         pos = json_args.find(':', pos + 8);
                         if (pos != String::npos)
                         {
                             ++pos;
                             while (pos < json_args.size() && (json_args[pos] == ' ' || json_args[pos] == '\t'))
                                 ++pos;
                             if (pos < json_args.size() && json_args[pos] == '{')
                             {
                                 usize brace = 1;
                                 usize end = pos + 1;
                                 while (end < json_args.size() && brace > 0)
                                 {
                                     if (json_args[end] == '{')
                                         ++brace;
                                     else if (json_args[end] == '}')
                                         --brace;
                                     ++end;
                                 }
                                 values_json = json_args.substr(pos, end - pos);
                             }
                         }
                     }
                     Entity e{entity_id, self->m_world.getEntityGeneration(entity_id)};
                     return self->setComponent(e, comp_name, values_json);
                 }});

    ToolRegistry::instance().registerTool(ToolDesc{StringInternPool::instance().intern("stepWorld"),
                                                   "Advance the simulation by one frame with given dt",
                                                   "{\"dt\": float}", false, [self](const String &json_args) -> String
                                                   {
                                                       f32 dt = 1.0f;
                                                       jsonParseFloat(json_args, "dt", dt);
                                                       self->step(dt);
                                                       return "{\"ok\":true}";
                                                   }});

    ToolRegistry::instance().registerTool(ToolDesc{StringInternPool::instance().intern("queryEntitiesByMask"),
                                                   "Query all alive entities that match a component mask",
                                                   "{\"mask\": uint32}", true, [self](const String &json_args) -> String
                                                   {
                                                       u32 mask = 0;
                                                       jsonParseUint32(json_args, "mask", mask);
                                                       return self->queryEntitiesByMask(mask);
                                                   }});

    ToolRegistry::instance().registerTool(ToolDesc{StringInternPool::instance().intern("getWorldSummary"),
                                                   "Get a summary of the current world state", "{}", true,
                                                   [self](const String &json_args) -> String
                                                   {
                                                       (void)json_args;
                                                       return self->getWorldSummary();
                                                   }});
}

void AgentBridge::step(f32 dt)
{
    m_scheduler.tick(m_world, dt);
}

String AgentBridge::queryEntities(const String &comp_name) const
{
    u32 mask = TypeRegistry::instance().getComponentMask(StringInternPool::instance().intern(comp_name.c_str()));
    if (mask == 0)
    {
        return "{\"error\":\"component not found\"}";
    }
    return queryEntitiesByMask(mask);
}

String AgentBridge::queryEntitiesByMask(u64 mask) const
{
    String json = "[";
    bool first = true;
    for (u32 id = 0; id < m_world.maxEntityID(); ++id)
    {
        Entity e{id, m_world.getEntityGeneration(id)};
        if (m_world.valid(e) && (m_world.getEntityMask(id) & mask) == mask)
        {
            if (!first)
                json += ",";
            first = false;
            json += toString(id);
        }
    }
    json += "]";
    return json;
}

String AgentBridge::getWorldSummary() const
{
    String json = "{";
    json += "\"entityCount\":" + toString(m_world.entityCount()) + ",";
    json += "\"componentArrays\":[";
    bool first = true;
    for (const auto &pair : m_world.componentArrays())
    {
        if (!first)
            json += ",";
        first = false;
        const auto *desc = TypeRegistry::instance().findComponent(pair.first);
        json += "{";
        const char *nameResolved = desc ? StringInternPool::instance().resolve(desc->name) : nullptr;
        json += "\"name\":\"" + String(nameResolved ? nameResolved : "unknown") + "\",";
        json += "\"count\":" + toString(pair.second->count());
        json += "}";
    }
    json += "]";
    json += "}";
    return json;
}

String AgentBridge::getComponent(Entity e, const String &comp_name) const
{
    if (!m_world.valid(e))
    {
        return "{\"error\":\"invalid entity\"}";
    }

    ComponentTypeID typeID =
        TypeRegistry::instance().findComponentID(StringInternPool::instance().intern(comp_name.c_str()));
    if (typeID == INVALID_COMPONENT_TYPE_ID)
    {
        return "{\"error\":\"component not found\"}";
    }

    const auto &arrays = m_world.componentArrays();
    auto *array = arrays.find(typeID);
    if (!array || !(*array)->has(e))
    {
        return "{\"error\":\"component not found on entity\"}";
    }

    const void *comp_ptr = (*array)->getRaw(e);
    const auto *desc = TypeRegistry::instance().findComponent(typeID);
    if (!desc)
    {
        return "{\"error\":\"component descriptor missing\"}";
    }

    String json = "{";
    bool first = true;
    for (const auto &field : desc->fields)
    {
        if (!first)
            json += ",";
        first = false;
        const char *fieldNameResolved = StringInternPool::instance().resolve(field.name);
        json += "\"" + String(fieldNameResolved ? fieldNameResolved : "") + "\":";
        if (field.type == "float"_sid)
        {
            f32 val = *reinterpret_cast<const f32 *>(static_cast<const char *>(comp_ptr) + field.offset);
            json += toString(val);
        }
        else if (field.type == "String"_sid)
        {
            const String *str = reinterpret_cast<const String *>(static_cast<const char *>(comp_ptr) + field.offset);
            json += "\"" + String(str->c_str()) + "\"";
        }
        else if (field.type == "StringId"_sid)
        {
            const StringId *id = reinterpret_cast<const StringId *>(static_cast<const char *>(comp_ptr) + field.offset);
            const char *resolved = StringInternPool::instance().resolve(*id);
            json += "\"";
            json += (resolved ? resolved : "<unresolved>");
            json += "\"";
        }
        else
        {
            json += "null";
        }
    }
    json += "}";
    return json;
}

String AgentBridge::setComponent(Entity e, const String &comp_name, const String &json)
{
    if (!m_world.valid(e))
    {
        return "{\"error\":\"invalid entity\"}";
    }

    ComponentTypeID typeID =
        TypeRegistry::instance().findComponentID(StringInternPool::instance().intern(comp_name.c_str()));
    if (typeID == INVALID_COMPONENT_TYPE_ID)
    {
        return "{\"error\":\"component not found\"}";
    }

    const auto &arrays = m_world.componentArrays();
    auto *array = arrays.find(typeID);
    if (!array || !(*array)->has(e))
    {
        return "{\"error\":\"component not found on entity\"}";
    }

    void *comp_ptr = (*array)->getRaw(e);
    const auto *desc = TypeRegistry::instance().findComponent(typeID);
    if (!desc)
    {
        return "{\"error\":\"component descriptor missing\"}";
    }

    for (const auto &field : desc->fields)
    {
        if (field.type == "float"_sid)
        {
            f32 val = 0.0f;
            const char *fieldNameResolved = StringInternPool::instance().resolve(field.name);
            if (jsonParseFloat(json, fieldNameResolved ? fieldNameResolved : "", val))
            {
                *reinterpret_cast<f32 *>(static_cast<char *>(comp_ptr) + field.offset) = val;
            }
        }
        else if (field.type == "String"_sid)
        {
            String val;
            const char *fieldNameResolved = StringInternPool::instance().resolve(field.name);
            if (jsonParseString(json, fieldNameResolved ? fieldNameResolved : "", val))
            {
                *reinterpret_cast<String *>(static_cast<char *>(comp_ptr) + field.offset) = val.c_str();
            }
        }
        else if (field.type == "StringId"_sid)
        {
            String val;
            const char *fieldNameResolved = StringInternPool::instance().resolve(field.name);
            if (jsonParseString(json, fieldNameResolved ? fieldNameResolved : "", val))
            {
                StringId id = StringInternPool::instance().intern(val.c_str());
                *reinterpret_cast<StringId *>(static_cast<char *>(comp_ptr) + field.offset) = id;
            }
        }
    }

    return "{\"ok\":true}";
}

String AgentBridge::callTool(const String &name, const String &json_args) const
{
    return ToolRegistry::instance().callTool(StringInternPool::instance().intern(name.c_str()), json_args);
}

} // namespace Entelechy
