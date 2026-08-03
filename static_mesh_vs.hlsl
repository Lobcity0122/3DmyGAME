#include "static_mesh.hlsli" 

VS_OUT main(float4 position : POSITION, float4 normal : NORMAL,float2 texcoord : TEXCOORD)
{
    VS_OUT vout;
    vout.position = mul(position, mul(world, view_projection));
    
    vout.world_position = mul(position, world);
    normal.w = 0;
    vout.world_normal = normalize(mul(normal, world));
    
    vout.color = material_color;
    vout.texcoord = texcoord; // テクスチャ座標は使用しないので0に設定
    
    //normal.w = 0;
    //float4 N = normalize(mul(normal, world));
    //float4 L = normalize(-light_direction);
    //vout.color.rgb = material_color.rgb * max(0, dot(L, N));
    //vout.color.a = material_color.a;
    
    return vout;
}