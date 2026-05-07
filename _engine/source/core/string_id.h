#pragma once
#include <cstdint>
#include <functional>
#include <compare>

namespace Entelechy {

constexpr uint64_t hashFNV1a(const char* str, uint64_t h = 0xcbf29ce484222325ULL) {
    return *str == '\0' ? h : hashFNV1a(str + 1, (h ^ static_cast<uint64_t>(*str)) * 0x100000001b3ULL);
}

class StringId {
    uint64_t m_hash;

public:
    constexpr StringId() : m_hash(0) {}
    constexpr StringId(const char* str) : m_hash(hashFNV1a(str)) {}

    [[nodiscard]] constexpr uint64_t value() const { return m_hash; }
    constexpr auto operator<=>(const StringId& other) const = default;
    constexpr bool operator==(const StringId& other) const = default;
};

consteval StringId operator"" _sid(const char* str, size_t) {
    return StringId(str);
}

} // namespace Entelechy

namespace std {
    template<>
    struct hash<Entelechy::StringId> {
        size_t operator()(const Entelechy::StringId& sid) const noexcept {
            return static_cast<size_t>(sid.value());
        }
    };
} // namespace std
