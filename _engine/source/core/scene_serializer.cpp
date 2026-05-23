#include "scene_serializer.h"
#include "world.h"
#include "type_registry.h"
#include "component_array.h"
#include "atom_registry.h"
#include "string_intern_pool.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>

namespace Entelechy {

// ------------------------------------------------------------------
// JSON writing helpers
// ------------------------------------------------------------------

static void writeJsonString(SmallString& out, const char* str) {
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

static void writeF32(SmallString& out, f32 v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%g", v);
    out += buf;
}

static void writeI32(SmallString& out, i32 v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d", v);
    out += buf;
}

static void writeU32(SmallString& out, u32 v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%u", v);
    out += buf;
}

static void serializeField(const FieldDesc& field, const void* componentPtr, SmallString& out);

static void serializeValue(const SmallString& typeName, const void* ptr, SmallString& out) {
    // Try atom registry first
    const AtomType* atom = AtomRegistry::instance().find(typeName);
    if (atom && atom->serialize) {
        atom->serialize(const_cast<void*>(ptr), out);
        return;
    }

    // Basic atoms (hard-coded for now)
    if (typeName == "f32" || typeName == "float") {
        writeF32(out, *reinterpret_cast<const f32*>(ptr));
    } else if (typeName == "i32" || typeName == "int" || typeName == "int32_t") {
        writeI32(out, *reinterpret_cast<const i32*>(ptr));
    } else if (typeName == "u32" || typeName == "uint32_t") {
        writeU32(out, *reinterpret_cast<const u32*>(ptr));
    } else if (typeName == "bool") {
        out += *reinterpret_cast<const bool*>(ptr) ? "true" : "false";
    } else if (typeName == "SmallString") {
        writeJsonString(out, reinterpret_cast<const SmallString*>(ptr)->c_str());
    } else if (typeName == "StringId") {
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
                out += f.name.c_str();
                out += "\":";
                serializeField(f, ptr, out);
            }
            out += '}';
        } else {
            out += "null";
        }
    }
}

static void serializeField(const FieldDesc& field, const void* componentPtr, SmallString& out) {
    const void* fieldPtr = static_cast<const u8*>(componentPtr) + field.offset;
    serializeValue(field.type, fieldPtr, out);
}

static SmallString serializeComponent(const ComponentDesc& desc, const void* componentPtr) {
    SmallString out = "{";
    bool first = true;
    for (const auto& field : desc.fields) {
        if (!first) out += ",";
        first = false;
        out += '"';
        out += field.name.c_str();
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

    bool parseString(SmallString& out) {
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

static bool deserializeAtom(const SmallString& typeName, void* ptr, JsonCursor& cur);
static bool deserializeComposite(const TypeDesc* typeDesc, void* ptr, JsonCursor& cur);

static bool deserializeValue(const SmallString& typeName, void* ptr, JsonCursor& cur) {
    // Try atom registry
    const AtomType* atom = AtomRegistry::instance().find(typeName);
    if (atom && atom->deserialize) {
        SmallString jsonFragment;
        // For atom deserialize, we need to extract the raw value as string.
        // This is tricky without a full parser. For now, handle basic types directly.
    }

    if (typeName == "f32" || typeName == "float") {
        return cur.parseFloat(*reinterpret_cast<f32*>(ptr));
    } else if (typeName == "i32" || typeName == "int" || typeName == "int32_t") {
        return cur.parseInt32(*reinterpret_cast<i32*>(ptr));
    } else if (typeName == "u32" || typeName == "uint32_t") {
        return cur.parseUint32(*reinterpret_cast<u32*>(ptr));
    } else if (typeName == "bool") {
        return cur.parseBool(*reinterpret_cast<bool*>(ptr));
    } else if (typeName == "SmallString") {
        SmallString val;
        if (!cur.parseString(val)) return false;
        *reinterpret_cast<SmallString*>(ptr) = val.c_str();
        return true;
    } else if (typeName == "StringId") {
        SmallString val;
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

        SmallString fieldName;
        if (!cur.parseString(fieldName)) return false;
        if (!cur.consume(':')) return false;

        bool found = false;
        for (const auto& f : typeDesc->fields) {
            if (f.name == fieldName) {
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

SmallString SceneSerializer::serialize(const World& world) const {
    SmallString json = "{\"entities\":[";
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

            writeJsonString(json, desc->name.c_str());
            json += ":";
            const void* raw = pair.second->getRaw(e);
            json += serializeComponent(*desc, raw);
        }

        json += "}}";
    }

    json += "]}";
    return json;
}

bool SceneSerializer::deserialize(World& world, const SmallString& jsonStr) const {
    JsonCursor cur{jsonStr.c_str(), 0, jsonStr.length()};

    if (!cur.consume('{')) return false;
    SmallString entitiesKey;
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
        SmallString key;
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

            SmallString compName;
            if (!cur.parseString(compName)) return false;
            if (!cur.consume(':')) return false;

            const ComponentDesc* desc = TypeRegistry::instance().findComponent(compName);
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
                        const TypeDesc* typeDesc = TypeRegistry::instance().findType(compName);
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
                    world.addComponentRaw(e, TypeRegistry::instance().findComponentID(compName), compBuffer);
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
    SmallString json = serialize(world);
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
    char* buf = new char[size + 1];
    std::fread(buf, 1, size, f);
    buf[size] = '\0';
    std::fclose(f);
    SmallString json(buf);
    delete[] buf;
    return deserialize(world, json);
}

} // namespace Entelechy
