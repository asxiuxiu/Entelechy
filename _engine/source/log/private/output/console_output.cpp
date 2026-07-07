#include "log/output/console_output.h"
#include <iostream>
#include "log/core/log_level.h"

namespace Entelechy
{

void ConsoleOutput::write(const QueuedLogEntry &entry)
{
    std::cout << "[" << logLevelToString(entry.m_level) << "]"
              << "[" << (entry.m_function ? entry.m_function : "unknown") << "] " << entry.m_message.c_str() << "\n";
}

void ConsoleOutput::flush()
{
    std::cout.flush();
}

} // namespace Entelechy
