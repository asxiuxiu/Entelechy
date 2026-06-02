#pragma once
#include "core/foundation_types.h"
#include <fstream>

#include "log/output/log_output_device.h"
#include "log/core/queued_log_entry.h"

namespace Entelechy {

// ============================================================
// JSON Lines output device with rolling support
// ============================================================
// Writes structured JSON Lines (.jsonl) output.
// Each line is a self-contained JSON object for machine consumption.
class JsonFileOutput : public LogOutputDevice {
public:
    explicit JsonFileOutput(const char* basePath = "logs/engine.jsonl",
                            u32 maxSizeMb = 10,
                            u32 maxFiles = 5);
    ~JsonFileOutput() override;

    bool init();
    void write(const QueuedLogEntry& entry) override;
    void flush() override;

private:
    String m_base_path;
    u32 m_max_size_mb;
    u32 m_max_files;
    std::ofstream m_file_stream;
    bool m_file_opened = false;
    u64 m_current_size = 0;

    bool openLogFile();
    void rollFile();
    bool checkAndRoll();
    static void writeJsonEscaped(std::ostream& out, const char* str);
};

} // namespace Entelechy
