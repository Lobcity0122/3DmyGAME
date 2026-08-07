#include "static_mesh.hlsli" 

//float4 main(VS_OUT pin) : SV_TARGET
//{
//    return pin.color;
//}

Texture2D color_map : register(t0);
Texture2D normal_map : register(t1);
SamplerState point_sampler_state : register(s0);
SamplerState linear_sampler_state : register(s1);
SamplerState anisotropic_sampler_state : register(s2);

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

    float3 diffuse = color.rgb * max(0.05f, dot(N, L)) * attenuation * light_color_intensity.rgb * light_color_intensity.w;
    
    float3 V = normalize(camera_position.xyz - pin.world_position.xyz);
    float3 specular = pow(max(0, dot(N, normalize(V + L))), 128);
    
    //return color_map.Sample(anisotropic_sampler_state, pin.texcoord) * pin.color;
    
    return float4(diffuse + specular * attenuation, alpha) * pin.color;
}
