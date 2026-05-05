#pragma once
#include "log_output_device.h"
#include "queued_log_entry.h"

namespace Entelechy {

// ============================================================
// Console output device
// ============================================================
// Writes log entries to stdout with level prefix.
class ConsoleOutput : public LogOutputDevice {
public:
    void write(const QueuedLogEntry& entry) override;
    void flush() override;
};

} // namespace Entelechy
