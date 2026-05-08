#include "string_intern_pool.h"

namespace Entelechy {

StringInternPool& StringInternPool::instance() {
    static StringInternPool pool;
    return pool;
}

StringId StringInternPool::intern(const char* str) {
    if (!str || str[0] == '\0') {
        return StringId();
    }
    u64 h = hashFNV1a(str);
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_pool.find(h);
    if (it != m_pool.end()) {
        // Collision detection: same hash must map to identical content
        if (std::strcmp(it->second, str) != 0) {
            CHECK(false && "StringId hash collision detected!");
        }
        return StringId(h);
    }
    usize len = std::strlen(str);
    char* copy = new char[len + 1];
    std::memcpy(copy, str, len + 1);
    m_pool[h] = copy;
    return StringId(h);
}

const char* StringInternPool::resolve(StringId id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_pool.find(id.value());
    return it != m_pool.end() ? it->second : nullptr;
}

bool StringInternPool::has(StringId id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pool.find(id.value()) != m_pool.end();
}

usize StringInternPool::count() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pool.size();
}

} // namespace Entelechy
