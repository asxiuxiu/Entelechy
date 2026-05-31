#include "core/string/string_intern_pool.h"

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
    auto* v = m_pool.find(h);
    if (v) {
        // Collision detection: same hash must map to identical content
        if (std::strcmp(*v, str) != 0) {
            CHECK(false && "StringId hash collision detected!");
        }
        return StringId(h);
    }
    usize len = std::strlen(str);
    char* copy = new char[len + 1];
    std::memcpy(copy, str, len + 1);
    m_pool.insert(h, copy);
    return StringId(h);
}

const char* StringInternPool::resolve(StringId id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto* v = m_pool.find(id.value());
    return v ? *v : nullptr;
}

bool StringInternPool::has(StringId id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pool.find(id.value()) != nullptr;
}

usize StringInternPool::count() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pool.size();
}

} // namespace Entelechy
