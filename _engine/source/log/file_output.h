#pragma once
#include <fstream>
#include <cstdint>
#include "log_output_device.h"
#include "queued_log_entry.h"

namespace Entelechy {

// ============================================================
// File output configuration
// ============================================================
struct LogFileConfig {
    const char* m_base_path = "logs/engine.log";
    uint32_t m_max_size_mb = 10;
    uint32_t m_max_files = 5;
};

// ============================================================
// Text file output device with rolling support
// ============================================================
// Writes human-readable log lines to a text file.
// Automatically rolls over when size exceeds threshold.
class FileOutput : public LogOutputDevice {
public:
    explicit FileOutput(const LogFileConfig& config = LogFileConfig{});
    ~FileOutput() override;

    bool init();
    void write(const QueuedLogEntry& entry) override;
    void flush() override;

private:
    LogFileConfig m_config;
    std::ofstream m_file_stream;
    bool m_file_opened = false;
    uint64_t m_current_size = 0;

    bool openLogFile();
    void rollFile();
    bool checkAndRoll();
};

} // namespace Entelechy
