#include "agent_bridge.h"
#include "type_registry.h"
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
        size_t pos = json.find(pattern);
        if (pos == std::string::npos) return false;

        pos = json.find(':', pos + pattern.length());
        if (pos == std::string::npos) return false;

        ++pos;
        // skip spaces and optional quotes
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '"')) ++pos;

        const char* str = json.c_str() + pos;
        char* end = nullptr;
        float val = std::strtof(str, &end);
        if (end == str) return false;

        out = val;
        return true;
    }

    bool jsonParseUint32(const std::string& json, const std::string& key, uint32_t& out) {
        std::string pattern = "\"" + key + "\"";
        size_t pos = json.find(pattern);
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
}

void initBridge() {
    printf("[Entelechy::bridge] initialized\n");
}

void AgentBridge::init() {
    m_scheduler.registerSystem(&m_movement_system);

    // Register built-in tools to ToolRegistry
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
                    size_t end = pos;
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
            uint32_t entity = 0;
            std::string comp_name;
            jsonParseUint32(json_args, "entity", entity);
            size_t pos = json_args.find("\"comp_name\"");
            if (pos != std::string::npos) {
                pos = json_args.find(':', pos + 11);
                if (pos != std::string::npos) {
                    ++pos;
                    while (pos < json_args.size() && (json_args[pos] == ' ' || json_args[pos] == '\t' || json_args[pos] == '"')) ++pos;
                    size_t end = pos;
                    while (end < json_args.size() && json_args[end] != '"' && json_args[end] != ',' && json_args[end] != '}') ++end;
                    comp_name = json_args.substr(pos, end - pos);
                }
            }
            return self->getComponent(entity, comp_name);
        }
    });

    ToolRegistry::instance().registerTool(ToolDesc{
        "setComponent",
        "Set component data for a specific entity",
        "{\"entity\": uint32, \"comp_name\": \"string\", \"values\": {...}}",
        false,
        [self](const std::string& json_args) -> std::string {
            uint32_t entity = 0;
            std::string comp_name;
            std::string values_json;
            jsonParseUint32(json_args, "entity", entity);
            size_t pos = json_args.find("\"comp_name\"");
            if (pos != std::string::npos) {
                pos = json_args.find(':', pos + 11);
                if (pos != std::string::npos) {
                    ++pos;
                    while (pos < json_args.size() && (json_args[pos] == ' ' || json_args[pos] == '\t' || json_args[pos] == '"')) ++pos;
                    size_t end = pos;
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
            return self->setComponent(entity, comp_name, values_json);
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
}

void AgentBridge::step(float dt) {
    m_scheduler.tick(m_world, dt);
}

std::string AgentBridge::queryEntities(const std::string& comp_name) const {
    const auto* desc = TypeRegistry::instance().findComponent(comp_name);
    if (!desc) {
        return "{\"error\":\"component not found\"}";
    }

    std::string json = "[";
    bool first = true;
    for (Entity e = 0; e < m_world.m_alive.size(); ++e) {
        if (m_world.valid(e)) {
            // In this minimal demo, all alive entities have Position and Velocity
            if (!first) json += ",";
            first = false;
            json += std::to_string(e);
        }
    }
    json += "]";
    return json;
}

std::string AgentBridge::getComponent(Entity e, const std::string& comp_name) const {
    if (!m_world.valid(e)) {
        return "{\"error\":\"invalid entity\"}";
    }

    const auto* desc = TypeRegistry::instance().findComponent(comp_name);
    if (!desc) {
        return "{\"error\":\"component not found\"}";
    }

    const void* comp_ptr = nullptr;
    if (comp_name == "Position") {
        comp_ptr = &m_world.m_positions[e];
    } else if (comp_name == "Velocity") {
        comp_ptr = &m_world.m_velocities[e];
    } else {
        return "{\"error\":\"component not mapped\"}";
    }

    std::string json = "{";
    bool first = true;
    for (const auto& field : desc->fields) {
        if (!first) json += ",";
        first = false;
        json += "\"" + field.name + "\":";
        if (field.type == "float") {
            float val = *reinterpret_cast<const float*>(static_cast<const char*>(comp_ptr) + field.offset);
            json += std::to_string(val);
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

    const auto* desc = TypeRegistry::instance().findComponent(comp_name);
    if (!desc) {
        return "{\"error\":\"component not found\"}";
    }

    void* comp_ptr = nullptr;
    if (comp_name == "Position") {
        comp_ptr = &m_world.m_positions[e];
    } else if (comp_name == "Velocity") {
        comp_ptr = &m_world.m_velocities[e];
    } else {
        return "{\"error\":\"component not mapped\"}";
    }

    for (const auto& field : desc->fields) {
        float val = 0.0f;
        if (jsonParseFloat(json, field.name, val)) {
            if (field.type == "float") {
                *reinterpret_cast<float*>(static_cast<char*>(comp_ptr) + field.offset) = val;
            }
        }
    }

    return "{\"ok\":true}";
}

std::string AgentBridge::callTool(const std::string& name, const std::string& json_args) const {
    return ToolRegistry::instance().callTool(name, json_args);
}

} // namespace Entelechy
