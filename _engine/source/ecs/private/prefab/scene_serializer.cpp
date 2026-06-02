#include "ecs/prefab/scene_serializer.h"
#include "ecs/world/world.h"
#include "ecs/type/type_registry.h"
#include "ecs/component/component_array.h"
#include "ecs/type/atom_registry.h"
#include "core/string/string_intern_pool.h"
#include "core/allocator/allocator.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <memory>

namespace Entelechy {

// ------------------------------------------------------------------
// JSON writing helpers
// ------------------------------------------------------------------

static void writeJsonString(String& out, const char* str) {
    out += '"';
    for (const char* p = str; *p; ++p) {
        switch (*p) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += *p; break;
        }
    }
    out += '"';
}

static void writeF32(String& out, f32 v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%g", v);
    out += buf;
}

static void writeI32(String& out, i32 v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d", v);
    out += buf;
}

static void writeU32(String& out, u32 v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%u", v);
    out += buf;
}

static void serializeField(const FieldDesc& field, const void* componentPtr, String& out);

static void serializeValue(StringId typeName, const void* ptr, String& out) {
    // Try atom registry first
    const AtomType* atom = AtomRegistry::instance().find(typeName);
    if (atom && atom->serialize) {
        atom->serialize(const_cast<void*>(ptr), out);
        return;
    }

    // Basic atoms (hard-coded for now)
    if (typeName == "f32"_sid || typeName == "float"_sid) {
        writeF32(out, *reinterpret_cast<const f32*>(ptr));
    } else if (typeName == "i32"_sid || typeName == "int"_sid || typeName == "int32_t"_sid) {
        writeI32(out, *reinterpret_cast<const i32*>(ptr));
    } else if (typeName == "u32"_sid || typeName == "uint32_t"_sid) {
        writeU32(out, *reinterpret_cast<const u32*>(ptr));
    } else if (typeName == "bool"_sid) {
        out += *reinterpret_cast<const bool*>(ptr) ? "true" : "false";
    } else if (typeName == "String"_sid) {
        writeJsonString(out, reinterpret_cast<const String*>(ptr)->c_str());
    } else if (typeName == "StringId"_sid) {
        StringId id = *reinterpret_cast<const StringId*>(ptr);
        const char* resolved = StringInternPool::instance().resolve(id);
        writeJsonString(out, resolved ? resolved : "");
    } else {
        // Try composite type lookup
        const TypeDesc* typeDesc = TypeRegistry::instance().findType(typeName);
        if (typeDesc && typeDesc->kind == TypeKind::Composite) {
            out += '{';
            bool first = true;
            for (const auto& f : typeDesc->fields) {
                if (!first) out += ",";
                first = false;
                out += '"';
                const char* nameResolved = StringInternPool::instance().resolve(f.name);
                if (nameResolved) out += nameResolved;
                out += "\":";
                serializeField(f, ptr, out);
            }
            out += '}';
        } else {
            out += "null";
        }
    }
}

static void serializeField(const FieldDesc& field, const void* componentPtr, String& out) {
    const void* fieldPtr = static_cast<const u8*>(componentPtr) + field.offset;
    serializeValue(field.type, fieldPtr, out);
}

static String serializeComponent(const ComponentDesc& desc, const void* componentPtr) {
    String out = "{";
    bool first = true;
    for (const auto& field : desc.fields) {
        if (!first) out += ",";
        first = false;
        out += '"';
        const char* nameResolved = StringInternPool::instance().resolve(field.name);
        if (nameResolved) out += nameResolved;
        out += "\":";
        serializeField(field, componentPtr, out);
    }
    out += "}";
    return out;
}

// ------------------------------------------------------------------
// JSON reading helpers (minimal parser for our output format)
// ------------------------------------------------------------------

struct JsonCursor {
    const char* s;
    usize pos;
    usize len;

    void skipWs() {
        while (pos < len && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' || s[pos] == '\r')) ++pos;
    }

    bool consume(char c) {
        skipWs();
        if (pos < len && s[pos] == c) { ++pos; return true; }
        return false;
    }

    bool match(const char* key) {
        skipWs();
        usize klen = std::strlen(key);
        if (pos + klen > len) return false;
        if (std::strncmp(s + pos, key, klen) == 0) {
            pos += klen;
            return true;
        }
        return false;
    }

    bool parseString(String& out) {
        skipWs();
        if (pos >= len || s[pos] != '"') return false;
        ++pos;
        usize start = pos;
        while (pos < len && s[pos] != '"') {
            if (s[pos] == '\\' && pos + 1 < len) pos += 2;
            else ++pos;
        }
        if (pos >= len) return false;
        out.assign(s + start, pos - start);
        ++pos; // skip closing quote
        return true;
    }

    bool parseFloat(f32& out) {
        skipWs();
        if (pos >= len) return false;
        char* end = nullptr;
        out = std::strtof(s + pos, &end);
        if (end == s + pos) return false;
        pos = end - s;
        return true;
    }

    bool parseUint32(u32& out) {
        skipWs();
        if (pos >= len) return false;
        char* end = nullptr;
        unsigned long v = std::strtoul(s + pos, &end, 10);
        if (end == s + pos) return false;
        out = static_cast<u32>(v);
        pos = end - s;
        return true;
    }

    bool parseInt32(i32& out) {
        skipWs();
        if (pos >= len) return false;
        char* end = nullptr;
        long v = std::strtol(s + pos, &end, 10);
        if (end == s + pos) return false;
        out = static_cast<i32>(v);
        pos = end - s;
        return true;
    }

    bool parseBool(bool& out) {
        skipWs();
        if (match("true")) { out = true; return true; }
        if (match("false")) { out = false; return true; }
        return false;
    }
};

static bool deserializeComposite(const TypeDesc* typeDesc, void* ptr, JsonCursor& cur);

static bool deserializeValue(StringId typeName, void* ptr, JsonCursor& cur) {
    // Try atom registry
    const AtomType* atom = AtomRegistry::instance().find(typeName);
    if (atom && atom->deserialize) {
        String jsonFragment;
        // For atom deserialize, we need to extract the raw value as string.
        // This is tricky without a full parser. For now, handle basic types directly.
    }

    if (typeName == "f32"_sid || typeName == "float"_sid) {
        return cur.parseFloat(*reinterpret_cast<f32*>(ptr));
    } else if (typeName == "i32"_sid || typeName == "int"_sid || typeName == "int32_t"_sid) {
        return cur.parseInt32(*reinterpret_cast<i32*>(ptr));
    } else if (typeName == "u32"_sid || typeName == "uint32_t"_sid) {
        return cur.parseUint32(*reinterpret_cast<u32*>(ptr));
    } else if (typeName == "bool"_sid) {
        return cur.parseBool(*reinterpret_cast<bool*>(ptr));
    } else if (typeName == "String"_sid) {
        String val;
        if (!cur.parseString(val)) return false;
        *reinterpret_cast<String*>(ptr) = val.c_str();
        return true;
    } else if (typeName == "StringId"_sid) {
        String val;
        if (!cur.parseString(val)) return false;
        *reinterpret_cast<StringId*>(ptr) = StringInternPool::instance().intern(val.c_str());
        return true;
    } else {
        const TypeDesc* typeDesc = TypeRegistry::instance().findType(typeName);
        if (typeDesc && typeDesc->kind == TypeKind::Composite) {
            return deserializeComposite(typeDesc, ptr, cur);
        }
    }
    return false;
}

static bool deserializeField(const FieldDesc& field, void* componentPtr, JsonCursor& cur) {
    void* fieldPtr = static_cast<u8*>(componentPtr) + field.offset;
    return deserializeValue(field.type, fieldPtr, cur);
}

static bool deserializeComposite(const TypeDesc* typeDesc, void* ptr, JsonCursor& cur) {
    if (!cur.consume('{')) return false;
    while (true) {
        cur.skipWs();
        if (cur.consume('}')) break;

        String fieldName;
        if (!cur.parseString(fieldName)) return false;
        if (!cur.consume(':')) return false;

        bool found = false;
        StringId fieldNameId = StringInternPool::instance().intern(fieldName.c_str());
        for (const auto& f : typeDesc->fields) {
            if (f.name == fieldNameId) {
                if (!deserializeField(f, ptr, cur)) return false;
                found = true;
                break;
            }
        }
        if (!found) {
            // Skip unknown field value
            // Very basic skip: count braces/brackets
            int depth = 0;
            bool inString = false;
            while (cur.pos < cur.len) {
                char c = cur.s[cur.pos];
                if (inString) {
                    if (c == '\\') { cur.pos += 2; continue; }
                    if (c == '"') inString = false;
                } else {
                    if (c == '"') inString = true;
                    else if (c == '{' || c == '[') ++depth;
                    else if (c == '}' || c == ']') {
                        if (depth == 0) break;
                        --depth;
                    } else if (c == ',' && depth == 0) {
                        break;
                    }
                }
                ++cur.pos;
            }
        }

        cur.skipWs();
        if (cur.consume('}')) break;
        if (!cur.consume(',')) return false;
    }
    return true;
}

// ------------------------------------------------------------------
// SceneSerializer implementation
// ------------------------------------------------------------------

String SceneSerializer::serialize(const World& world) const {
    String json = "{\"entities\":[";
    bool firstEntity = true;

    for (u32 id = 0; id < world.maxEntityID(); ++id) {
        Entity e{id, world.getEntityGeneration(id)};
        if (!world.valid(e)) continue;

        if (!firstEntity) json += ",";
        firstEntity = false;

        json += "{\"id\":";
        writeU32(json, e.id);
        json += ",\"components\":{";

        bool firstComp = true;
        for (const auto& pair : world.componentArrays()) {
            if (!pair.second->has(e)) continue;
            const auto* desc = TypeRegistry::instance().findComponent(pair.first);
            if (!desc) continue;

            if (!firstComp) json += ",";
            firstComp = false;

            const char* nameResolved = StringInternPool::instance().resolve(desc->name);
            writeJsonString(json, nameResolved ? nameResolved : "");
            json += ":";
            const void* raw = pair.second->getRaw(e);
            json += serializeComponent(*desc, raw);
        }

        json += "}}";
    }

    json += "]}";
    return json;
}

bool SceneSerializer::deserialize(World& world, const String& jsonStr) const {
    JsonCursor cur{jsonStr.c_str(), 0, jsonStr.length()};

    if (!cur.consume('{')) return false;
    String entitiesKey;
    if (!cur.parseString(entitiesKey)) return false;
    if (!cur.consume(':')) return false;
    if (!cur.consume('[')) return false;

    // Clear existing world
    // Note: we don't fully clear here; caller can decide.

    while (true) {
        cur.skipWs();
        if (cur.consume(']')) break;

        if (!cur.consume('{')) return false;

        // Parse "id":N
        String key;
        if (!cur.parseString(key) || key != "id") return false;
        if (!cur.consume(':')) return false;
        u32 entityId = 0;
        if (!cur.parseUint32(entityId)) return false;

        if (!cur.consume(',')) return false;

        // Parse "components":{...}
        if (!cur.parseString(key) || key != "components") return false;
        if (!cur.consume(':')) return false;
        if (!cur.consume('{')) return false;

        Entity e = world.spawn();
        (void)entityId; // we spawn a fresh entity; mapping could be added later

        while (true) {
            cur.skipWs();
            if (cur.consume('}')) break;

            String compName;
            if (!cur.parseString(compName)) return false;
            if (!cur.consume(':')) return false;

            StringId compNameId = StringInternPool::instance().intern(compName.c_str());
            const ComponentDesc* desc = TypeRegistry::instance().findComponent(compNameId);
            if (desc) {
                // Allocate component data on stack (max reasonable component size)
                alignas(64) u8 compBuffer[512];
                if (desc->size > sizeof(compBuffer)) {
                    // Skip oversized component
                    int depth = 0;
                    bool inString = false;
                    while (cur.pos < cur.len) {
                        char c = cur.s[cur.pos];
                        if (inString) {
                            if (c == '\\') { cur.pos += 2; continue; }
                            if (c == '"') inString = false;
                        } else {
                            if (c == '"') inString = true;
                            else if (c == '{' || c == '[') ++depth;
                            else if (c == '}' || c == ']') {
                                if (depth == 0) break;
                                --depth;
                            } else if (c == ',' && depth == 0) break;
                        }
                        ++cur.pos;
                    }
                } else {
                    std::memset(compBuffer, 0, desc->size);
                    for (const auto& field : desc->fields) {
                        // For JSON objects, fields may be in any order.
                        // We need to parse the whole object and then match fields.
                        // But our parser above does this in deserializeComposite...
                        // Actually ComponentDesc fields need to be parsed as a composite.
                        const TypeDesc* typeDesc = TypeRegistry::instance().findType(compNameId);
                        if (typeDesc) {
                            deserializeComposite(typeDesc, compBuffer, cur);
                        } else {
                            // Fallback: parse each field individually (not implemented)
                            int depth = 0;
                            bool inString = false;
                            while (cur.pos < cur.len) {
                                char c = cur.s[cur.pos];
                                if (inString) {
                                    if (c == '\\') { cur.pos += 2; continue; }
                                    if (c == '"') inString = false;
                                } else {
                                    if (c == '"') inString = true;
                                    else if (c == '{' || c == '[') ++depth;
                                    else if (c == '}' || c == ']') {
                                        if (depth == 0) break;
                                        --depth;
                                    } else if (c == ',' && depth == 0) break;
                                }
                                ++cur.pos;
                            }
                        }
                        break; // only one composite parse per component
                    }
                    world.addComponentRaw(e, TypeRegistry::instance().findComponentID(compNameId), compBuffer);
                }
            } else {
                // Unknown component: skip its JSON value
                int depth = 0;
                bool inString = false;
                while (cur.pos < cur.len) {
                    char c = cur.s[cur.pos];
                    if (inString) {
                        if (c == '\\') { cur.pos += 2; continue; }
                        if (c == '"') inString = false;
                    } else {
                        if (c == '"') inString = true;
                        else if (c == '{' || c == '[') ++depth;
                        else if (c == '}' || c == ']') {
                            if (depth == 0) break;
                            --depth;
                        } else if (c == ',' && depth == 0) break;
                    }
                    ++cur.pos;
                }
            }

            cur.skipWs();
            if (cur.consume('}')) break;
            if (!cur.consume(',')) return false;
        }

        cur.skipWs();
        if (cur.consume(']')) break;
        if (!cur.consume(',')) return false;
    }

    if (!cur.consume('}')) return false;
    return true;
}

bool SceneSerializer::saveToFile(const World& world, const Path& path) const {
    String json = serialize(world);
    FILE* f = nullptr;
#if PLATFORM_WINDOWS
    fopen_s(&f, path.c_str(), "wb");
#else
    f = std::fopen(path.c_str(), "wb");
#endif
    if (!f) return false;
    std::fwrite(json.c_str(), 1, json.length(), f);
    std::fclose(f);
    return true;
}

bool SceneSerializer::loadFromFile(World& world, const Path& path) const {
    FILE* f = nullptr;
#if PLATFORM_WINDOWS
    fopen_s(&f, path.c_str(), "rb");
#else
    f = std::fopen(path.c_str(), "rb");
#endif
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size <= 0) {
        std::fclose(f);
        return false;
    }
    char* buf = static_cast<char*>(DefaultAllocator::alloc(size + 1, alignof(char)));
    std::fread(buf, 1, size, f);
    buf[size] = '\0';
    std::fclose(f);
    String json(buf);
    DefaultAllocator::free(buf);
    return deserialize(world, json);
}

} // namespace Entelechy
