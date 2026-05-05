#include "logger.h"
#include "console_output.h"
#include "file_output.h"
#include "json_file_output.h"
#include <iostream>
#include <ctime>

namespace Entelechy {

// ============================================================
// Singleton
// ============================================================
Logger& Logger::instance() {
    static Logger s_instance;
    return s_instance;
}

Logger::~Logger() {
    shutdown();
}

// ============================================================
// Lifecycle
// ============================================================
bool Logger::init(const char* filePath) {
    // Console output
    addOutputDevice(std::make_unique<ConsoleOutput>());

    // Text file output
    auto fileOutput = std::make_unique<FileOutput>(LogFileConfig{filePath, 10, 5});
    if (fileOutput->init()) {
        addOutputDevice(std::move(fileOutput));
    }

    // JSON Lines output
    // Derive .jsonl path from .log path, e.g. "logs/engine.log" -> "logs/engine.jsonl"
    char jsonPath[256];
    size_t pathLen = std::strlen(filePath);
    if (pathLen > 4 && std::strcmp(filePath + pathLen - 4, ".log") == 0) {
        std::snprintf(jsonPath, sizeof(jsonPath), "%.*s.jsonl",
            static_cast<int>(pathLen - 4), filePath);
    } else {
        std::snprintf(jsonPath, sizeof(jsonPath), "%s.jsonl", filePath);
    }
    auto jsonOutput = std::make_unique<JsonFileOutput>(jsonPath, 10, 5);
    if (jsonOutput->init()) {
        addOutputDevice(std::move(jsonOutput));
    }

    return true;
}

void Logger::addOutputDevice(std::unique_ptr<LogOutputDevice> device) {
    if (device) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_devices.push_back(std::move(device));
    }
}

void Logger::shutdown() {
    flush();

    std::lock_guard<std::mutex> lock(m_mutex);
    m_devices.clear();
}

// ============================================================
// Runtime level filtering
// ============================================================
void Logger::setMinLevel(LogLevel level) {
    m_min_level.store(level, std::memory_order_relaxed);
}

LogLevel Logger::getMinLevel() const {
    return m_min_level.load(std::memory_order_relaxed);
}

// ============================================================
// Producer
// ============================================================
void Logger::log(const LogEntry& entry) {
    // Fast path: atomic level check without lock
    if (static_cast<uint8_t>(entry.m_level) < static_cast<uint8_t>(m_min_level.load(std::memory_order_relaxed))) {
        return;
    }

    QueuedLogEntry queued;
    queued.m_level = entry.m_level;
    queued.m_category = entry.m_category;
    queued.m_category_name = entry.m_category_name ? entry.m_category_name : "Unknown";
    queued.m_message = entry.m_message ? entry.m_message : "";
    queued.m_timestamp = std::chrono::system_clock::now();
    queued.m_file = entry.m_file;
    queued.m_function = entry.m_function ? entry.m_function : "unknown";

    std::lock_guard<std::mutex> lock(m_mutex);
    m_write_queue.push_back(std::move(queued));
}

// ============================================================
// Consumer (called once per frame)
// ============================================================
void Logger::flush() {
    // Swap queues under lock so producers can continue allocating
    // into m_write_queue while we process m_read_queue outside the lock.
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_write_queue.swap(m_read_queue);
    }

    for (const auto& entry : m_read_queue) {
        // Write to all output devices
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (auto& device : m_devices) {
                device->write(entry);
            }
        }
        pushToHistory(entry);
    }

    m_read_queue.clear();

    // Flush all devices
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& device : m_devices) {
            device->flush();
        }
    }
}

// ============================================================
// Ring-buffer history
// ============================================================
void Logger::pushToHistory(const QueuedLogEntry& entry) {
    m_history.push_back(entry);
    if (m_history.size() > MAX_HISTORY) {
        m_history.pop_front();
    }
}

} // namespace Entelechy
