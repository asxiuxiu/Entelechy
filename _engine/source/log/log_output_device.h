#pragma once
#include "queued_log_entry.h"

namespace Entelechy {

// ============================================================
// Log output device interface (FOutputDevice-style abstraction)
// ============================================================
// Each output destination implements this interface.
// New devices can be added without modifying Logger core code.
class LogOutputDevice {
public:
    virtual ~LogOutputDevice() = default;

    // Write a single log entry to this device.
    virtual void write(const QueuedLogEntry& entry) = 0;

    // Flush any buffered output.
    virtual void flush() = 0;
};

} // namespace Entelechy
