#include "agent_bridge.h"
#include "type_registry.h"
#include "log/log_macros.h"
#include "core/string_intern_pool.h"
#include <cstdio>
#include <string>
#include <cstring>

namespace Entelechy {

namespace {
    bool jsonHasKey(const std::string& json, const std::string& key) {
        std::string pattern = "\"" + key + "\"";
        return json.find(pattern) != std::string::npos;
    }

    bool jsonParseFloat(const std::string& json, const std::string& key, float& out) {
        std::string pattern = "\"" + key + "\"";
        usize pos = json.find(pattern);
        if (pos == std::string::npos) return false;

        pos = json.find(':', pos + pattern.length());
        if (pos == std::string::npos) return false;

        ++pos;
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '"')) ++pos;

        const char* str = json.c_str() + pos;
        char* end = nullptr;
        float val = std::strtof(str, &end);
        if (end == str) return false;

        out = val;
        return true;
    }

    bool jsonParseUint32(const std::string& json, const std::string& key, u32& out) {
        std::string pattern = "\"" + key + "\"";
        usize pos = json.find(pattern);
        if (pos == std::string::npos) return false;

        pos = json.find(':', pos + pattern.length());
        if (pos == std::string::npos) return false;

        ++pos;
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;

        const char* str = json.c_str() + pos;
        char* end = nullptr;
        unsigned long val = std::strtoul(str, &end, 10);
        if (end == str) return false;

        out = static_cast<uint32_t>(val);
        return true;
    }

    bool jsonParseString(const std::string& json, const std::string& key, std::string& out) {
        std::string pattern = "\"" + key + "\"";
        usize pos = json.find(pattern);
        if (pos == std::string::npos) return false;

        pos = json.find(':', pos + pattern.length());
        if (pos == std::string::npos) return false;

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
        [self](const std::string& json_args) -> std::string {
            std::string comp_name;
            size_t pos = json_args.find("\"comp_name\"");
            if (pos != std::string::npos) {
                pos = json_args.find(':', pos + 11);
                if (pos != std::string::npos) {
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
        [self](const std::string& json_args) -> std::string {
            u32 entity_id = 0;
            std::string comp_name;
            jsonParseUint32(json_args, "entity", entity_id);
            size_t pos = json_args.find("\"comp_name\"");
            if (pos != std::string::npos) {
                pos = json_args.find(':', pos + 11);
                if (pos != std::string::npos) {
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
        [self](const std::string& json_args) -> std::string {
            u32 entity_id = 0;
            std::string comp_name;
            std::string values_json;
            jsonParseUint32(json_args, "entity", entity_id);
            size_t pos = json_args.find("\"comp_name\"");
            if (pos != std::string::npos) {
                pos = json_args.find(':', pos + 11);
                if (pos != std::string::npos) {
                    ++pos;
                    while (pos < json_args.size() && (json_args[pos] == ' ' || json_args[pos] == '\t' || json_args[pos] == '"')) ++pos;
                    usize end = pos;
                    while (end < json_args.size() && json_args[end] != '"' && json_args[end] != ',' && json_args[end] != '}') ++end;
                    comp_name = json_args.substr(pos, end - pos);
                }
            }
            pos = json_args.find("\"values\"");
            if (pos != std::string::npos) {
                pos = json_args.find(':', pos + 8);
                if (pos != std::string::npos) {
                    ++pos;
                    while (pos < json_args.size() && (json_args[pos] == ' ' || json_args[pos] == '\t')) ++pos;
                    if (pos < json_args.size() && json_args[pos] == '{') {
                        size_t brace = 1;
                        size_t end = pos + 1;
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
        [self](const std::string& json_args) -> std::string {
            float dt = 1.0f;
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
        [self](const std::string& json_args) -> std::string {
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
        [self](const std::string& json_args) -> std::string {
            (void)json_args;
            return self->getWorldSummary();
        }
    });
}

void AgentBridge::step(float dt) {
    m_scheduler.tick(m_world, dt);
}

std::string AgentBridge::queryEntities(const std::string& comp_name) const {
    u32 mask = TypeRegistry::instance().getComponentMask(comp_name.c_str());
    if (mask == 0) {
        return "{\"error\":\"component not found\"}";
    }
    return queryEntitiesByMask(mask);
}

std::string AgentBridge::queryEntitiesByMask(u32 mask) const {
    std::string json = "[";
    bool first = true;
    for (u32 id = 0; id < m_world.maxEntityID(); ++id) {
        Entity e{id, m_world.getEntityGeneration(id)};
        if (m_world.valid(e) && (m_world.getEntityMask(id) & mask) == mask) {
            if (!first) json += ",";
            first = false;
            json += std::to_string(id);
        }
    }
    json += "]";
    return json;
}

std::string AgentBridge::getWorldSummary() const {
    std::string json = "{";
    json += "\"entityCount\":" + std::to_string(m_world.entityCount()) + ",";
    json += "\"componentArrays\":[";
    bool first = true;
    for (const auto& pair : m_world.componentArrays()) {
        if (!first) json += ",";
        first = false;
        const auto* desc = TypeRegistry::instance().findComponent(pair.first);
        json += "{";
        json += "\"name\":\"" + std::string(desc ? desc->name.c_str() : "unknown") + "\",";
        json += "\"count\":" + std::to_string(pair.second->count());
        json += "}";
    }
    json += "]";
    json += "}";
    return json;
}

std::string AgentBridge::getComponent(Entity e, const std::string& comp_name) const {
    if (!m_world.valid(e)) {
        return "{\"error\":\"invalid entity\"}";
    }

    ComponentTypeID typeID = TypeRegistry::instance().findComponentID(comp_name.c_str());
    if (typeID == INVALID_COMPONENT_TYPE_ID) {
        return "{\"error\":\"component not found\"}";
    }

    const auto& arrays = m_world.componentArrays();
    auto it = arrays.find(typeID);
    if (it == arrays.end() || !it->second->has(e)) {
        return "{\"error\":\"component not found on entity\"}";
    }

    const void* comp_ptr = it->second->getRaw(e);
    const auto* desc = TypeRegistry::instance().findComponent(typeID);
    if (!desc) {
        return "{\"error\":\"component descriptor missing\"}";
    }

    std::string json = "{";
    bool first = true;
    for (const auto& field : desc->fields) {
        if (!first) json += ",";
        first = false;
        json += "\"" + std::string(field.name.c_str()) + "\":";
        if (field.type == "float") {
            float val = *reinterpret_cast<const float*>(static_cast<const char*>(comp_ptr) + field.offset);
            json += std::to_string(val);
        } else if (field.type == "SmallString") {
            const SmallString* str = reinterpret_cast<const SmallString*>(static_cast<const char*>(comp_ptr) + field.offset);
            json += "\"" + std::string(str->c_str()) + "\"";
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

std::string AgentBridge::setComponent(Entity e, const std::string& comp_name, const std::string& json) {
    if (!m_world.valid(e)) {
        return "{\"error\":\"invalid entity\"}";
    }

    ComponentTypeID typeID = TypeRegistry::instance().findComponentID(comp_name.c_str());
    if (typeID == INVALID_COMPONENT_TYPE_ID) {
        return "{\"error\":\"component not found\"}";
    }

    const auto& arrays = m_world.componentArrays();
    auto it = arrays.find(typeID);
    if (it == arrays.end() || !it->second->has(e)) {
        return "{\"error\":\"component not found on entity\"}";
    }

    void* comp_ptr = it->second->getRaw(e);
    const auto* desc = TypeRegistry::instance().findComponent(typeID);
    if (!desc) {
        return "{\"error\":\"component descriptor missing\"}";
    }

    for (const auto& field : desc->fields) {
        if (field.type == "float") {
            float val = 0.0f;
            if (jsonParseFloat(json, field.name.c_str(), val)) {
                *reinterpret_cast<float*>(static_cast<char*>(comp_ptr) + field.offset) = val;
            }
        } else if (field.type == "SmallString") {
            std::string val;
            if (jsonParseString(json, field.name.c_str(), val)) {
                *reinterpret_cast<SmallString*>(static_cast<char*>(comp_ptr) + field.offset) = val.c_str();
            }
        } else if (field.type == "StringId") {
            std::string val;
            if (jsonParseString(json, field.name.c_str(), val)) {
                StringId id = StringInternPool::instance().intern(val.c_str());
                *reinterpret_cast<StringId*>(static_cast<char*>(comp_ptr) + field.offset) = id;
            }
        }
    }

    return "{\"ok\":true}";
}

std::string AgentBridge::callTool(const std::string& name, const std::string& json_args) const {
    return ToolRegistry::instance().callTool(name, json_args);
}

} // namespace Entelechy
