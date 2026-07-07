#pragma once
#include "log/core/queued_log_entry.h"

namespace Entelechy
{

// ============================================================
// Log output device interface (FOutputDevice-style abstraction)
// ============================================================
// Each output destination implements this interface.
// New devices can be added without modifying Logger core code.
//
// Architecture:
// - This is a classic Strategy pattern: Logger owns a list of devices
//   and forwards each log entry to every registered device.
// - Devices are responsible for their own buffering, formatting, and
//   I/O. Logger does not interpret device-specific behaviour.
// - Example extensions: NetworkOutput (TCP/UDP syslog), TelemetryOutput
//   (metrics backend), EditorOutput (forward to ImGui console panel).
class LogOutputDevice
{
public:
    virtual ~LogOutputDevice() = default;

    // Write a single log entry to this device.
    virtual void write(const QueuedLogEntry &entry) = 0;

    // Flush any buffered output.
    virtual void flush() = 0;
};

} // namespace Entelechy
