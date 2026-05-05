#include "json_file_output.h"
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

JsonFileOutput::JsonFileOutput(const char* basePath, uint32_t maxSizeMb, uint32_t maxFiles)
    : m_base_path(basePath)
    , m_max_size_mb(maxSizeMb)
    , m_max_files(maxFiles)
    , m_current_size(0) {
}

JsonFileOutput::~JsonFileOutput() {
    if (m_file_opened) {
        m_file_stream.close();
    }
}

bool JsonFileOutput::init() {
    // Ensure directory exists
    const char* lastSep = nullptr;
    for (const char* p = m_base_path; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            lastSep = p;
        }
    }

    if (lastSep) {
        char dirBuf[256] = {};
        size_t len = static_cast<size_t>(lastSep - m_base_path);
        if (len >= sizeof(dirBuf)) {
            len = sizeof(dirBuf) - 1;
        }
        std::memcpy(dirBuf, m_base_path, len);
        dirBuf[len] = '\0';

#ifdef _WIN32
        _mkdir(dirBuf);
#else
        mkdir(dirBuf, 0755);
#endif
    }

    return openLogFile();
}

void JsonFileOutput::write(const QueuedLogEntry& entry) {
    if (!m_file_opened) {
        return;
    }

    if (!checkAndRoll()) {
        return;
    }

    auto timeT = std::chrono::system_clock::to_time_t(entry.m_timestamp);
    std::tm* tm = std::localtime(&timeT);
    char timeBuf[32] = {};
    std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%dT%H:%M:%S", tm);

    // Build JSON line in a buffer
    char buf[4096];
    int pos = 0;

    pos += std::snprintf(buf + pos, sizeof(buf) - pos,
        "{\"timestamp\":\"%s\",\"level\":\"%s\",",
        timeBuf, logLevelToString(entry.m_level));

    pos += std::snprintf(buf + pos, sizeof(buf) - pos, "\"category\":\"%s\",",
        entry.m_category_name ? entry.m_category_name : "Unknown");

    pos += std::snprintf(buf + pos, sizeof(buf) - pos,
        "\"file\":\"%s\",\"function\":\"%s\",\"message\":\"",
        logFileBasename(entry.m_file),
        entry.m_function ? entry.m_function : "unknown");

    // Write message with JSON escaping
    m_file_stream.write(buf, pos);
    writeJsonEscaped(m_file_stream, entry.m_message.c_str());

    m_file_stream.write("\"}\n", 3);
    m_current_size += static_cast<uint64_t>(pos + 3); // approximate
}

void JsonFileOutput::flush() {
    if (m_file_opened) {
        m_file_stream.flush();
    }
}

bool JsonFileOutput::openLogFile() {
    m_file_stream.open(m_base_path, std::ios::out | std::ios::app);
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

void JsonFileOutput::rollFile() {
    if (!m_file_opened) {
        return;
    }

    m_file_stream.close();
    m_file_opened = false;

    char oldestPath[256];
    std::snprintf(oldestPath, sizeof(oldestPath), "%s.%u", m_base_path, m_max_files);
    std::remove(oldestPath);

    for (int i = static_cast<int>(m_max_files) - 1; i >= 1; --i) {
        char oldPath[256];
        char newPath[256];
        std::snprintf(oldPath, sizeof(oldPath), "%s.%d", m_base_path, i);
        std::snprintf(newPath, sizeof(newPath), "%s.%d", m_base_path, i + 1);
        std::rename(oldPath, newPath);
    }

    char backupPath[256];
    std::snprintf(backupPath, sizeof(backupPath), "%s.1", m_base_path);
    std::rename(m_base_path, backupPath);

    openLogFile();
}

bool JsonFileOutput::checkAndRoll() {
    const uint64_t maxSizeBytes = static_cast<uint64_t>(m_max_size_mb) * 1024 * 1024;
    if (m_current_size >= maxSizeBytes) {
        rollFile();
    }
    return m_file_opened;
}

void JsonFileOutput::writeJsonEscaped(std::ostream& out, const char* str) {
    if (!str) {
        return;
    }
    for (const char* p = str; *p; ++p) {
        switch (*p) {
            case '"': out.write("\\\"", 2); break;
            case '\\': out.write("\\\\", 2); break;
            case '\b': out.write("\\b", 2); break;
            case '\f': out.write("\\f", 2); break;
            case '\n': out.write("\\n", 2); break;
            case '\r': out.write("\\r", 2); break;
            case '\t': out.write("\\t", 2); break;
            default:
                if (static_cast<unsigned char>(*p) < 0x20) {
                    char buf[7];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(*p));
                    out.write(buf, 6);
                } else {
                    out.put(*p);
                }
                break;
        }
    }
}

} // namespace Entelechy
