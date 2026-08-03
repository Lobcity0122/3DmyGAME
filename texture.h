#pragma once

#include <d3d11.h>
#include <memory>
#include "misc.h"

// テクスチャ読み込み関数
HRESULT load_texture_from_file(
	ID3D11Device* device, 
	const wchar_t* filename,
	ID3D11ShaderResourceView** shader_resource_view, 
	D3D11_TEXTURE2D_DESC* texture2d_desc
);

// テクスチャが存在しない場合の(ダミーテクスチャ関数の作成)
HRESULT make_dummy_texture(ID3D11Device* device,
	ID3D11ShaderResourceView** shader_resource_view,
	DWORD value/*0xAABBGGRR*/, UINT dimension
);