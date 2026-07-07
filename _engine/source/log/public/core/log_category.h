#pragma once
#include <cstdint>
#include "core/string/string_id.h"

namespace Entelechy
{

// ============================================================
// Log category
// ============================================================
// Categories provide logical grouping for logs, such as Render, Physics, AI, etc.
// Uses StringId (FNV-1a hash) to avoid runtime string comparison and storage overhead.
//
// Usage example:
//   static constexpr LogCategory kLogRender("Render");
//   LOG_INFO(kLogRender, "Shader compiled");
//
// Note: The string pointer passed to LogCategory constructor is only used at
// compile-time / static initialization and is not retained long-term.
// The hash value is the sole identifier for subsequent comparisons and storage.
struct LogCategory
{
    StringId m_id;      // Hash identifier for the category
    const char *m_name; // Original category name (for human-readable output, points to static string)

    constexpr LogCategory(const char *name) : m_id(name), m_name(name) {}

    constexpr bool operator==(const LogCategory &other) const
    {
        return m_id == other.m_id;
    }
    constexpr bool operator!=(const LogCategory &other) const
    {
        return m_id != other.m_id;
    }
};

// ============================================================
// Engine built-in categories (predefined)
// ============================================================
// These categories exist from engine initialization.
// User code may also define custom categories.
// Custom category names should use PascalCase, e.g. "MyGameSystem".
namespace LogCategories
{
inline constexpr LogCategory kEngine("Engine");
inline constexpr LogCategory kRender("Render");
inline constexpr LogCategory kPhysics("Physics");
inline constexpr LogCategory kAI("AI");
inline constexpr LogCategory kInput("Input");
inline constexpr LogCategory kAudio("Audio");
inline constexpr LogCategory kNetwork("Network");
inline constexpr LogCategory kECS("ECS");
inline constexpr LogCategory kMemory("Memory");
inline constexpr LogCategory kWindow("Window");
} // namespace LogCategories

} // namespace Entelechy
