#include "asset/loader/texture_asset_loader.h"
#include "log/core/log_macros.h"
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace Entelechy
{

namespace
{
constexpr LogCategory kLogAsset("Asset");
}

TextureAsset TextureAssetLoader::load(const FileData &data, const Path &path)
{
    TextureAsset texture;
    if (!data.valid || data.bytes.size() == 0)
    {
        LOG_ERROR(kLogAsset, "TextureAssetLoader: empty or invalid file data for '%s'", path.c_str());
        return texture;
    }

    int w = 0;
    int h = 0;
    int channels = 0;
    stbi_uc *decoded = stbi_load_from_memory(data.bytes.data(), static_cast<int>(data.bytes.size()), &w, &h,
                                             &channels, STBI_rgb_alpha);
    if (!decoded)
    {
        LOG_ERROR(kLogAsset, "TextureAssetLoader: failed to decode '%s': %s", path.c_str(), stbi_failure_reason());
        return texture;
    }

    texture.width = static_cast<u32>(w);
    texture.height = static_cast<u32>(h);
    const usize byteCount = static_cast<usize>(w) * static_cast<usize>(h) * 4;
    texture.pixels.resize(byteCount);
    std::memcpy(texture.pixels.data(), decoded, byteCount);
    stbi_image_free(decoded);
    return texture;
}

} // namespace Entelechy
