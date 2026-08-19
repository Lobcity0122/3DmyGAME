Texture2D scene_texture : register(t0);
SamplerState linear_sampler : register(s0);
cbuffer BLOOM_CONSTANT_BUFFER : register(b0) { float2 texel_size; float threshold; float intensity; };
struct VS_OUT { float4 position : SV_POSITION; float4 color : COLOR; float2 texcoord : TEXCOORD; };
float3 bright(float3 color) { return color * saturate((dot(color, float3(.2126,.7152,.0722)) - threshold) / max(1 - threshold, .001)); }
float4 main(VS_OUT pin) : SV_TARGET
{
    float3 original = scene_texture.Sample(linear_sampler, pin.texcoord).rgb, glow = 0;
    const float2 offsets[8] = { float2(-1,-1),float2(0,-1),float2(1,-1),float2(-1,0),float2(1,0),float2(-1,1),float2(0,1),float2(1,1) };
    [unroll] for (int i = 0; i < 8; ++i) glow += bright(scene_texture.Sample(linear_sampler, pin.texcoord + offsets[i] * texel_size * 2.5).rgb);
    return float4(original + glow * (intensity / 8.0), 1);
}
