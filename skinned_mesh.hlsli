struct VS_IN
{
    float4 position : POSITION;
    float4 normal : NORMAL;
    float2 texcoord : TEXCOORD;
};

struct VS_OUT
{
    float4 position : SV_POSITION;
    float4 world_position : POSITION;
    float4 world_normal : NORMAL;
    float2 texcoord : TEXCOORD;
    float4 color : COLOR;
};

cbuffer OBJECT_CONSTANT_BUFFER : register(b0)
{
    row_major float4x4 world;
    float4 material_color;
    float4 material_params; // x: metallic, y: roughness
};

cbuffer SCENE_CONSTANT_BUFFER : register(b1)
{
    row_major float4x4 view_projection;
    float4 light_direction;
    float4 camera_position;
    float4 light_position_range; // xyz: point light position, w: range
    float4 light_color_intensity; // rgb: color, w: intensity
    float4 ambient_color_intensity; // rgb: sky/environment color, w: intensity
    float4 render_options; // x: 1=point light, y: 1=unlit texture check
    row_major float4x4 light_view_projection;
    float4 shadow_settings; // x: depth bias, y: 1=enable directional shadow
    float4 post_process_settings; // x: exposure in EV
};
