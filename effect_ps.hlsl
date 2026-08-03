#include "sprite.hlsli"

// UNIT.10
Texture2D color_texture : register(t0);
SamplerState point_sampler_state : register(s0);
SamplerState linear_sampler_state : register(s1);
SamplerState anisotropic_sampler_state : register(s2);

float4 main(VS_OUT pin) : SV_TARGET
{

	uint colour_map_mip_level = 0, colour_map_width, colour_map_height, colour_map_number_of_levels;
	color_texture.GetDimensions(colour_map_mip_level, colour_map_width, colour_map_height, colour_map_number_of_levels);

	float2 step = float2(1.0 / colour_map_width, 1.0 / colour_map_height) * 2.5;
	float4 color = color_texture.Sample(point_sampler_state, float2(pin.texcoord.x, pin.texcoord.y)) * 0.25;
	color += color_texture.Sample(point_sampler_state, float2(pin.texcoord.x + step.x, pin.texcoord.y + 0.0)) * 0.125;
	color += color_texture.Sample(point_sampler_state, float2(pin.texcoord.x - step.x, pin.texcoord.y + 0.0)) * 0.125;
	color += color_texture.Sample(point_sampler_state, float2(pin.texcoord.x + 0.0, pin.texcoord.y + step.y)) * 0.125;
	color += color_texture.Sample(point_sampler_state, float2(pin.texcoord.x - 0.0, pin.texcoord.y - step.y)) * 0.125;
	color += color_texture.Sample(point_sampler_state, float2(pin.texcoord.x + step.x, pin.texcoord.y + step.y)) * 0.0625;
	color += color_texture.Sample(point_sampler_state, float2(pin.texcoord.x - step.x, pin.texcoord.y + step.y)) * 0.0625;
	color += color_texture.Sample(point_sampler_state, float2(pin.texcoord.x - step.x, pin.texcoord.y - step.y)) * 0.0625;
	color += color_texture.Sample(point_sampler_state, float2(pin.texcoord.x + step.x, pin.texcoord.y - step.y)) * 0.0625;

	float4 color_filter = float4(2.5, 1.0, 0.2, 1);
	return color * color_filter;
}
