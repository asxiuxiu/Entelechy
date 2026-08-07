// Simple Cube Pixel Shader — HLSL SM 6.0
// Solid color output for demo geometry.

cbuffer PerMaterial : register(b0)
{
    float3 uColor;
    float  _pad0;
};

float4 main() : SV_Target0
{
    return float4(uColor, 1.0);
}
