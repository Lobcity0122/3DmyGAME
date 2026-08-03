#pragma once
#include <d3d11.h>

#include <wrl.h> // ComPtrを使うために必要なヘッダーファイル。Microsoft::WRLの名前空間の中に ComPtr が定義されている。
using namespace Microsoft::WRL;

// 頂点シェーダーオブジェクト読み込み関数
//HRESULT create_vs_from_cso(ID3D11Device* device,
//	const char* cso_name,
//	ID3D11VertexShader** vertex_shader,
//	ID3D11InputLayout** input_layout,
//	D3D11_INPUT_ELEMENT_DESC* input_element_desc,
//	UINT num_elements
//);

HRESULT create_vs_from_cso(
	ID3D11Device* device,
	const char* cso_name,
	ID3D11VertexShader** vertex_shader,        
	ID3D11InputLayout** input_layout,          
	D3D11_INPUT_ELEMENT_DESC* input_element_desc,
	UINT num_elements
);

// ピクセルシェーダーオブジェクト読み込み
//HRESULT create_ps_from_cso(ID3D11Device* device,
//	const char* cso_name,
//	ID3D11PixelShader** pixel_shader
//);

HRESULT create_ps_from_cso(
    ID3D11Device* device,               
	const char* cso_name,
	ID3D11PixelShader** pixel_shader           
);