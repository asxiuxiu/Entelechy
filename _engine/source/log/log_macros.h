#pragma once
#include <cstdio>
#include <chrono>
#include "log_level.h"
#include "log_entry.h"
#include "log_category.h"
#include "logger.h"
#include "base/string_format.h"

namespace Entelechy {

// ============================================================
// Log macro architecture: two-tier filtering
// ============================================================
//
// Tier 1 — Compile-time filter (LOG_COMPILE_LEVEL):
//   - Shipping builds strip Debug/Info via if constexpr.
//   - Zero runtime cost for disabled levels.
//
// Tier 2 — Runtime filter (Logger::setMinLevel):
//   - Atomic level check in logDispatch() before formatting/queueing.
//   - Allows live toggling (e.g. console command "log.render verbose").
//
// Once macros (LOG_*_ONCE) use a static bool per call site.
// They are NOT thread-safe for the first race, but the race window
// is a single bool store and duplicate emission is harmless.
//
// ============================================================
// Compile-time log level filter
// ============================================================
#ifdef SHIPPING_BUILD
    constexpr LogLevel LOG_COMPILE_LEVEL = LogLevel::Error;
#else
    constexpr LogLevel LOG_COMPILE_LEVEL = LogLevel::Debug;
#endif

// ============================================================
// Compile-time level check
// ============================================================
template<LogLevel Level>
constexpr bool isLogEnabled() {
    return static_cast<u8>(Level) >= static_cast<u8>(LOG_COMPILE_LEVEL);
}

// ============================================================
// Timestamp helper
// ============================================================
inline f64 getLogTimestampSec() {
    using Clock = std::chrono::steady_clock;
    static const auto s_start = Clock::now();
    auto now = Clock::now();
    return std::chrono::duration<f64>(now - s_start).count();
}

// ============================================================
// Internal dispatch helper
// ============================================================
template<LogLevel Level>
inline LogEntry buildLogEntry(const LogCategory& category, const char* message, const char* file, const char* function) {
    LogEntry entry;
    entry.m_level = Level;
    entry.m_category = category.m_id;
    entry.m_category_name = category.m_name;
    entry.m_message = message;
    entry.m_timestamp_sec = getLogTimestampSec();
    entry.m_file = file;
    entry.m_function = function;
    return entry;
}

// Variadic dispatch: formats the message when extra args are provided,
// passes through directly when there are no format arguments.
template<LogLevel Level, typename... Args>
inline void logDispatch(const LogCategory& category, const char* file, const char* function, const char* fmt, Args&&... args) {
    if constexpr (isLogEnabled<Level>()) {
        if constexpr (sizeof...(Args) == 0) {
            // No formatting arguments: use fmt directly to avoid snprintf
            // interpreting '%' characters as format specifiers.
            LogEntry entry = buildLogEntry<Level>(category, fmt, file, function);
            Logger::instance().log(entry);
        } else {
            char buf[1024];
            std::snprintf(buf, sizeof(buf), fmt, std::forward<Args>(args)...);
            LogEntry entry = buildLogEntry<Level>(category, buf, file, function);
            Logger::instance().log(entry);
        }
    }
}

// ============================================================
// Type-safe format-string log macros ({0} style)
// ============================================================
// Uses formatString() instead of snprintf for type safety and zero heap alloc.
// Syntax: "Player {0} pos=({1}, {2})"
//
// Usage:
//   LOG_FMT_INFO(Entelechy::LogCategories::kEngine, "Player {0} pos=({1}, {2})", "hero", 1.0f, 2.0f);

template<LogLevel Level, typename... Args>
inline void logDispatchFmt(const LogCategory& category, const char* file, const char* function, const char* fmt, Args&&... args) {
    if constexpr (isLogEnabled<Level>()) {
        if constexpr (sizeof...(Args) == 0) {
            LogEntry entry = buildLogEntry<Level>(category, fmt, file, function);
            Logger::instance().log(entry);
        } else {
            char buf[1024];
            formatString(buf, sizeof(buf), fmt, std::forward<Args>(args)...);
            LogEntry entry = buildLogEntry<Level>(category, buf, file, function);
            Logger::instance().log(entry);
        }
    }
}

} // namespace Entelechy

// ============================================================
// User-facing log macros (printf-style variadic)
// ============================================================
// Usage:
//   LOG_DEBUG(Entelechy::LogCategories::kRender, "Shader compiled");
//   LOG_INFO(Entelechy::LogCategories::kEngine, "FPS=%.1f", fps);
//   LOG_WARN(Entelechy::LogCategories::kPhysics, "Slow frame, dt=%f", dt);
//   LOG_ERROR(Entelechy::LogCategories::kNetwork, "Connection lost, code=%d", code);

#define LOG_DEBUG(category, ...) \
    ::Entelechy::logDispatch<::Entelechy::LogLevel::Debug>(category, __FILE__, __FUNCTION__, __VA_ARGS__)

#define LOG_INFO(category, ...) \
    ::Entelechy::logDispatch<::Entelechy::LogLevel::Info>(category, __FILE__, __FUNCTION__, __VA_ARGS__)

#define LOG_WARN(category, ...) \
    ::Entelechy::logDispatch<::Entelechy::LogLevel::Warning>(category, __FILE__, __FUNCTION__, __VA_ARGS__)

#define LOG_ERROR(category, ...) \
    ::Entelechy::logDispatch<::Entelechy::LogLevel::Error>(category, __FILE__, __FUNCTION__, __VA_ARGS__)

// ============================================================
// Once-only log macros
// ============================================================
// Guarantees that a log message is only emitted once per call site.
// Usage:
//   LOG_WARN_ONCE(Entelechy::LogCategories::kRender, "Shader fallback used");

#define LOG_DEBUG_ONCE(category, ...) \
    do { static bool _logged = false; if (!_logged) { _logged = true; LOG_DEBUG(category, __VA_ARGS__); } } while(0)

#define LOG_INFO_ONCE(category, ...) \
    do { static bool _logged = false; if (!_logged) { _logged = true; LOG_INFO(category, __VA_ARGS__); } } while(0)

#define LOG_WARN_ONCE(category, ...) \
    do { static bool _logged = false; if (!_logged) { _logged = true; LOG_WARN(category, __VA_ARGS__); } } while(0)

#define LOG_ERROR_ONCE(category, ...) \
    do { static bool _logged = false; if (!_logged) { _logged = true; LOG_ERROR(category, __VA_ARGS__); } } while(0)

// ============================================================
// User-facing type-safe format log macros ({0} style)
// ============================================================
#define LOG_FMT_DEBUG(category, ...) \
    ::Entelechy::logDispatchFmt<::Entelechy::LogLevel::Debug>(category, __FILE__, __FUNCTION__, __VA_ARGS__)

#define LOG_FMT_INFO(category, ...) \
    ::Entelechy::logDispatchFmt<::Entelechy::LogLevel::Info>(category, __FILE__, __FUNCTION__, __VA_ARGS__)

#define LOG_FMT_WARN(category, ...) \
    ::Entelechy::logDispatchFmt<::Entelechy::LogLevel::Warning>(category, __FILE__, __FUNCTION__, __VA_ARGS__)

#define LOG_FMT_ERROR(category, ...) \
    ::Entelechy::logDispatchFmt<::Entelechy::LogLevel::Error>(category, __FILE__, __FUNCTION__, __VA_ARGS__)
