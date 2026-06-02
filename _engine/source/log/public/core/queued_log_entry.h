#pragma once
#include <chrono>
#include "log/core/log_level.h"
#include "core/string/string_id.h"
#include "core/string/string.h"

namespace Entelechy {

// ============================================================
// Internal queued representation of a log entry
// ============================================================
// Unlike LogEntry (which holds a borrowed const char*), this stores the message
// in a String to survive across thread boundaries and buffer swaps.
struct QueuedLogEntry {
    LogLevel m_level;
    StringId m_category;
    const char* m_category_name; // Human-readable category name (points to static string)
    String m_message;
    std::chrono::system_clock::time_point m_timestamp;
    const char* m_file;
    const char* m_function;
};

} // namespace Entelechy
