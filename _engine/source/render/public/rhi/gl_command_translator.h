#pragma once
#include "render/rhi/render_command_buffer.h"
#include "core/container/hash_map.h"
#include "core/string/string_id.h"
#include <glad/glad.h>

namespace Entelechy
{

// Uniform location cache key (same as used by the old GLCommandList)
struct TranslatorUniformLocKey
{
    GLuint program = 0;
    StringId name;

    bool operator==(const TranslatorUniformLocKey &other) const
    {
        return program == other.program && name == other.name;
    }
};

struct TranslatorUniformLocKeyHash
{
    u64 operator()(const TranslatorUniformLocKey &key) const
    {
        u64 h = static_cast<u64>(key.program);
        h ^= key.name.value() + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        return h;
    }
};

// ------------------------------------------------------------------
// GLCommandTranslator — replays a RenderCommandBuffer by issuing
// equivalent OpenGL calls. Maintains the same state cache that the
// old immediate-mode GLCommandList used.
// ------------------------------------------------------------------
class GLCommandTranslator
{
public:
    GLCommandTranslator() = default;

    // Replay all commands in the buffer. The buffer must have been fully
    // recorded (begin/end lifecycle managed by DeferredCommandList).
    void execute(const RenderCommandBuffer &buffer);

    // Reset cached state between frames (called before execute).
    void resetState();

private:
    GLint getUniformLocation(StringId name);

    // Cached GL state to avoid redundant driver calls
    GLuint m_bound_program = 0;
    GLuint m_bound_vao = 0;
    GLuint m_bound_ebo = 0;
    u32 m_ebo_offset = 0;
    HashMap<TranslatorUniformLocKey, GLint, TranslatorUniformLocKeyHash> m_uniform_cache;
    u32 m_debug_group_depth = 0;
};

} // namespace Entelechy
