# ShaderCompiler — Offline HLSL Cross-Compilation Tool

**Responsibility**: Compile HLSL SM 6.0 shaders to DXIL (D3D12), SPIR-V (Vulkan), and GLSL (OpenGL) via DXC + SPIRV-Cross.

## Key Files

| File | Role |
|------|------|
| `private/main.cpp` | CLI entry point; parses `shaders.json`, orchestrates compilation |
| `private/dxc_compiler.h/.cpp` | DXC library API wrapper (`IDxcCompiler3`) for HLSL→DXIL and HLSL→SPIR-V |
| `private/spirv_cross_compiler.h/.cpp` | SPIRV-Cross C API wrapper for SPIR-V→GLSL cross-compilation + cbuffer/texture reflection JSON dump |

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
- `{name}_{stage}.glsl` — Cross-compiled GLSL source (OpenGL, desktop 410 + 420pack extension; 4.10+ is required so SPIRV-Cross can emit explicit `layout(location)` on VS outputs / FS inputs — at 3.30 the stage interface links by variable name, and the generated `out_var_*`/`in_var_*` names never match, leaving all FS inputs zero)
- `{name}_{stage}_reflection.json` — cbuffer/texture binding layout (6e): per cbuffer its binding point (b-register), std140-padded size and member list (name/type/offset/size); per texture its t-register. Consumed by `ShaderReflection` at runtime to build the CPU constant blob and BindGroup without any string matching.

## Architecture Decisions

- **HLSL-first**: Single source language targeting SM 6.0. DXC is the primary compiler.
- **DXC integration**: Prebuilt binaries in `third_party/dxc/` discovered via `cmake/FindDXC.cmake`. No Conan recipe available. Only headers are committed; `bin/` and `lib/` are gitignored and auto-downloaded by `scripts/tools/ensure_dxc.py` (invoked from `setup_env.py` and `build.py`; version pinned in `configs/environment.json`).
- **SPIRV-Cross via Conan**: Static link with C API (`spirv-cross/1.4.350.0`). Used offline only — no runtime dependency.
- **cbuffers stay real UBOs (6e)**: SPIRV-Cross no longer flattens cbuffer blocks to plain uniforms. The emitted GLSL declares `layout(binding = N, std140) uniform <block>` blocks, bound at runtime with `glBindBufferRange`. The 420pack extension is enabled so UBOs and samplers get explicit `layout(binding = N)` (GLSL stays 410). Binding points must be **unique across stages within a material** (GL shares one `GL_UNIFORM_BUFFER` namespace), and cbuffer members should be **vec4/mat4-only** so HLSL packing (D3D12) and std140 (GL) agree on one shared CPU blob — see the `.hlsl` comments for the constraint.
- **Combined sampler binding inheritance**: After `build_combined_image_samplers`, each combined sampler is renamed back to its original HLSL texture name AND inherits the source image's `Binding` decoration, so the GLSL sampler carries `layout(binding = N)` (N == the t-register == the texture unit). No sampler uniform location lookups are needed.
- **Reflection metadata**: cbuffer member layout is dumped to `_reflection.json` from the SPIR-V (DXC HLSL packing, identical to DXIL for the vec4/mat4-only members) — member offsets come from the `Offset` decorations via `spvc_compiler_get_member_decoration`.
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
