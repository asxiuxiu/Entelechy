// PBR Lit Pixel Shader — HLSL SM 6.0
// Cook-Torrance BRDF: GGX NDF, Schlick-GGX geometry, Schlick fresnel.
// Supports base color, normal map, and metallic-roughness textures.

// Cbuffer layout constraint (6e): every cbuffer member is a float4/float4x4
// so that HLSL constant-buffer packing (DXC) and std140 (GL UBO) agree
// exactly — the CPU uploads ONE shared layout blob for both backends.
// Scalars ride in the .w component of their vector.

struct PSInput
{
    float4 svPosition : SV_Position;
    float3 vWorldPos  : TEXCOORD0;
    float3 vNormal    : TEXCOORD1;
    float3 vTangent   : TEXCOORD2;
    float  vTangentW  : TEXCOORD3;
    float2 vUV        : TEXCOORD4;
};

cbuffer PerFrame : register(b0)
{
    float4 uViewPos;    // xyz = camera position, w unused
    float4 uLightDir;   // xyz = light travel direction, w = light intensity
    float4 uLightColor; // xyz = light color, w = ambient
};

cbuffer PerMaterial : register(b1)
{
    float4 uColor;   // xyz = base color, w = metallic factor
    float4 uAlphaMR; // x = roughness, y = alpha cutoff, z = has normal tex, w = has MR tex
};

Texture2D    uBaseColorTex : register(t0);
SamplerState uBaseColorSampler : register(s0);
Texture2D    uNormalTex : register(t1);
SamplerState uNormalSampler : register(s1);
Texture2D    uMRTex : register(t2);
SamplerState uMRSampler : register(s2);

static const float PI = 3.14159265359;

// D: GGX / Trowbridge-Reitz.
float distributionGGX(float3 N, float3 H, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

// G: Schlick-GGX (direct-lighting k).
float geometrySchlickGGX(float NdotX, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotX / (NdotX * (1.0 - k) + k);
}

float geometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    return geometrySchlickGGX(max(dot(N, V), 0.0), roughness) *
           geometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}

// F: Schlick approximation.
float3 fresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float4 main(PSInput input, bool isFrontFace : SV_IsFrontFace) : SV_Target0
{
    float4 tex = uBaseColorTex.Sample(uBaseColorSampler, input.vUV);
    if (uAlphaMR.y > 0.0 && tex.a < uAlphaMR.y)
        discard;

    float3 albedo = pow(uColor.xyz * tex.rgb, float3(2.2, 2.2, 2.2));
    float metallic  = uColor.w;
    float roughness = uAlphaMR.x;
    if (uAlphaMR.w > 0.5)
    {
        // glTF metallicRoughnessTexture: G = roughness, B = metallic,
        // multiplied with the factors.
        float3 mr = uMRTex.Sample(uMRSampler, input.vUV).rgb;
        metallic  *= mr.b;
        roughness *= mr.g;
    }
    metallic  = clamp(metallic, 0.0, 1.0);
    roughness = clamp(roughness, 0.05, 1.0);

    float3 N = normalize(input.vNormal);
    if (uAlphaMR.z > 0.5)
    {
        // B = cross(N,T) * handedness; the normal map is
        // tangent-space, so N = TBN * (tex*2-1).
        float3 T = normalize(input.vTangent - dot(input.vTangent, N) * N);
        float3 B = cross(N, T) * input.vTangentW;
        float3x3 TBN = float3x3(T, B, N);
        N = normalize(mul(uNormalTex.Sample(uNormalSampler, input.vUV).xyz * 2.0 - 1.0, transpose(TBN)));
    }
    if (!isFrontFace)
        N = -N; // double-sided materials: flip normals on backfaces

    float3 V = normalize(uViewPos.xyz - input.vWorldPos);
    float3 L = normalize(-uLightDir.xyz);
    float3 H = normalize(V + L);

    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);

    float NdotL = max(dot(N, L), 0.0);
    float3 radiance = uLightColor.xyz * uLightDir.w;

    float NDF = distributionGGX(N, H, roughness);
    float G   = geometrySmith(N, V, L, roughness);
    float3 F  = fresnelSchlick(max(dot(H, V), 0.0), F0);

    float3 specular = (NDF * G * F) / (4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001);

    float3 kS = F;
    float3 kD = (float3(1.0, 1.0, 1.0) - kS) * (1.0 - metallic);

    float3 Lo    = (kD * albedo / PI + specular) * radiance * NdotL;
    float3 color = uLightColor.w * albedo + Lo;

    return float4(pow(color, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2)), 1.0);
}
