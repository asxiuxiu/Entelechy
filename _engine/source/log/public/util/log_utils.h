#pragma once

namespace Entelechy
{

// Extracts the basename (filename only) from a full file path.
// Returns the original pointer if no path separator is found.
inline const char *logFileBasename(const char *filePath)
{
    if (!filePath)
    {
        return "unknown";
    }
    const char *basename = filePath;
    for (const char *p = filePath; *p; ++p)
    {
        if (*p == '/' || *p == '\\')
        {
            basename = p + 1;
        }
    }
    return basename;
}

} // namespace Entelechy
