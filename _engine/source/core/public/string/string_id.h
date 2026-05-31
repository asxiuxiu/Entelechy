#pragma once
#include "core/foundation_types.h"
#include <functional>
#include <compare>

namespace Entelechy {

constexpr u64 hashFNV1a(const char* str, u64 h = 0xcbf29ce484222325ULL) {
    return *str == '\0' ? h : hashFNV1a(str + 1, (h ^ static_cast<u64>(*str)) * 0x100000001b3ULL);
}

class StringId {
    u64 m_hash;

public:
    constexpr StringId() : m_hash(0) {}
    constexpr StringId(const char* str) : m_hash(hashFNV1a(str)) {}
    constexpr explicit StringId(u64 hash) : m_hash(hash) {}

    [[nodiscard]] constexpr u64 value() const { return m_hash; }
    constexpr auto operator<=>(const StringId& other) const = default;
    constexpr bool operator==(const StringId& other) const = default;
};

consteval StringId operator"" _sid(const char* str, usize) {
    return StringId(str);
}

} // namespace Entelechy

namespace std {
    template<>
    struct hash<Entelechy::StringId> {
        usize operator()(const Entelechy::StringId& sid) const noexcept {
            return static_cast<usize>(sid.value());
        }
    };
} // namespace std
