#include "asset/loader/material_asset_loader.h"
#include "core/json/json_cursor.h"
#include "log/core/log_macros.h"

namespace Entelechy
{

namespace
{
constexpr LogCategory kLogAsset("Asset");
}

MaterialAsset MaterialAssetLoader::load(const FileData &data, const Path &path)
{
    MaterialAsset material;
    if (!data.valid || data.bytes.size() == 0)
    {
        LOG_ERROR(kLogAsset, "MaterialAssetLoader: empty or invalid file data for '%s'", path.c_str());
        return material;
    }

    // Null-terminated copy so strtof always stops inside the buffer.
    const String text(reinterpret_cast<const char *>(data.bytes.data()), data.bytes.size());
    JsonCursor cur{text.c_str(), 0, text.length()};

    String key;
    String strValue;
    bool ok = cur.consume('{');
    while (ok)
    {
        if (cur.consume('}'))
            break;
        if (!cur.parseString(key) || !cur.consume(':'))
        {
            ok = false;
            break;
        }

        if (key == "base_color_texture")
            ok = cur.parseString(material.base_color_texture_path);
        else if (key == "normal_texture")
            ok = cur.parseString(material.normal_texture_path);
        else if (key == "mr_texture")
            ok = cur.parseString(material.mr_texture_path);
        else if (key == "base_color_factor")
            ok = cur.parseFloatArray(&material.base_color.x, 3);
        else if (key == "metallic_factor")
            ok = cur.parseFloat(material.metallic_factor);
        else if (key == "roughness_factor")
            ok = cur.parseFloat(material.roughness_factor);
        else if (key == "alpha_cutoff")
            ok = cur.parseFloat(material.alpha_cutoff);
        else if (key == "double_sided")
            ok = cur.parseBool(material.double_sided);
        else if (key == "alpha_mode")
        {
            ok = cur.parseString(strValue);
            if (ok)
            {
                if (strValue == "opaque")
                    material.alpha_mode = AlphaMode::Opaque;
                else if (strValue == "mask")
                    material.alpha_mode = AlphaMode::Mask;
                else if (strValue == "blend")
                    material.alpha_mode = AlphaMode::Blend;
                else
                    ok = false;
            }
        }
        else
        {
            // Fixed schema: unknown keys are rejected, not skipped.
            ok = false;
        }

        if (!ok)
            break;
        if (cur.consume(','))
            continue;
        if (cur.consume('}'))
            break;
        ok = false;
    }

    if (!ok)
    {
        LOG_ERROR(kLogAsset, "MaterialAssetLoader: malformed .emat in '%s' (not a cooker material file?)",
                  path.c_str());
        return MaterialAsset{};
    }
    return material;
}

} // namespace Entelechy
