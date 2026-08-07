#include "skinned_mesh.hlsli"
#define POINT 0 
#define LINEAR 1 
#define ANISOTROPIC 2 

SamplerState sampler_states[3] : register(s0);
Texture2D texture_maps[4] : register(t0);
 
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
    float3 diffuse = color.rgb * max(0.05f, dot(N, L)) * attenuation * light_color_intensity.rgb * light_color_intensity.w;
    return float4(diffuse, color.a) * pin.color;
}
