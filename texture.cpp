#include "texture.h"

#include <WICTextureLoader.h> // DirectXTKに含まれる、一般的な画像（PNGやJPEG）をDirectX11用に読み込むための便利な機能のライブラリ
using namespace DirectX;

#include <wrl.h> // ComPtrを使うために必要なヘッダーファイル。Microsoft::WRLの名前空間の中に ComPtr が定義されている。
using namespace Microsoft::WRL;

// 画像のファイル名（文字列：wstring）と、読み込んだテクスチャデータ（ComPtr<...>）をセットで記憶するための連想配列（std::map）を使用するための準備。
#include <map>
#include <string>

using namespace std;

// テクスチャを保存するマップ
static map<wstring, ComPtr<ID3D11ShaderResourceView>>resources;

HRESULT load_texture_from_file(
    ID3D11Device* device,
    const wchar_t* filename,
    ID3D11ShaderResourceView** shader_resource_view,
    D3D11_TEXTURE2D_DESC* texture2d_desc
)
{
    HRESULT hr = S_OK;

    ComPtr<ID3D11Resource> resource;

    // すでに読み込まれてるかチェック
    auto it = resources.find(filename);

    if (it != resources.end())
    {
        // キャッシュから取得
        *shader_resource_view = it->second.Get();
        (*shader_resource_view)->AddRef();

        // 対応するリソース取得
        (*shader_resource_view)->GetResource(resource.GetAddressOf());
    }
    else
    {
        // 新しく読み込む
        hr = CreateWICTextureFromFile(
            device,
            filename,
            resource.GetAddressOf(),
            shader_resource_view
        );

        // 読み込んだテクスチャを保存
        resources.insert(make_pair(filename, *shader_resource_view));
    }

    // テクスチャ情報取得（サイズなど）
    ComPtr<ID3D11Texture2D> texture2d;
    hr = resource->QueryInterface<ID3D11Texture2D>(texture2d.GetAddressOf());

    texture2d->GetDesc(texture2d_desc);

    return hr;
}

// ダミーテスクチャ関数の実装
HRESULT make_dummy_texture(ID3D11Device* device,
    ID3D11ShaderResourceView** shader_resource_view,
    DWORD value/*0xAABBGGRR*/, UINT dimension
)
{
    HRESULT hr{ S_OK };

    D3D11_TEXTURE2D_DESC texture2d_desc{};
    texture2d_desc.Width = dimension;
    texture2d_desc.Height = dimension;
    texture2d_desc.MipLevels = 1;
    texture2d_desc.ArraySize = 1;
    texture2d_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texture2d_desc.SampleDesc.Count = 1;
    texture2d_desc.SampleDesc.Quality = 0;
    texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
    texture2d_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    size_t texels = dimension * dimension;
    unique_ptr<DWORD[]>system{ make_unique<DWORD[]>(texels) };
    for (size_t i = 0; i < texels; i++)
    {
        system[i] = value;
    }

    D3D11_SUBRESOURCE_DATA subresource_data{};
    subresource_data.pSysMem = system.get();
    subresource_data.SysMemPitch = sizeof(DWORD) * dimension;

    ComPtr<ID3D11Texture2D> texture2d;
    hr = device->CreateTexture2D(&texture2d_desc, &subresource_data, &texture2d);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc{};
    shader_resource_view_desc.Format = texture2d_desc.Format;
    shader_resource_view_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    shader_resource_view_desc.Texture2D.MipLevels = 1;
    hr = device->CreateShaderResourceView(texture2d.Get(), &shader_resource_view_desc,
        shader_resource_view);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    return hr;
}

// 全解放
void release_all_textures()
{
    resources.clear();
}