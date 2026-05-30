#include "agent_bridge.h"
#include "type_registry.h"
#include "log/log_macros.h"
#include "base/string_intern_pool.h"
#include <cstdio>
#include "base/string_format.h"
#include <cstring>

namespace Entelechy {

namespace {
    inline SmallString toSmallString(u32 v) {
        char buf[32];
        toStringBuf(buf, sizeof(buf), v);
        return SmallString(buf);
    }
    inline SmallString toSmallString(usize v) {
        char buf[32];
        toStringBuf(buf, sizeof(buf), static_cast<u64>(v));
        return SmallString(buf);
    }
    inline SmallString toSmallString(f32 v) {
        char buf[32];
        toStringBuf(buf, sizeof(buf), v);
        return SmallString(buf);
    }

    bool jsonHasKey(const SmallString& json, const SmallString& key) {
        SmallString pattern = "\"" + key + "\"";
        return json.find(pattern) != SmallString::npos;
    }

    bool jsonParseFloat(const SmallString& json, const SmallString& key, f32& out) {
        SmallString pattern = "\"" + key + "\"";
        usize pos = json.find(pattern);
        if (pos == SmallString::npos) return false;

        pos = json.find(':', pos + pattern.length());
        if (pos == SmallString::npos) return false;

        ++pos;
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '"')) ++pos;

        const char* str = json.c_str() + pos;
        char* end = nullptr;
        f32 val = std::strtof(str, &end);
        if (end == str) return false;

        out = val;
        return true;
    }

    bool jsonParseUint32(const SmallString& json, const SmallString& key, u32& out) {
        SmallString pattern = "\"" + key + "\"";
        usize pos = json.find(pattern);
        if (pos == SmallString::npos) return false;

        pos = json.find(':', pos + pattern.length());
        if (pos == SmallString::npos) return false;

        ++pos;
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;

        const char* str = json.c_str() + pos;
        char* end = nullptr;
        unsigned long val = std::strtoul(str, &end, 10);
        if (end == str) return false;

        out = static_cast<u32>(val);
        return true;
    }

    bool jsonParseString(const SmallString& json, const SmallString& key, SmallString& out) {
        SmallString pattern = "\"" + key + "\"";
        usize pos = json.find(pattern);
        if (pos == SmallString::npos) return false;

        pos = json.find(':', pos + pattern.length());
        if (pos == SmallString::npos) return false;

        ++pos;
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
        if (pos >= json.size() || json[pos] != '"') return false;
        ++pos;
        usize end = pos;
        while (end < json.size() && json[end] != '"') ++end;
        out = json.substr(pos, end - pos);
        return true;
    }
}

void initBridge() {
    LOG_INFO(LogCategories::kEngine, "Bridge initialized");
}

void AgentBridge::init() {
    m_scheduler.registerSystem(&m_movement_system);

    AgentBridge* self = this;

    ToolRegistry::instance().registerTool(ToolDesc{
        "queryEntities",
        "Query all alive entities that have a given component",
        "{\"comp_name\": \"string\"}",
        true,
        [self](const SmallString& json_args) -> SmallString {
            SmallString comp_name;
            usize pos = json_args.find("\"comp_name\"");
            if (pos != SmallString::npos) {
                pos = json_args.find(':', pos + 11);
                if (pos != SmallString::npos) {
                    ++pos;
                    while (pos < json_args.size() && (json_args[pos] == ' ' || json_args[pos] == '\t' || json_args[pos] == '"')) ++pos;
                    usize end = pos;
                    while (end < json_args.size() && json_args[end] != '"' && json_args[end] != ',' && json_args[end] != '}') ++end;
                    comp_name = json_args.substr(pos, end - pos);
                }
            }
            return self->queryEntities(comp_name);
        }
    });

    ToolRegistry::instance().registerTool(ToolDesc{
        "getComponent",
        "Get component data for a specific entity",
        "{\"entity\": uint32, \"comp_name\": \"string\"}",
        true,
        [self](const SmallString& json_args) -> SmallString {
            u32 entity_id = 0;
            SmallString comp_name;
            jsonParseUint32(json_args, "entity", entity_id);
            usize pos = json_args.find("\"comp_name\"");
            if (pos != SmallString::npos) {
                pos = json_args.find(':', pos + 11);
                if (pos != SmallString::npos) {
                    ++pos;
                    while (pos < json_args.size() && (json_args[pos] == ' ' || json_args[pos] == '\t' || json_args[pos] == '"')) ++pos;
                    usize end = pos;
                    while (end < json_args.size() && json_args[end] != '"' && json_args[end] != ',' && json_args[end] != '}') ++end;
                    comp_name = json_args.substr(pos, end - pos);
                }
            }
            Entity e{entity_id, self->m_world.getEntityGeneration(entity_id)};
            return self->getComponent(e, comp_name);
        }
    });

    ToolRegistry::instance().registerTool(ToolDesc{
        "setComponent",
        "Set component data for a specific entity",
        "{\"entity\": uint32, \"comp_name\": \"string\", \"values\": {...}}",
        false,
        [self](const SmallString& json_args) -> SmallString {
            u32 entity_id = 0;
            SmallString comp_name;
            SmallString values_json;
            jsonParseUint32(json_args, "entity", entity_id);
            usize pos = json_args.find("\"comp_name\"");
            if (pos != SmallString::npos) {
                pos = json_args.find(':', pos + 11);
                if (pos != SmallString::npos) {
                    ++pos;
                    while (pos < json_args.size() && (json_args[pos] == ' ' || json_args[pos] == '\t' || json_args[pos] == '"')) ++pos;
                    usize end = pos;
                    while (end < json_args.size() && json_args[end] != '"' && json_args[end] != ',' && json_args[end] != '}') ++end;
                    comp_name = json_args.substr(pos, end - pos);
                }
            }
            pos = json_args.find("\"values\"");
            if (pos != SmallString::npos) {
                pos = json_args.find(':', pos + 8);
                if (pos != SmallString::npos) {
                    ++pos;
                    while (pos < json_args.size() && (json_args[pos] == ' ' || json_args[pos] == '\t')) ++pos;
                    if (pos < json_args.size() && json_args[pos] == '{') {
                        usize brace = 1;
                        usize end = pos + 1;
                        while (end < json_args.size() && brace > 0) {
                            if (json_args[end] == '{') ++brace;
                            else if (json_args[end] == '}') --brace;
                            ++end;
                        }
                        values_json = json_args.substr(pos, end - pos);
                    }
                }
            }
            Entity e{entity_id, self->m_world.getEntityGeneration(entity_id)};
            return self->setComponent(e, comp_name, values_json);
        }
    });

    ToolRegistry::instance().registerTool(ToolDesc{
        "stepWorld",
        "Advance the simulation by one frame with given dt",
        "{\"dt\": float}",
        false,
        [self](const SmallString& json_args) -> SmallString {
            f32 dt = 1.0f;
            jsonParseFloat(json_args, "dt", dt);
            self->step(dt);
            return "{\"ok\":true}";
        }
    });

    ToolRegistry::instance().registerTool(ToolDesc{
        "queryEntitiesByMask",
        "Query all alive entities that match a component mask",
        "{\"mask\": uint32}",
        true,
        [self](const SmallString& json_args) -> SmallString {
            u32 mask = 0;
            jsonParseUint32(json_args, "mask", mask);
            return self->queryEntitiesByMask(mask);
        }
    });

    ToolRegistry::instance().registerTool(ToolDesc{
        "getWorldSummary",
        "Get a summary of the current world state",
        "{}",
        true,
        [self](const SmallString& json_args) -> SmallString {
            (void)json_args;
            return self->getWorldSummary();
        }
    });
}

void AgentBridge::step(f32 dt) {
    m_scheduler.tick(m_world, dt);
}

SmallString AgentBridge::queryEntities(const SmallString& comp_name) const {
    u32 mask = TypeRegistry::instance().getComponentMask(comp_name.c_str());
    if (mask == 0) {
        return "{\"error\":\"component not found\"}";
    }
    return queryEntitiesByMask(mask);
}

SmallString AgentBridge::queryEntitiesByMask(u64 mask) const {
    SmallString json = "[";
    bool first = true;
    for (u32 id = 0; id < m_world.maxEntityID(); ++id) {
        Entity e{id, m_world.getEntityGeneration(id)};
        if (m_world.valid(e) && (m_world.getEntityMask(id) & mask) == mask) {
            if (!first) json += ",";
            first = false;
            json += toSmallString(id);
        }
    }
    json += "]";
    return json;
}

SmallString AgentBridge::getWorldSummary() const {
    SmallString json = "{";
    json += "\"entityCount\":" + toSmallString(m_world.entityCount()) + ",";
    json += "\"componentArrays\":[";
    bool first = true;
    for (const auto& pair : m_world.componentArrays()) {
        if (!first) json += ",";
        first = false;
        const auto* desc = TypeRegistry::instance().findComponent(pair.first);
        json += "{";
        json += "\"name\":\"" + SmallString(desc ? desc->name.c_str() : "unknown") + "\",";
        json += "\"count\":" + toSmallString(pair.second->count());
        json += "}";
    }
    json += "]";
    json += "}";
    return json;
}

SmallString AgentBridge::getComponent(Entity e, const SmallString& comp_name) const {
    if (!m_world.valid(e)) {
        return "{\"error\":\"invalid entity\"}";
    }

    ComponentTypeID typeID = TypeRegistry::instance().findComponentID(comp_name.c_str());
    if (typeID == INVALID_COMPONENT_TYPE_ID) {
        return "{\"error\":\"component not found\"}";
    }

    const auto& arrays = m_world.componentArrays();
    auto* array = arrays.find(typeID);
    if (!array || !(*array)->has(e)) {
        return "{\"error\":\"component not found on entity\"}";
    }

    const void* comp_ptr = (*array)->getRaw(e);
    const auto* desc = TypeRegistry::instance().findComponent(typeID);
    if (!desc) {
        return "{\"error\":\"component descriptor missing\"}";
    }

    SmallString json = "{";
    bool first = true;
    for (const auto& field : desc->fields) {
        if (!first) json += ",";
        first = false;
        json += "\"" + SmallString(field.name.c_str()) + "\":";
        if (field.type == "float") {
            f32 val = *reinterpret_cast<const f32*>(static_cast<const char*>(comp_ptr) + field.offset);
            json += toSmallString(val);
        } else if (field.type == "SmallString") {
            const SmallString* str = reinterpret_cast<const SmallString*>(static_cast<const char*>(comp_ptr) + field.offset);
            json += "\"" + SmallString(str->c_str()) + "\"";
        } else if (field.type == "StringId") {
            const StringId* id = reinterpret_cast<const StringId*>(static_cast<const char*>(comp_ptr) + field.offset);
            const char* resolved = StringInternPool::instance().resolve(*id);
            json += "\"";
            json += (resolved ? resolved : "<unresolved>");
            json += "\"";
        } else {
            json += "null";
        }
    }
    json += "}";
    return json;
}

SmallString AgentBridge::setComponent(Entity e, const SmallString& comp_name, const SmallString& json) {
    if (!m_world.valid(e)) {
        return "{\"error\":\"invalid entity\"}";
    }

    ComponentTypeID typeID = TypeRegistry::instance().findComponentID(comp_name.c_str());
    if (typeID == INVALID_COMPONENT_TYPE_ID) {
        return "{\"error\":\"component not found\"}";
    }

    const auto& arrays = m_world.componentArrays();
    auto* array = arrays.find(typeID);
    if (!array || !(*array)->has(e)) {
        return "{\"error\":\"component not found on entity\"}";
    }

    void* comp_ptr = (*array)->getRaw(e);
    const auto* desc = TypeRegistry::instance().findComponent(typeID);
    if (!desc) {
        return "{\"error\":\"component descriptor missing\"}";
    }

    for (const auto& field : desc->fields) {
        if (field.type == "float") {
            f32 val = 0.0f;
            if (jsonParseFloat(json, field.name.c_str(), val)) {
                *reinterpret_cast<f32*>(static_cast<char*>(comp_ptr) + field.offset) = val;
            }
        } else if (field.type == "SmallString") {
            SmallString val;
            if (jsonParseString(json, field.name.c_str(), val)) {
                *reinterpret_cast<SmallString*>(static_cast<char*>(comp_ptr) + field.offset) = val.c_str();
            }
        } else if (field.type == "StringId") {
            SmallString val;
            if (jsonParseString(json, field.name.c_str(), val)) {
                StringId id = StringInternPool::instance().intern(val.c_str());
                *reinterpret_cast<StringId*>(static_cast<char*>(comp_ptr) + field.offset) = id;
            }
        }
    }

    return "{\"ok\":true}";
}

SmallString AgentBridge::callTool(const SmallString& name, const SmallString& json_args) const {
    return ToolRegistry::instance().callTool(name, json_args);
}

} // namespace Entelechy
