#include "skinned_mesh.hlsli"
#define POINT 0 
#define LINEAR 1 
#define ANISOTROPIC 2 

SamplerState sampler_states[3] : register(s0);
Texture2D texture_maps[4] : register(t0);
Texture2D<float> shadow_map : register(t3);
SamplerComparisonState shadow_sampler : register(s3);

static const float PI = 3.14159265f;

float3 aces_tonemap(float3 color)
{
    return saturate((color * (2.51f * color + 0.03f)) / (color * (2.43f * color + 0.59f) + 0.14f));
}

float3 fresnel_schlick(float cosine, float3 F0)
{
    return F0 + (1.0f - F0) * pow(1.0f - cosine, 5.0f);
}
 
float4 main(VS_OUT pin) : SV_TARGET
{ 
    float4 color = texture_maps[0].Sample(sampler_states[ANISOTROPIC], pin.texcoord);
    if (render_options.y > 0.5f)
    {
        return color * pin.color;
    }

    float3 N = normalize(pin.world_normal.xyz);
    float3 L;
    float attenuation = 1.0f;
    if (render_options.x > 0.5f)
    {
        float3 to_light = light_position_range.xyz - pin.world_position.xyz;
        float distance_to_light = length(to_light);
        L = to_light / max(distance_to_light, 0.0001f);
        attenuation = saturate(1.0f - distance_to_light / max(light_position_range.w, 0.0001f));
        attenuation *= attenuation;
    }
    else
    {
        L = normalize(-light_direction.xyz);
    }
    float shadow = 1.0f;
    if (shadow_settings.y > 0.5f)
    {
        float4 shadow_position = mul(pin.world_position, light_view_projection);
        float3 shadow_ndc = shadow_position.xyz / max(shadow_position.w, 0.0001f);
        float2 shadow_uv = float2(shadow_ndc.x * 0.5f + 0.5f, -shadow_ndc.y * 0.5f + 0.5f);
        if (all(shadow_uv >= 0.0f) && all(shadow_uv <= 1.0f) && shadow_ndc.z >= 0.0f && shadow_ndc.z <= 1.0f)
        {
            shadow = shadow_map.SampleCmpLevelZero(shadow_sampler, shadow_uv, shadow_ndc.z - shadow_settings.x);
        }
    }
    float3 albedo = pow(saturate(color.rgb * pin.color.rgb), 2.2f);
    float3 V = normalize(camera_position.xyz - pin.world_position.xyz);
    float3 H = normalize(V + L);
    float metallic = saturate(material_params.x);
    float roughness = clamp(material_params.y, 0.08f, 1.0f);
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0f);
    float D = a2 / max(PI * pow(NdotH * NdotH * (a2 - 1.0f) + 1.0f, 2.0f), 0.0001f);
    float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);
    float k = pow(roughness + 1.0f, 2.0f) / 8.0f;
    float G = (NdotV / max(NdotV * (1.0f - k) + k, 0.0001f)) *
        (NdotL / max(NdotL * (1.0f - k) + k, 0.0001f));
    float3 F = fresnel_schlick(max(dot(H, V), 0.0f), lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic));
    float3 specular = (D * G * F) / max(4.0f * NdotV * NdotL, 0.0001f);
    float3 direct = ((1.0f - F) * (1.0f - metallic) * albedo / PI + specular) *
        light_color_intensity.rgb * light_color_intensity.w * attenuation * shadow * NdotL;
    float3 ambient = albedo * ambient_color_intensity.rgb * ambient_color_intensity.w * (1.0f - metallic);
    float3 final_color = (ambient + direct) * exp2(post_process_settings.x);
    return float4(pow(aces_tonemap(final_color), 1.0f / 2.2f), color.a * pin.color.a);
}
