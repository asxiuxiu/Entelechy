#pragma once
#include <mutex>
#include <atomic>
#include <memory>
#include "base/dynamic_array.h"
#include "base/fixed_ring_queue.h"
#include "log_level.h"
#include "log_entry.h"
#include "log_category.h"
#include "queued_log_entry.h"
#include "log_output_device.h"

namespace Entelechy {

// ============================================================
// Asynchronous double-buffered logger
// ============================================================
// Thread-safe producer interface (log()) combined with a single-consumer
// flush() intended to be called once per frame from the main thread.
//
// Design:
// - Two vectors act as a double buffer: m_write_queue (producer) and
//   m_read_queue (consumer). Swap happens under mutex in flush().
// - A deque stores the last MAX_HISTORY entries as a ring buffer for
//   the ImGui log panel.
// - Output is routed through pluggable LogOutputDevice instances.
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
