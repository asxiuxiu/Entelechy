// Simple Cube Vertex Shader — HLSL SM 6.0
// Minimal MVP transform for demo geometry.

struct VSInput
{
    float3 aPos : POSITION;
};

struct VSOutput
{
    float4 svPosition : SV_Position;
};

cbuffer PerDraw : register(b0)
{
    float4x4 uMVP;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.svPosition = mul(uMVP, float4(input.aPos, 1.0));
    return output;
}
