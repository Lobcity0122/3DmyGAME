#pragma once

#include <d3d11.h>
#include <directxmath.h>
#include <wrl.h>
using namespace Microsoft::WRL;

class geometric_primitive
{
public:
	struct vertex
	{
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 normal;
	};
	struct constants
	{
		DirectX::XMFLOAT4X4 world;
		DirectX::XMFLOAT4 material_color;
	};

private:
	ComPtr<ID3D11Buffer> vertex_buffer;
	ComPtr<ID3D11Buffer> index_buffer;

	ComPtr<ID3D11VertexShader> vertex_shader;
	ComPtr<ID3D11PixelShader> pixel_shader;
	ComPtr<ID3D11InputLayout> input_layout;
	ComPtr<ID3D11Buffer> constant_buffer;

public:
	geometric_primitive(ID3D11Device* device);
	virtual ~geometric_primitive() = default;

	void render(ID3D11DeviceContext* immediate_context,
		const DirectX::XMFLOAT4X4& world, 
		const DirectX::XMFLOAT4& material_color
	);

protected:
	void create_com_buffers(ID3D11Device * device, 
		vertex * vertices, size_t vertex_count,
        uint32_t * indices, size_t index_count
	);
};

// 立方体のジオメトリを生成するクラス
class cube : public geometric_primitive
{
public:
	cube(ID3D11Device* device);
};

// 円柱のジオメトリを生成するクラス
class cylinder : public geometric_primitive
{
public:
	cylinder(ID3D11Device* device,int slice);
};

// 球のジオメトリを生成するクラス
class ball : public geometric_primitive
{
public:
	ball(ID3D11Device* device, int slice, int stack);
};

// カプセルのジオメトリを生成するクラス
class capsule : public geometric_primitive
{
public:
	capsule(ID3D11Device* device, int slice, int stack, float height);
};
