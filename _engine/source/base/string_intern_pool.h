#pragma once
#include "foundation_types.h"
#include "string_id.h"
#include "hash_map.h"
#include <mutex>
#include <cstring>

namespace Entelechy {

// ============================================================
// StringInternPool — Runtime string deduplication & collision detection
// ============================================================
// Stores a global mapping from StringId hash to the original string content.
// - Enables O(1) StringId -> readable string resolution for debug/tooling
// - Detects hash collisions in DEBUG builds (CHECK-fail)
// - Thread-safe: all operations are protected by a mutex
//
// Usage:
//   StringId id = StringInternPool::instance().intern("MeshRenderer");
//   const char* name = StringInternPool::instance().resolve(id);
//
// Note: Interned strings are never freed (intentional for engine lifetime).
//       Do NOT intern dynamically-generated unique strings (e.g. "Entity_12345")
//       or the pool will grow unbounded.

class StringInternPool {
public:
    static StringInternPool& instance();

    StringId intern(const char* str);
    const char* resolve(StringId id) const;

    bool has(StringId id) const;
    usize count() const;

private:
    mutable std::mutex m_mutex;
    HashMap<u64, const char*> m_pool;
};

} // namespace Entelechy
