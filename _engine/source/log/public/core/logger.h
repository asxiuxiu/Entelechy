#pragma once
#include <mutex>
#include <atomic>
#include <memory>
#include "core/container/dynamic_array.h"
#include "core/container/fixed_ring_queue.h"
#include "log/core/log_level.h"
#include "log/core/log_entry.h"
#include "log/core/log_category.h"
#include "log/core/queued_log_entry.h"
#include "log/output/log_output_device.h"

namespace Entelechy {

// ============================================================
// Asynchronous double-buffered logger
// ============================================================
// Thread-safe producer interface (log()) combined with a single-consumer
// flush() intended to be called once per frame from the main thread.
//
// Design:
// - Two DynamicArrays act as a double buffer: m_write_queue (producer) and
//   m_read_queue (consumer). Swap happens under mutex in flush().
// - A deque stores the last MAX_HISTORY entries as a ring buffer for
//   the ImGui log panel.
// - Output is routed through pluggable LogOutputDevice instances.
//
// Architecture notes:
// - Logger is intentionally a global singleton, NOT an ECS-driven system.
//   Logging is lower-level than ECS (memory allocators, ECS bootstrap,
//   and thread pools all need to log before a World exists).
// - Future evolution (Phase 2~3): replace mutex+vector with TLS lock-free
//   ring buffers (UE TraceLog style) to eliminate contention under
//   >1000 logs/frame from many worker threads.
// - Future evolution (Phase 1+): ECS can emit LogEvent components; a
//   LogSinkSystem reads them and forwards to Logger::instance().log().
class Logger {
public:
    static constexpr usize MAX_HISTORY = 4096;

    static Logger& instance();

    // Initialize with default devices (console + file + jsonl).
    bool init(const char* filePath = "logs/engine.log");

    // Add a custom output device. Logger takes ownership.
    void addOutputDevice(std::unique_ptr<LogOutputDevice> device);

    // Close file stream and flush any remaining entries.
    void shutdown();

    // Thread-safe: called from any producer thread.
    // Fast path: atomic level check without lock.
    void log(const LogEntry& entry);

    // Called once per frame from the main thread.
    // Swaps the double buffer and drains all pending entries to devices and history.
    void flush();

    // Register OS-level crash/termination handlers.
    // Call once after init() and before the engine starts running.
    void installCrashHandlers();

    // Best-effort signal-safe emergency flush. Does NOT take locks.
    // Iterates both m_read_queue and m_write_queue and writes to all
    // output devices. Intended for use only inside crash handlers.
    void emergencyFlush() noexcept;

    // Runtime level filtering
    void setMinLevel(LogLevel level);
    LogLevel getMinLevel() const;

    // Ordered from oldest to newest. Should only be accessed from the thread that calls flush().
    const FixedRingQueue<QueuedLogEntry, MAX_HISTORY>& history() const { return m_history; }

private:
    Logger() : m_min_level(LogLevel::Debug) {}
    ~Logger();

    DynamicArray<QueuedLogEntry> m_write_queue;
    DynamicArray<QueuedLogEntry> m_read_queue;
    std::mutex m_mutex;

    FixedRingQueue<QueuedLogEntry, MAX_HISTORY> m_history;

    DynamicArray<std::unique_ptr<LogOutputDevice>> m_devices;

    std::atomic<LogLevel> m_min_level;

    void pushToHistory(const QueuedLogEntry& entry);
};

} // namespace Entelechy
