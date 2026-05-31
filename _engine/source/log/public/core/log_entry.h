#pragma once
#include "core/foundation_types.h"
#include "log/core/log_level.h"
#include "core/string/string_id.h"

namespace Entelechy {

// ============================================================
// Single log record data structure (ECS-friendly: pure data, no behavior)
// ============================================================
// LogEntry is a self-describing struct containing all metadata for one log line.
// Designed to avoid heap-allocating types like std::string, keeping a POD-like
// nature for efficient copying into ring buffers and async queues.
//
// Lifetime contract:
// - m_message points to a pre-formatted string buffer (caller guarantees validity until flush)
// - m_file points to the compile-time string literal (__FILE__), globally static lifetime
struct LogEntry {
    LogLevel    m_level;          // Severity level of this log entry
    StringId    m_category;       // Category hash (e.g. "Render" / "Physics")
    const char* m_category_name;  // Human-readable category name (points to static string)
    const char* m_message;        // Pointer to pre-formatted message buffer
    f64         m_timestamp_sec;  // High-resolution timestamp in seconds (steady_clock basis)
    const char* m_file;           // Source file path (__FILE__)
    const char* m_function;       // Function name (__FUNCTION__)
};

} // namespace Entelechy
