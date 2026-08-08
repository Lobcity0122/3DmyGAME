#include "static_mesh.hlsli" 

//float4 main(VS_OUT pin) : SV_TARGET
//{
//    return pin.color;
//}

Texture2D color_map : register(t0);
Texture2D normal_map : register(t1);
Texture2D<float> shadow_map : register(t3);
SamplerState point_sampler_state : register(s0);
SamplerState linear_sampler_state : register(s1);
SamplerState anisotropic_sampler_state : register(s2);
SamplerComparisonState shadow_sampler : register(s3);

static const float PI = 3.14159265f;

float distribution_ggx(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0f);
    float denominator = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
    return a2 / max(PI * denominator * denominator, 0.0001f);
}

float geometry_schlick_ggx(float NdotV, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    return NdotV / max(NdotV * (1.0f - k) + k, 0.0001f);
}

float geometry_smith(float3 N, float3 V, float3 L, float roughness)
{
    return geometry_schlick_ggx(max(dot(N, V), 0.0f), roughness) *
        geometry_schlick_ggx(max(dot(N, L), 0.0f), roughness);
}

float3 fresnel_schlick(float cosine, float3 F0)
{
    return F0 + (1.0f - F0) * pow(1.0f - cosine, 5.0f);
}

float3 aces_tonemap(float3 color)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return saturate((color * (a * color + b)) / (color * (c * color + d) + e));
}

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 color = color_map.Sample(anisotropic_sampler_state, pin.texcoord);
    float  alpha = color.a;

    // This debug view bypasses lighting so every material texture is easy to inspect.
    if (render_options.y > 0.5f)
    {
        return float4(color.rgb * pin.color.rgb, alpha * pin.color.a);
    }

    float3 N = normalize(pin.world_normal.xyz);
    
    float3 T = float3(1.0001, 0, 0);
    float3 B = normalize(cross(N, T));
    T = normalize(cross(B, N));
    
    float4 normal = normal_map.Sample(linear_sampler_state, pin.texcoord);
    normal = (normal * 2.0) - 1.0;
    normal.w = 0;
    N = normalize((normal.x * T) + (normal.y * B) + (normal.z * N));
    
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

    float3 V = normalize(camera_position.xyz - pin.world_position.xyz);
    float3 linear_albedo = pow(saturate(color.rgb * pin.color.rgb), 2.2f);

    if (render_options.z < 0.5f)
    {
        float3 legacy = linear_albedo * (ambient_color_intensity.rgb * ambient_color_intensity.w +
            max(0.0f, dot(N, L)) * attenuation * shadow * light_color_intensity.rgb * light_color_intensity.w);
        legacy *= exp2(post_process_settings.x);
        return float4(pow(aces_tonemap(legacy), 1.0f / 2.2f), alpha * pin.color.a);
    }

    float metallic = saturate(material_params.x);
    float roughness = clamp(material_params.y, 0.08f, 1.0f);
    float3 H = normalize(V + L);
    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), linear_albedo, metallic);
    float3 F = fresnel_schlick(max(dot(H, V), 0.0f), F0);
    float D = distribution_ggx(N, H, roughness);
    float G = geometry_smith(N, V, L, roughness);
    float3 specular = (D * G * F) / max(4.0f * max(dot(N, V), 0.0f) * max(dot(N, L), 0.0f), 0.0001f);
    float3 kD = (1.0f - F) * (1.0f - metallic);
    float3 radiance = light_color_intensity.rgb * light_color_intensity.w * attenuation * shadow;
    float3 direct = (kD * linear_albedo / PI + specular) * radiance * max(dot(N, L), 0.0f);
    float3 ambient = linear_albedo * ambient_color_intensity.rgb * ambient_color_intensity.w * (1.0f - metallic);
    float3 final_color = (ambient + direct) * exp2(post_process_settings.x);
    return float4(pow(aces_tonemap(final_color), 1.0f / 2.2f), alpha * pin.color.a);
}
