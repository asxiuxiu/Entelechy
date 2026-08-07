// ShaderCompiler — Offline HLSL cross-compilation tool.
// Reads shaders.json config, compiles each entry point to DXIL + SPIR-V + GLSL.

#include "dxc_compiler.h"
#include "spirv_cross_compiler.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace
{

struct EntryPoint
{
    std::string stage;   // "vertex" or "pixel"
    std::string file;    // source file relative to config dir
    std::string entry;   // entry point function name
    std::string target;  // e.g. "vs_6_0", "ps_6_0"
};

struct ShaderConfig
{
    std::string name;
    std::vector<EntryPoint> entry_points;
    std::vector<std::string> defines;
    std::vector<std::string> include_dirs;
};

// Minimal JSON parser for shaders.json (no external dependency needed).
// Supports the specific schema used by this tool.
std::string readFile(const char *path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open())
        return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool writeFile(const char *path, const void *data, size_t size)
{
    std::ofstream f(path, std::ios::binary);
    if (!f.is_open())
        return false;
    f.write(static_cast<const char *>(data), static_cast<std::streamsize>(size));
    return f.good();
}

bool writeTextFile(const char *path, const std::string &text)
{
    return writeFile(path, text.data(), text.size());
}

// Skip whitespace in JSON string
size_t skipWs(const std::string &s, size_t pos)
{
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' || s[pos] == '\r'))
        ++pos;
    return pos;
}

// Parse a JSON string value (after opening quote)
std::string parseJsonString(const std::string &s, size_t &pos)
{
    if (pos >= s.size() || s[pos] != '"')
        return {};
    ++pos; // skip opening quote
    std::string result;
    while (pos < s.size() && s[pos] != '"')
    {
        if (s[pos] == '\\' && pos + 1 < s.size())
        {
            ++pos;
            switch (s[pos])
            {
            case '"': result += '"'; break;
            case '\\': result += '\\'; break;
            case 'n': result += '\n'; break;
            case 't': result += '\t'; break;
            default: result += s[pos]; break;
            }
        }
        else
        {
            result += s[pos];
        }
        ++pos;
    }
    if (pos < s.size())
        ++pos; // skip closing quote
    return result;
}

// Parse array of strings
std::vector<std::string> parseJsonStringArray(const std::string &s, size_t &pos)
{
    std::vector<std::string> result;
    pos = skipWs(s, pos);
    if (pos >= s.size() || s[pos] != '[')
        return result;
    ++pos; // skip '['
    pos = skipWs(s, pos);
    while (pos < s.size() && s[pos] != ']')
    {
        if (s[pos] == '"')
        {
            result.push_back(parseJsonString(s, pos));
        }
        else
        {
            ++pos; // skip comma or whitespace
        }
        pos = skipWs(s, pos);
    }
    if (pos < s.size())
        ++pos; // skip ']'
    return result;
}

// Parse an entry_point object
EntryPoint parseEntryPoint(const std::string &s, size_t &pos)
{
    EntryPoint ep;
    pos = skipWs(s, pos);
    if (pos >= s.size() || s[pos] != '{')
        return ep;
    ++pos; // skip '{'
    pos = skipWs(s, pos);
    while (pos < s.size() && s[pos] != '}')
    {
        if (s[pos] == '"')
        {
            std::string key = parseJsonString(s, pos);
            pos = skipWs(s, pos);
            if (pos < s.size() && s[pos] == ':')
                ++pos;
            pos = skipWs(s, pos);
            std::string val = parseJsonString(s, pos);
            if (key == "stage") ep.stage = val;
            else if (key == "file") ep.file = val;
            else if (key == "entry") ep.entry = val;
            else if (key == "target") ep.target = val;
        }
        else
        {
            ++pos;
        }
        pos = skipWs(s, pos);
    }
    if (pos < s.size())
        ++pos; // skip '}'
    return ep;
}

// Parse array of entry_point objects
std::vector<EntryPoint> parseEntryPoints(const std::string &s, size_t &pos)
{
    std::vector<EntryPoint> result;
    pos = skipWs(s, pos);
    if (pos >= s.size() || s[pos] != '[')
        return result;
    ++pos; // skip '['
    pos = skipWs(s, pos);
    while (pos < s.size() && s[pos] != ']')
    {
        if (s[pos] == '{')
        {
            result.push_back(parseEntryPoint(s, pos));
        }
        else
        {
            ++pos;
        }
        pos = skipWs(s, pos);
    }
    if (pos < s.size())
        ++pos; // skip ']'
    return result;
}

// Parse a single shader config object
ShaderConfig parseShaderConfig(const std::string &s, size_t &pos)
{
    ShaderConfig cfg;
    pos = skipWs(s, pos);
    if (pos >= s.size() || s[pos] != '{')
        return cfg;
    ++pos; // skip '{'
    pos = skipWs(s, pos);
    while (pos < s.size() && s[pos] != '}')
    {
        if (s[pos] == '"')
        {
            std::string key = parseJsonString(s, pos);
            pos = skipWs(s, pos);
            if (pos < s.size() && s[pos] == ':')
                ++pos;
            pos = skipWs(s, pos);
            if (key == "name")
            {
                cfg.name = parseJsonString(s, pos);
            }
            else if (key == "entry_points")
            {
                cfg.entry_points = parseEntryPoints(s, pos);
            }
            else if (key == "defines")
            {
                cfg.defines = parseJsonStringArray(s, pos);
            }
            else if (key == "include_dirs")
            {
                cfg.include_dirs = parseJsonStringArray(s, pos);
            }
            else
            {
                // Skip unknown value — find next comma or brace
                while (pos < s.size() && s[pos] != ',' && s[pos] != '}')
                    ++pos;
            }
        }
        else
        {
            ++pos;
        }
        pos = skipWs(s, pos);
    }
    if (pos < s.size())
        ++pos; // skip '}'
    return cfg;
}

std::vector<ShaderConfig> parseShadersJson(const std::string &json)
{
    std::vector<ShaderConfig> configs;
    size_t pos = 0;
    pos = skipWs(json, pos);
    if (pos >= json.size() || json[pos] != '{')
        return configs;
    ++pos; // skip '{'
    pos = skipWs(json, pos);
    while (pos < json.size() && json[pos] != '}')
    {
        if (json[pos] == '"')
        {
            std::string key = parseJsonString(json, pos);
            pos = skipWs(json, pos);
            if (pos < json.size() && json[pos] == ':')
                ++pos;
            pos = skipWs(json, pos);
            if (key == "shaders")
            {
                // Parse array of shader configs
                if (pos < json.size() && json[pos] == '[')
                {
                    ++pos; // skip '['
                    pos = skipWs(json, pos);
                    while (pos < json.size() && json[pos] != ']')
                    {
                        if (json[pos] == '{')
                        {
                            configs.push_back(parseShaderConfig(json, pos));
                        }
                        else
                        {
                            ++pos;
                        }
                        pos = skipWs(json, pos);
                    }
                    if (pos < json.size())
                        ++pos; // skip ']'
                }
            }
            else
            {
                while (pos < json.size() && json[pos] != ',' && json[pos] != '}')
                    ++pos;
            }
        }
        else
        {
            ++pos;
        }
        pos = skipWs(json, pos);
    }
    return configs;
}

std::string getDirectory(const std::string &path)
{
    size_t sep = path.find_last_of("/\\");
    if (sep == std::string::npos)
        return ".";
    return path.substr(0, sep);
}

void printUsage()
{
    fprintf(stderr, "Usage: ShaderCompiler --config <shaders.json> --output <output_dir>\n");
}

} // anonymous namespace

int main(int argc, char *argv[])
{
    const char *configPath = nullptr;
    const char *outputDir = nullptr;

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc)
            configPath = argv[++i];
        else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc)
            outputDir = argv[++i];
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
        {
            printUsage();
            return 0;
        }
    }

    if (!configPath || !outputDir)
    {
        printUsage();
        return 1;
    }

    // Read and parse config
    std::string configJson = readFile(configPath);
    if (configJson.empty())
    {
        fprintf(stderr, "[ShaderCompiler] Failed to read config: %s\n", configPath);
        return 1;
    }

    auto configs = parseShadersJson(configJson);
    if (configs.empty())
    {
        fprintf(stderr, "[ShaderCompiler] No shaders found in config\n");
        return 1;
    }

    std::string configDir = getDirectory(configPath);

    // Initialize compilers
    Entelechy::DxcCompiler dxc;
    if (!dxc.isValid())
    {
        fprintf(stderr, "[ShaderCompiler] Failed to initialize DXC compiler\n");
        return 1;
    }

    Entelechy::SpirvCrossCompiler spirvCross;
    if (!spirvCross.isValid())
    {
        fprintf(stderr, "[ShaderCompiler] Failed to initialize SPIRV-Cross compiler\n");
        return 1;
    }

    int errorCount = 0;
    int compiledCount = 0;

    for (const auto &shader : configs)
    {
        printf("[ShaderCompiler] Processing shader: %s\n", shader.name.c_str());

        for (const auto &ep : shader.entry_points)
        {
            // Read source file
            std::string sourcePath = configDir + "/" + ep.file;
            std::string source = readFile(sourcePath.c_str());
            if (source.empty())
            {
                fprintf(stderr, "  [ERROR] Cannot read source: %s\n", sourcePath.c_str());
                ++errorCount;
                continue;
            }

            printf("  Compiling %s (%s, %s)...\n", ep.file.c_str(), ep.stage.c_str(), ep.target.c_str());

            // Compile to DXIL
            auto dxilResult = dxc.compileToDxil(source.c_str(), source.size(),
                                                ep.entry.c_str(), ep.target.c_str(),
                                                ep.file.c_str());
            if (!dxilResult.success)
            {
                fprintf(stderr, "  [ERROR] DXIL compilation failed:\n%s\n",
                        dxilResult.error_message.c_str());
                ++errorCount;
                continue;
            }

            // Compile to SPIR-V
            auto spirvResult = dxc.compileToSpirv(source.c_str(), source.size(),
                                                  ep.entry.c_str(), ep.target.c_str(),
                                                  ep.file.c_str());
            if (!spirvResult.success)
            {
                fprintf(stderr, "  [ERROR] SPIR-V compilation failed:\n%s\n",
                        spirvResult.error_message.c_str());
                ++errorCount;
                continue;
            }

            // Cross-compile SPIR-V to GLSL
            auto glslResult = spirvCross.compileToGlsl(spirvResult.bytecode.data(),
                                                       spirvResult.bytecode.size());
            if (!glslResult.success)
            {
                fprintf(stderr, "  [ERROR] SPIRV-Cross GLSL generation failed:\n%s\n",
                        glslResult.error_message.c_str());
                ++errorCount;
                continue;
            }

            // Write outputs
            std::string baseName = std::string(outputDir) + "/" + shader.name + "_" + ep.stage;

            std::string dxilPath = baseName + ".dxil";
            std::string spvPath = baseName + ".spv";
            std::string glslPath = baseName + ".glsl";

            if (!writeFile(dxilPath.c_str(), dxilResult.bytecode.data(), dxilResult.bytecode.size()))
            {
                fprintf(stderr, "  [ERROR] Failed to write: %s\n", dxilPath.c_str());
                ++errorCount;
                continue;
            }
            if (!writeFile(spvPath.c_str(), spirvResult.bytecode.data(), spirvResult.bytecode.size()))
            {
                fprintf(stderr, "  [ERROR] Failed to write: %s\n", spvPath.c_str());
                ++errorCount;
                continue;
            }
            if (!writeTextFile(glslPath.c_str(), glslResult.glsl_source))
            {
                fprintf(stderr, "  [ERROR] Failed to write: %s\n", glslPath.c_str());
                ++errorCount;
                continue;
            }

            printf("    -> %s (%zu bytes DXIL, %zu bytes SPIR-V, %zu chars GLSL)\n",
                   ep.stage.c_str(),
                   dxilResult.bytecode.size(),
                   spirvResult.bytecode.size(),
                   glslResult.glsl_source.size());
            ++compiledCount;
        }
    }

    printf("[ShaderCompiler] Done: %d compiled, %d errors\n", compiledCount, errorCount);
    return errorCount > 0 ? 1 : 0;
}
