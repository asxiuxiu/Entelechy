// PBR Lit Vertex Shader — HLSL SM 6.0
// Cook-Torrance BRDF lighting with normal mapping and tangent-space TBN.

struct VSInput
{
    float3 aPos     : POSITION;
    float3 aNormal  : NORMAL;
    float2 aUV      : TEXCOORD0;
    float4 aTangent : TANGENT; // xyz = tangent, w = handedness (+/-1)
};

struct VSOutput
{
    float4 svPosition : SV_Position;
    float3 vWorldPos  : TEXCOORD0;
    float3 vNormal    : TEXCOORD1;
    float3 vTangent   : TEXCOORD2;
    float  vTangentW  : TEXCOORD3;
    float2 vUV        : TEXCOORD4;
};

cbuffer PerDraw : register(b2)
{
    float4x4 uMVP;
    float4x4 uModel;
    float3x3 uNormalMatrix;
};

VSOutput main(VSInput input)
{
    VSOutput output;

    output.vWorldPos = mul(uModel, float4(input.aPos, 1.0)).xyz;

    // N goes through the normal matrix (inverse-transpose),
    // T through the plain model matrix, then T is re-orthogonalized
    // against N (Gram-Schmidt) — B is rebuilt in the fragment shader.
    float3 N = mul(uNormalMatrix, input.aNormal);
    float3 T = mul((float3x3)uModel, input.aTangent.xyz);
    T = normalize(T - dot(T, N) * N);

    output.vNormal   = N;
    output.vTangent  = T;
    output.vTangentW = input.aTangent.w;
    output.vUV       = input.aUV;
    output.svPosition = mul(uMVP, float4(input.aPos, 1.0));

    return output;
}
