#include "sprite_batch.h"
#include "texture.h"
#include "shader.h"
#include "misc.h"
#include <WICTextureLoader.h> // DirectXTKライブラリから(画像ファイル読み込みのため)
#include <sstream>

sprite_batch::sprite_batch(ID3D11Device* device, const wchar_t* filename, size_t max_sprites)
	:max_vertices(max_sprites * 6)
{
	HRESULT hr{ S_OK };

	// 頂点情報のセット
	
	//vertex vertices[]
	//{
	//  { { -1.0, +1.0, 0 }, { 1, 1, 1, 1 }, { 0, 0 } },
	//  { { +1.0, +1.0, 0 }, { 1, 1, 1, 1 }, { 1, 0 } },
	//  { { -1.0, -1.0, 0 }, { 1, 1, 1, 1 }, { 0, 1 } },
	//  { { +1.0, -1.0, 0 }, { 1, 1, 1, 1 }, { 1, 1 } },
	//};

	// 頂点バッファオブジェクトの生成
	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = sizeof(vertex) * max_vertices;
	buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
	buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	buffer_desc.MiscFlags = 0;
	buffer_desc.StructureByteStride = 0;
	hr = device->CreateBuffer(&buffer_desc, NULL, vertex_buffer.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	// 頂点シェーダーオブジェクトの生成
	//{
	//	const char* cso_name{ "sprite_vs.cso" };

	//	FILE* fp{};
	//	fopen_s(&fp, cso_name, "rb");
	//	_ASSERT_EXPR_A(fp, "CSO File not found");

	//	fseek(fp, 0, SEEK_END);
	//	long cso_sz{ ftell(fp) };
	//	fseek(fp, 0, SEEK_SET);

	//	std::unique_ptr<unsigned char[]> cso_data{ std::make_unique<unsigned char[]>(cso_sz) };
	//	fread(cso_data.get(), cso_sz, 1, fp);
	//	fclose(fp);

	//	hr = device->CreateVertexShader(cso_data.get(), cso_sz, nullptr, vertex_shader.GetAddressOf());
	//	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	//	// 入力レイアウトオブジェクトの生成
	//	// ※要素を追加
	//	D3D11_INPUT_ELEMENT_DESC input_element_desc[]
	//	{
	//	   { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
	//		  D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	//	   { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
	//		  D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	//	   { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
	//		  D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	//	};
	//	hr = device->CreateInputLayout(input_element_desc, _countof(input_element_desc),
	//		cso_data.get(), cso_sz, input_layout.GetAddressOf());
	//	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	//}

	// 頂点シェーダーオブジェクトと入力レイアウトの生成（モジュール関数を使用）
	D3D11_INPUT_ELEMENT_DESC input_element_desc[]
	{
	   { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	   { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	   { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	create_vs_from_cso(device, "sprite_vs.cso", vertex_shader.GetAddressOf(), input_layout.GetAddressOf(), input_element_desc, _countof(input_element_desc));

	// 画像ファイルのロードとシェーダーリソースオブジェクトの生成
	/*ComPtr<ID3D11Resource> resource{};
	hr = DirectX::CreateWICTextureFromFile(device, filename, resource.GetAddressOf(), shader_resource_view.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));*/

	// テクスチャのロードと情報の取得（モジュール関数）
    load_texture_from_file(device, filename, shader_resource_view.GetAddressOf(), &texture2d_desc);

	// ピクセルシェーダーオブジェクトの生成
	/*{
		const char* cso_name{ "sprite_ps.cso" };

		FILE* fp{};
		fopen_s(&fp, cso_name, "rb");
		_ASSERT_EXPR_A(fp, "CSO File not found");

		fseek(fp, 0, SEEK_END);
		long cso_sz{ ftell(fp) };
		fseek(fp, 0, SEEK_SET);

		std::unique_ptr<unsigned char[]> cso_data{ std::make_unique<unsigned char[]>(cso_sz) };
		fread(cso_data.get(), cso_sz, 1, fp);
		fclose(fp);

		hr = device->CreatePixelShader(cso_data.get(), cso_sz, nullptr, pixel_shader.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}*/

	// ピクセルシェーダーオブジェクトの生成（モジュール関数を使用）
	create_ps_from_cso(device, "sprite_ps.cso", pixel_shader.GetAddressOf());

	// テクスチャ情報の取得
	/*ComPtr<ID3D11Texture2D> texture2d{};
	// hr = resource->QueryInterface<ID3D11Texture2D>(&texture2d);
	hr = resource.As(&texture2d);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	texture2d->GetDesc(&texture2d_desc);*/
}

// デストラクタ
sprite_batch::~sprite_batch()
{
	/*vertex_shader->Release();
	pixel_shader->Release();
	input_layout->Release();
	vertex_buffer->Release();
	shader_resource_view->Release();*/
}

// renderメンバ関数の実装
// 頂点バッファーのバインド
void sprite_batch::render(ID3D11DeviceContext* immediate_context,
	float dx, float dy, float dw, float dh,
	float r, float g, float b, float a,
	float angle)
{
	// コードの重複が最小になるようにリファクタリングする
	render(immediate_context,
		dx, dy, dw, dh,
		r, g, b, a, angle,
		0, 0, static_cast<float>(texture2d_desc.Width),
		static_cast<float>(texture2d_desc.Height)
	);
}

// framework::render()で追加した sprite オブジェクトを描画する
void sprite_batch::render(ID3D11DeviceContext* immediate_context,
	float dx, float dy, float dw, float dh,
	float r, float g, float b, float a,
	float angle,
	float sx, float sy, float sw, float sh)
{
	float cos{ cosf(DirectX::XMConvertToRadians(angle)) };
	float sin{ sinf(DirectX::XMConvertToRadians(angle)) };

	// スクリーン(ビューポート)のサイズを取得する
	D3D11_VIEWPORT viewport{}; // 使うための宣言
	UINT num_viewports{ 1 }; // 1つ取り出す
	immediate_context->RSGetViewports(&num_viewports, &viewport);

	// left-top
	float x0{ dx };
	float y0{ dy };

	// right-top
	float x1{ dx + dw };
	float y1{ dy };

	// left-bottom
	float x2{ dx };
	float y2{ dy + dh };

	// right-bottom
	float x3{ dx + dw };
	float y3{ dy + dh };

	// 回転の中心を矩形の中心点にした場合
	float cx = dx + dw * 0.5f;
	float cy = dy + dh * 0.5f;
	rotate(x0, y0, cx, cy, cos, sin);
	rotate(x1, y1, cx, cy, cos, sin);
	rotate(x2, y2, cx, cy, cos, sin);
	rotate(x3, y3, cx, cy, cos, sin);

	// Convert to NDC space
	// スクリーン座標からNDCへの座標変換を行う
	x0 = 2.0f * x0 / viewport.Width - 1.0f;
	y0 = 1.0f - 2.0f * y0 / viewport.Height;

	x1 = 2.0f * x1 / viewport.Width - 1.0f;
	y1 = 1.0f - 2.0f * y1 / viewport.Height;

	x2 = 2.0f * x2 / viewport.Width - 1.0f;
	y2 = 1.0f - 2.0f * y2 / viewport.Height;

	x3 = 2.0f * x3 / viewport.Width - 1.0f;
	y3 = 1.0f - 2.0f * y3 / viewport.Height;

	// テクセル座標(ピクセル単位)からUV座標への変換
	float u0{ sx / static_cast<float>(texture2d_desc.Width) };
	float v0{ sy / static_cast<float>(texture2d_desc.Height) };
	float u1{ (sx + sw) / static_cast<float>(texture2d_desc.Width) };
	float v1{ (sy + sh) / static_cast<float>(texture2d_desc.Height) };

	vertices.push_back({ {x0,y0,0},{r,g,b,a},{u0,v0} });
	vertices.push_back({ {x1,y1,0},{r,g,b,a},{u1,v0} });
	vertices.push_back({ {x2,y2,0},{r,g,b,a},{u0,v1} });
	vertices.push_back({ {x2,y2,0},{r,g,b,a},{u0,v1} });
	vertices.push_back({ {x1,y1,0},{r,g,b,a},{u1,v0} });
	vertices.push_back({ {x3,y3,0},{r,g,b,a},{u1,v1} }); 
}

void sprite_batch::begin(ID3D11DeviceContext* immediate_context, ID3D11PixelShader* replaced_pixel_shader)
{
	vertices.clear();
	immediate_context->VSSetShader(vertex_shader.Get(), nullptr, 0);
	immediate_context->PSSetShader(
		replaced_pixel_shader ? replaced_pixel_shader : pixel_shader.Get(), nullptr, 0
	);
	immediate_context->PSSetShaderResources(0, 1, shader_resource_view.GetAddressOf());
}

void sprite_batch::end(ID3D11DeviceContext* immediate_context)
{
	// 計算結果で頂点バッファオブジェクトを更新
	HRESULT hr{ S_OK };
	D3D11_MAPPED_SUBRESOURCE mapped_subresource{};
	hr = immediate_context->Map(vertex_buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_subresource);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	size_t vertex_count = vertices.size();
	_ASSERT_EXPR(max_vertices >= vertex_count, "Buffer overflow");
	vertex* data{ reinterpret_cast<vertex*>(mapped_subresource.pData) };
	if (data != nullptr)
	{
		const vertex* p = vertices.data();
		memcpy_s(data, max_vertices * sizeof(vertex), p, vertex_count * sizeof(vertex));
	}
	immediate_context->Unmap(vertex_buffer.Get(), 0);

	UINT stride{ sizeof(vertex) };
	UINT offset{ 0 };
	immediate_context->IASetVertexBuffers(0, 1, vertex_buffer.GetAddressOf(), &stride, &offset);
	immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	immediate_context->IASetInputLayout(input_layout.Get());

	immediate_context->Draw(static_cast<UINT>(vertex_count), 0);
}

void sprite_batch::render(ID3D11DeviceContext* immediate_context, float dx, float dy, float dw, float dh)
{
	render(immediate_context, 
		dx, dy, dw, dh, 
		1.0f, 1.0f, 1.0f, 1.0f,
		0.0f,
		0.0f, 0.0f,
		static_cast<float>(texture2d_desc.Width), 
		static_cast<float>(texture2d_desc.Height)
	);
}