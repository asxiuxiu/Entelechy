// Sky Gradient Pixel Shader — HLSL SM 6.0
// Vertical gradient from horizon to zenith color with gamma correction.

// Cbuffer layout constraint (6e): every cbuffer member is a float4/float4x4
// so that HLSL constant-buffer packing (DXC) and std140 (GL UBO) agree
// exactly. Bound at b1 (the VS PerFrame block uses b0): the GL backend
// shares one GL_UNIFORM_BUFFER binding namespace across stages, so a
// material's cbuffers must have globally unique binding points.
cbuffer PerFramePS : register(b1)
{
    float4 uViewPos;      // xyz = camera position, w unused
    float4 uHorizonColor; // xyz = horizon color
    float4 uZenithColor;  // xyz = zenith color
};

struct PSInput
{
    float4 svPosition : SV_Position;
    float3 vFarPos    : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target0
{
    float3 dir = normalize(input.vFarPos - uViewPos.xyz);
    float t = clamp(dir.y, 0.0, 1.0);
    float3 color = lerp(uHorizonColor.xyz, uZenithColor.xyz, t);
    return float4(pow(color, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2)), 1.0);
}
