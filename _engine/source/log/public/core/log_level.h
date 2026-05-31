#pragma once
#include "core/foundation_types.h"

namespace Entelechy {

// ============================================================
// Log level enumeration
// ============================================================
// Ordered from low to high severity for numeric comparison filtering.
// Higher numeric value indicates more severe level.
enum class LogLevel : u8 {
    Debug = 0,   // Fine-grained debug information
    Info = 1,    // General informational messages, enabled by default
    Warning = 2, // Unexpected but recoverable situations
    Error = 3    // Errors that may prevent normal execution
};

// Converts LogLevel to a human-readable C string for text output.
constexpr const char* logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:   return "DBG";
        case LogLevel::Info:    return "INF";
        case LogLevel::Warning: return "WRN";
        case LogLevel::Error:   return "ERR";
    }
    return "???";
}

// Converts LogLevel to a color tag string for ImGui panel rendering.
constexpr const char* logLevelToColorTag(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:   return "";
        case LogLevel::Info:    return "";
        case LogLevel::Warning: return "[YELLOW]";
        case LogLevel::Error:   return "[RED]";
    }
    return "";
}

} // namespace Entelechy
