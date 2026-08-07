#include "render/screenshot/screenshot.h"
#include "log/core/log_macros.h"

#include <filesystem>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace Entelechy
{

bool saveScreenshotPng(const char *path, const u8 *pixelsRGBA8, u32 width, u32 height)
{
    if (!path || !pixelsRGBA8 || width == 0 || height == 0)
    {
        LOG_ERROR(LogCategories::kEngine, "saveScreenshotPng: invalid arguments");
        return false;
    }

    std::error_code ec;
    const std::filesystem::path parent = std::filesystem::path(path).parent_path();
    if (!parent.empty())
        std::filesystem::create_directories(parent, ec);

    // stb_image_write expects rows top-down, which matches our convention.
    if (!stbi_write_png(path, static_cast<int>(width), static_cast<int>(height), 4, pixelsRGBA8,
                        static_cast<int>(width * 4)))
    {
        LOG_ERROR(LogCategories::kEngine, "saveScreenshotPng: failed to write %s", path);
        return false;
    }

    LOG_INFO(LogCategories::kEngine, "Screenshot saved: %s (%ux%u)", path, width, height);
    return true;
}

} // namespace Entelechy
