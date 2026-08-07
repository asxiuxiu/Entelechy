// Sky Gradient Pixel Shader — HLSL SM 6.0
// Vertical gradient from horizon to zenith color with gamma correction.

struct PSInput
{
    float4 svPosition : SV_Position;
    float3 vFarPos    : TEXCOORD0;
};

// Named differently from the vertex stage's PerFrame: after SPIRV-Cross
// flattening both would emit `uniform vec4 type_PerFrame[N]` with different
// lengths in the same GL program namespace (link-time conflict + the CPU
// side would clobber the VS matrix rows with PS colors).
cbuffer PerFramePS : register(b0)
{
    float3 uViewPos;
    float  _pad0;
    float3 uHorizonColor;
    float  _pad1;
    float3 uZenithColor;
    float  _pad2;
};

float4 main(PSInput input) : SV_Target0
{
    float3 dir = normalize(input.vFarPos - uViewPos);
    float t = clamp(dir.y, 0.0, 1.0);
    float3 color = lerp(uHorizonColor, uZenithColor, t);
    return float4(pow(color, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2)), 1.0);
}
