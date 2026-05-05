#pragma once
#include <fstream>
#include <cstdint>
#include "log_output_device.h"
#include "queued_log_entry.h"

namespace Entelechy {

// ============================================================
// JSON Lines output device with rolling support
// ============================================================
// Writes structured JSON Lines (.jsonl) output.
// Each line is a self-contained JSON object for machine consumption.
class JsonFileOutput : public LogOutputDevice {
public:
    explicit JsonFileOutput(const char* basePath = "logs/engine.jsonl",
                            uint32_t maxSizeMb = 10,
                            uint32_t maxFiles = 5);
    ~JsonFileOutput() override;

    bool init();
    void write(const QueuedLogEntry& entry) override;
    void flush() override;

private:
    const char* m_base_path;
    uint32_t m_max_size_mb;
    uint32_t m_max_files;
    std::ofstream m_file_stream;
    bool m_file_opened = false;
    uint64_t m_current_size = 0;

    bool openLogFile();
    void rollFile();
    bool checkAndRoll();
    static void writeJsonEscaped(std::ostream& out, const char* str);
};

} // namespace Entelechy
