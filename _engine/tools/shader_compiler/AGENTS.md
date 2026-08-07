# ShaderCompiler — Offline HLSL Cross-Compilation Tool

**Responsibility**: Compile HLSL SM 6.0 shaders to DXIL (D3D12), SPIR-V (Vulkan), and GLSL (OpenGL) via DXC + SPIRV-Cross.

## Key Files

| File | Role |
|------|------|
| `private/main.cpp` | CLI entry point; parses `shaders.json`, orchestrates compilation |
| `private/dxc_compiler.h/.cpp` | DXC library API wrapper (`IDxcCompiler3`) for HLSL→DXIL and HLSL→SPIR-V |
| `private/spirv_cross_compiler.h/.cpp` | SPIRV-Cross C API wrapper for SPIR-V→GLSL cross-compilation |

## Usage

```bash
ShaderCompiler --config <path/to/shaders.json> --output <output_dir>
```

### Compile Config Format (`shaders.json`)

```json
{
  "shaders": [
    {
      "name": "pbr_lit",
      "entry_points": [
        { "stage": "vertex", "file": "pbr_lit_vs.hlsl", "entry": "main", "target": "vs_6_0" },
        { "stage": "pixel",  "file": "pbr_lit_ps.hlsl", "entry": "main", "target": "ps_6_0" }
      ],
      "defines": [],
      "include_dirs": ["."]
    }
  ]
}
```

### Output Per Entry Point

- `{name}_{stage}.dxil` — DXIL bytecode (D3D12)
- `{name}_{stage}.spv` — SPIR-V binary (Vulkan)
- `{name}_{stage}.glsl` — Cross-compiled GLSL source (OpenGL, desktop 410; 4.10+ is required so SPIRV-Cross can emit explicit `layout(location)` on VS outputs / FS inputs — at 3.30 the stage interface links by variable name, and the generated `out_var_*`/`in_var_*` names never match, leaving all FS inputs zero)

## Architecture Decisions

- **HLSL-first**: Single source language targeting SM 6.0. DXC is the primary compiler.
- **DXC integration**: Prebuilt binaries in `third_party/dxc/` discovered via `cmake/FindDXC.cmake`. No Conan recipe available.
- **SPIRV-Cross via Conan**: Static link with C API (`spirv-cross/1.4.350.0`). Used offline only — no runtime dependency.
- **No reflection metadata yet**: CBV/SRV binding reflection deferred to a future iteration when BindGroup system lands.
- **Combined sampler renaming**: After `build_combined_image_samplers`, each GLSL combined sampler is renamed back to its original HLSL texture name (e.g. `uBaseColorTex`). The auto-generated `_<SPIR-V ID>` names follow first-use order, not register order — never bind by them from C++.
- **No hot reload**: Runtime shader recompilation is out of scope (see TODO.md).

## Dependencies

- `CoreLib` — logging, string utilities, file I/O
- `DXC::DXC` — Microsoft DirectX Shader Compiler (imported target from FindDXC.cmake)
- `spirv-cross::spirv-cross` — SPIR-V cross-compiler (Conan)

## Testing

No unit tests (`NO_TESTS`). Acceptance verified by:
1. Compiling all engine shaders without errors
2. Validating DXIL output via `dxc -disasm`
3. Visual regression: Sponza renders identically through SPIRV-Cross GLSL path
