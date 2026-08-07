// Sky Gradient Vertex Shader — HLSL SM 6.0
// Fullscreen NDC quad; reconstructs far-plane world position via inverse view-projection.

struct VSInput
{
    float2 aNDC : POSITION;
};

struct VSOutput
{
    float4 svPosition : SV_Position;
    float3 vFarPos    : TEXCOORD0;
};

cbuffer PerFrame : register(b0)
{
    float4x4 uInvViewProj;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.svPosition = float4(input.aNDC, 1.0, 1.0);
    float4 world = mul(uInvViewProj, float4(input.aNDC, 1.0, 1.0));
    output.vFarPos = world.xyz / world.w;
    return output;
}
