#include "file_output.h"
#include "log_utils.h"
#include <ctime>
#include <cstring>
#include <cstdio>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

namespace Entelechy {

FileOutput::FileOutput(const LogFileConfig& config)
    : m_config(config)
    , m_current_size(0) {
}

FileOutput::~FileOutput() {
    if (m_file_opened) {
        m_file_stream.close();
    }
}

bool FileOutput::init() {
    return openLogFile();
}

void FileOutput::write(const QueuedLogEntry& entry) {
    if (!m_file_opened) {
        return;
    }

    if (!checkAndRoll()) {
        return;
    }

    auto timeT = std::chrono::system_clock::to_time_t(entry.m_timestamp);
    std::tm* tm = std::localtime(&timeT);
    char timeBuf[32] = {};
    std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", tm);

    char lineBuf[2048];
    int lineLen = std::snprintf(lineBuf, sizeof(lineBuf), "%s [%s][%s::%s] %s\n",
        timeBuf,
        logLevelToString(entry.m_level),
        logFileBasename(entry.m_file),
        entry.m_function ? entry.m_function : "unknown",
        entry.m_message.c_str());

    if (lineLen > 0) {
        m_file_stream.write(lineBuf, lineLen);
        m_current_size += static_cast<uint64_t>(lineLen);
    }
}

void FileOutput::flush() {
    if (m_file_opened) {
        m_file_stream.flush();
    }
}

bool FileOutput::openLogFile() {
    // Extract directory path and ensure it exists.
    const char* lastSep = nullptr;
    for (const char* p = m_config.m_base_path; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            lastSep = p;
        }
    }

    if (lastSep) {
        char dirBuf[256] = {};
        size_t len = static_cast<size_t>(lastSep - m_config.m_base_path);
        if (len >= sizeof(dirBuf)) {
            len = sizeof(dirBuf) - 1;
        }
        std::memcpy(dirBuf, m_config.m_base_path, len);
        dirBuf[len] = '\0';

#ifdef _WIN32
        _mkdir(dirBuf);
#else
        mkdir(dirBuf, 0755);
#endif
    }

    m_file_stream.open(m_config.m_base_path, std::ios::out | std::ios::app);
    m_file_opened = m_file_stream.is_open();

    if (m_file_opened) {
        m_file_stream.seekp(0, std::ios::end);
        m_current_size = static_cast<uint64_t>(m_file_stream.tellp());
        if (m_current_size == static_cast<uint64_t>(-1)) {
            m_current_size = 0;
        }
    }

    return m_file_opened;
}

void FileOutput::rollFile() {
    if (!m_file_opened) {
        return;
    }

    m_file_stream.close();
    m_file_opened = false;

    // Delete the oldest file if it exists
    char oldestPath[256];
    std::snprintf(oldestPath, sizeof(oldestPath), "%s.%u",
        m_config.m_base_path, m_config.m_max_files);
    std::remove(oldestPath);

    // Shift existing backups: N-1 -> N, N-2 -> N-1, ..., 1 -> 2
    for (int i = static_cast<int>(m_config.m_max_files) - 1; i >= 1; --i) {
        char oldPath[256];
        char newPath[256];
        std::snprintf(oldPath, sizeof(oldPath), "%s.%d",
            m_config.m_base_path, i);
        std::snprintf(newPath, sizeof(newPath), "%s.%d",
            m_config.m_base_path, i + 1);
        std::rename(oldPath, newPath);
    }

    // Rename current to .1
    char backupPath[256];
    std::snprintf(backupPath, sizeof(backupPath), "%s.1", m_config.m_base_path);
    std::rename(m_config.m_base_path, backupPath);

    // Reopen new file
    openLogFile();
}

bool FileOutput::checkAndRoll() {
    const uint64_t maxSizeBytes = static_cast<uint64_t>(m_config.m_max_size_mb) * 1024 * 1024;
    if (m_current_size >= maxSizeBytes) {
        rollFile();
    }
    return m_file_opened;
}

} // namespace Entelechy
