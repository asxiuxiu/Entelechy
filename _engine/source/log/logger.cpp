#include "logger.h"
#include "console_output.h"
#include "file_output.h"
#include "json_file_output.h"
#include <iostream>
#include <ctime>
#include <cstring>

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
    // Generate timestamped base path: insert _YYYYMMDD_HHMMSS_mmm before extension
    auto now = std::chrono::system_clock::now();
    auto timeT = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&timeT);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    char timeSuffixBuf[64];
    std::strftime(timeSuffixBuf, sizeof(timeSuffixBuf), "_%Y%m%d_%H%M%S", tm);
    SmallString timeSuffix = timeSuffixBuf;
    char msBuf[8];
    std::snprintf(msBuf, sizeof(msBuf), "_%03d", static_cast<int>(ms.count()));
    timeSuffix += msBuf;

    // Build timestamped text path
    SmallString textPath;
    const char* dot = std::strrchr(filePath, '.');
    if (dot) {
        textPath.assign(filePath, dot - filePath);
        textPath += timeSuffix;
        textPath += dot;
    } else {
        textPath = filePath;
        textPath += timeSuffix;
    }

    // Derive json path from text path
    SmallString jsonPath;
    usize pathLen = textPath.size();
    if (pathLen > 4 && textPath.endsWith(".log")) {
        jsonPath = textPath.substr(0, pathLen - 4) + ".jsonl";
    } else {
        jsonPath = textPath + ".jsonl";
    }

    // Console output
    addOutputDevice(std::make_unique<ConsoleOutput>());

    // Text file output
    auto fileOutput = std::make_unique<FileOutput>(LogFileConfig{textPath, 10, 5});
    if (fileOutput->init()) {
        addOutputDevice(std::move(fileOutput));
    }

    // JSON Lines output
    auto jsonOutput = std::make_unique<JsonFileOutput>(jsonPath.c_str(), 10, 5);
    if (jsonOutput->init()) {
        addOutputDevice(std::move(jsonOutput));
    }

    return true;
}

void Logger::addOutputDevice(std::unique_ptr<LogOutputDevice> device) {
    if (device) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_devices.pushBack(std::move(device));
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
    if (static_cast<u8>(entry.m_level) < static_cast<u8>(m_min_level.load(std::memory_order_relaxed))) {
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
    m_write_queue.pushBack(std::move(queued));
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
    m_history.pushBack(entry);
}

} // namespace Entelechy
