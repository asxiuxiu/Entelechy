// Simple Cube Pixel Shader — HLSL SM 6.0
// Solid color output for demo geometry.

// Cbuffer layout constraint (6e): every cbuffer member is a float4/float4x4
// so that HLSL constant-buffer packing (DXC) and std140 (GL UBO) agree
// exactly. Bound at b1 (the VS PerDraw block uses b0): the GL backend
// shares one GL_UNIFORM_BUFFER binding namespace across stages, so a
// material's cbuffers must have globally unique binding points.
cbuffer PerMaterial : register(b1)
{
    float4 uColor; // xyz = color, w unused
};

float4 main() : SV_Target0
{
    return float4(uColor.xyz, 1.0);
}
