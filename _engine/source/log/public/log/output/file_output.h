#pragma once
#include "core/foundation_types.h"
#include <fstream>

#include "log/output/log_output_device.h"
#include "log/core/queued_log_entry.h"

namespace Entelechy {

// ============================================================
// File output configuration
// ============================================================
struct LogFileConfig {
    SmallString m_base_path = "logs/engine.log";
    u32 m_max_size_mb = 10;
    u32 m_max_files = 5;
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
    u64 m_current_size = 0;

    bool openLogFile();
    void rollFile();
    bool checkAndRoll();
};

} // namespace Entelechy
