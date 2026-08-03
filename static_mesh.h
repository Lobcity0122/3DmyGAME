#pragma once

#include <d3d11.h>
#include <directxmath.h>
#include <wrl.h>
#include <vector>

using namespace Microsoft::WRL;

class static_mesh
{
public:
	struct vertex
	{
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 normal;
		DirectX::XMFLOAT2 texcoord;
	};
	struct constants
	{
		DirectX::XMFLOAT4X4 world;
		DirectX::XMFLOAT4 material_color;
	};
	struct subset
	{
		std::wstring usemtl;
		uint32_t index_start{ 0 }; // start position of index buffer インデックスバッファの開始位置
		uint32_t index_count{ 0 }; // number of vertices (indices)  インデックスの数
	};
	std::vector<subset> subsets;

private:
	ComPtr<ID3D11Buffer> vertex_buffer;
	ComPtr<ID3D11Buffer> index_buffer;

	ComPtr<ID3D11VertexShader> vertex_shader;
	ComPtr<ID3D11PixelShader> pixel_shader;
	ComPtr<ID3D11InputLayout> input_layout;
	ComPtr<ID3D11Buffer> constant_buffer;

	/*std::wstring texture_filename;
	ComPtr<ID3D11ShaderResourceView> shader_resource_view;*/

	struct material
	{
		std::wstring name;
		DirectX::XMFLOAT4 Ka{ 0.2f, 0.2f, 0.2f, 1.0f };
		DirectX::XMFLOAT4 Kd{ 0.8f, 0.8f, 0.8f, 1.0f };
		DirectX::XMFLOAT4 Ks{ 1.0f, 1.0f, 1.0f, 1.0f };
		std::wstring texture_filenames[2];
		ComPtr<ID3D11ShaderResourceView> shader_resource_views[2];
	};
	std::vector<material> materials;

	// バウンディングボックス用のメンバ変数の追加
	DirectX::XMFLOAT3 bounding_box_min{ FLT_MAX, FLT_MAX, FLT_MAX };    // バウンディングボックスの最小座標
	DirectX::XMFLOAT3 bounding_box_max{ -FLT_MAX, -FLT_MAX, -FLT_MAX }; // バウンディングボックスの最大座標

public:
	static_mesh(ID3D11Device* device, const wchar_t* obj_filename);
	virtual ~static_mesh() = default;

	void render(ID3D11DeviceContext* immediate_context,
		const DirectX::XMFLOAT4X4& world,
		const DirectX::XMFLOAT4& material_color,
		ID3D11PixelShader* alternative_pixel_shader = nullptr
	);

	// バウンディングボックスの最小、最大座標を取得するゲッター
	void get_bounding_box(DirectX::XMFLOAT3& min_vertex, DirectX::XMFLOAT3& max_vertex) const
	{
		min_vertex = bounding_box_min;
		max_vertex = bounding_box_max;
	}

protected:
	void create_com_buffers(ID3D11Device* device,
		vertex* vertices, size_t vertex_count,
		uint32_t* indices, size_t index_count
	);
};
