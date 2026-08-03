#include <filesystem>
#include <string>
#include <fstream>
#include <algorithm>
#include "static_mesh.h"
#include "misc.h"
#include "shader.h"
#include "texture.h"

using namespace DirectX;
using std::wstring;

static_mesh::static_mesh(ID3D11Device * device, const wchar_t* obj_filename)
{
    // 頂点データ配列とインデックスデータ配列
    std::vector<vertex> vertices;
    std::vector<uint32_t> indices;
    uint32_t current_index{ 0 };

    // objファイルから読み込んだ頂点座標と法線を格納するための変数
    std::vector<XMFLOAT3> positions;
    std::vector<XMFLOAT3> normals;

    // objファイルパーサー部でテクスチャ座標とマテリアルファイル名を取得する
    std::vector<XMFLOAT2> texcoords;
    std::vector<wstring> mtl_filenames;

    std::wifstream fin(obj_filename);
    _ASSERT_EXPR(fin, L"'OBJ file not found.");
    wchar_t command[256]; // 読み込んだファイルの1行

    while (fin)
    {
        fin >> command; // 1行目のコマンドを読み込む
        if (0 == wcscmp(command, L"v")) // 頂点位置の読み込み
        {
            // 頂点座標の読み込み
            float x, y, z;
            fin >> x >> y >> z;
            positions.push_back({ x, y, z });

			// 読み込んだ頂点座標(x、y、z)を使ってバウンディングボックスの最小座標と最大座標を更新する
			bounding_box_min.x = (std::min)(bounding_box_min.x, x);
			bounding_box_min.y = (std::min)(bounding_box_min.y, y);
			bounding_box_min.z = (std::min)(bounding_box_min.z, z);

            bounding_box_max.x = (std::max)(bounding_box_max.x, x);
            bounding_box_max.y = (std::max)(bounding_box_max.y, y);
            bounding_box_max.z = (std::max)(bounding_box_max.z, z);
            fin.ignore(1024, L'\n');
        }
        else if (0 == wcscmp(command, L"vn"))
        {
            // 法線の読み込み
            float i, j, k;
            fin >> i >> j >> k;
            normals.push_back({ i, j, k });
            fin.ignore(1024, L'\n');
        }
        else if (0 == wcscmp(command, L"vt"))
        {
            float u, v;
            fin >> u >> v;
            texcoords.push_back({ u, 1.0f - v }); //  texcoords.push_back({ u, v }); 
            fin.ignore(1024, L'\n');
        }
        else if (0 == wcscmp(command, L"f"))
        {
            // 三角形(面)の読み込み
            for (size_t i = 0; i < 3; i++)
            {
                vertex vertex;
                size_t v, vt, vn;

                fin >> v;
                vertex.position = positions.at(v - 1);
                if (L'/' == fin.peek())
                {
                    fin.ignore(1);
                    if (L'/' != fin.peek())
                    {
                        fin >> vt;
                        vertex.texcoord = texcoords.at(vt - 1);
                    }
                    if (L'/' == fin.peek())
                    {
                        fin.ignore(1);
                        fin >> vn;
                        vertex.normal = normals.at(vn - 1);
                    }
                }
                vertices.push_back(vertex);
                indices.push_back(current_index++);
            }
            fin.ignore(1024, L'\n');
        }
        else if (0 == wcscmp(command, L"mtllib"))
        {
            wchar_t mtllib[256];
            fin >> mtllib;
            mtl_filenames.push_back(mtllib);
        }
        else if(0== wcscmp(command, L"usemtl"))
        {
            wchar_t usemtl[MAX_PATH]{ 0 };
            fin >> usemtl;
            subsets.push_back({ usemtl,static_cast<uint32_t>(indices.size()),0 });
		}
        else
        {
            // それ以外の行は無視する
            fin.ignore(1024, L'\n');
        }
    }
    std::vector<subset>::reverse_iterator iterator = subsets.rbegin();
    iterator->index_count = static_cast<uint32_t>(indices.size()) - iterator->index_start;
    for (iterator = subsets.rbegin() + 1; iterator != subsets.rend(); ++iterator)
    {
      iterator->index_count = (iterator - 1)->index_start - iterator->index_start;
    }

    fin.close();

	// MTLファイルを開く前に確認する
    if(mtl_filenames.empty())
    {
		// エラーハンドリング(ログor既定マテリアル)
        return;
    }
   
    std::filesystem::path mtl_filename(obj_filename);
    mtl_filename.replace_filename(std::filesystem::path(mtl_filenames[0]).filename());

    fin.open(mtl_filename);
    //_ASSERT_EXPR(fin, L"'MTL file not found.");

    while (fin)
    {
        fin >> command;
        if (0 == wcscmp(command, L"newmtl"))
        {
            fin.ignore();
            wchar_t newmtl[256];
            material material;
            fin >> newmtl;
            material.name = newmtl;
            materials.push_back(material);
            fin.ignore(1024, L'\n');
        }
		else if (0 == wcscmp(command, L"map_Kd"))
        {
            fin.ignore();
            wchar_t map_Kd[256];
            fin >> map_Kd;

            std::filesystem::path path(obj_filename);
            path.replace_filename(std::filesystem::path(map_Kd).filename());
            //materials.rbegin()->texture_filename = path; 
            materials.rbegin()->texture_filenames[0] = path;
            fin.ignore(1024, L'\n');
        }
        else if (0 == wcscmp(command, L"map_bump") || 0 == wcscmp(command, L"bump"))
        {
            fin.ignore();
            wchar_t map_bump[256];
            fin >> map_bump;
            std::filesystem::path path(obj_filename);
            path.replace_filename(std::filesystem::path(map_bump).filename());
            materials.rbegin()->texture_filenames[1] = path;
            fin.ignore(1024, L'\n');
        }
        else if (0 == wcscmp(command, L"Kd"))
        {
            float r, g, b;
            fin >> r >> g >> b;
            materials.rbegin()->Kd = { r, g, b, 1 };
            fin.ignore(1024, L'\n');
        }
        else
        {
            fin.ignore(1024, L'\n');
        }
    }

    fin.close();  // ファイルを閉じる

    // テクスチャのロード、シェーダーリソースビューオブジェクトの生成をおこなう
    D3D11_TEXTURE2D_DESC texture2d_desc{};
    /*load_texture_from_file(device, texture_filename.c_str(),
        shader_resource_view.GetAddressOf(), &texture2d_desc);*/
    for (material& material : materials)
    {
        // カラーマップのロード 要素[0]
        if (!material.texture_filenames[0].empty())
        {
            load_texture_from_file(device, material.texture_filenames[0].c_str(),
                material.shader_resource_views[0].GetAddressOf(), &texture2d_desc);
        }

        // バンプマップのロード 要素[1]
        if (!material.texture_filenames[1].empty())
        {
            load_texture_from_file(device, material.texture_filenames[1].c_str(),
                material.shader_resource_views[1].GetAddressOf(), &texture2d_desc);
        }
    }

    if (materials.size() == 0)
    {
        for (const subset& subset : subsets)
        {
            materials.push_back({ subset.usemtl });
        }
    }

    for (material& material : materials)
    {
        if (material.shader_resource_views[0] == nullptr)
        {
            make_dummy_texture(device, material.shader_resource_views[0].GetAddressOf(), 0xFFFFFFFF, 16);
        }

        if (material.shader_resource_views[1] == nullptr)
        {
            make_dummy_texture(device, material.shader_resource_views[1].GetAddressOf(), 0xFFFF7F7F, 16);
        }
    }

    create_com_buffers(device, vertices.data(), vertices.size(), indices.data(), indices.size());

    D3D11_INPUT_ELEMENT_DESC input_element_desc[]
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
          D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
          D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
          D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    create_vs_from_cso(device, "static_mesh_vs.cso", vertex_shader.GetAddressOf(),
        input_layout.GetAddressOf(), input_element_desc, ARRAYSIZE(input_element_desc));
    create_ps_from_cso(device, "static_mesh_ps.cso", pixel_shader.GetAddressOf());

    HRESULT hr{ S_OK };

    // 定数バッファ作成
    D3D11_BUFFER_DESC buffer_desc{};
    buffer_desc.ByteWidth = sizeof(constants);
    buffer_desc.Usage = D3D11_USAGE_DEFAULT;
    buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    hr = device->CreateBuffer(&buffer_desc, nullptr, constant_buffer.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

void static_mesh::create_com_buffers(ID3D11Device * device,
    vertex * vertices, size_t vertex_count,
    uint32_t * indices, size_t index_count
)
{
    HRESULT hr{ S_OK };

    D3D11_BUFFER_DESC buffer_desc{};
    D3D11_SUBRESOURCE_DATA subresource_data{};
    buffer_desc.ByteWidth = static_cast<UINT>(sizeof(vertex) * vertex_count);
    buffer_desc.Usage = D3D11_USAGE_DEFAULT;
    buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    buffer_desc.CPUAccessFlags = 0;
    buffer_desc.MiscFlags = 0;
    buffer_desc.StructureByteStride = 0;
    subresource_data.pSysMem = vertices;
    subresource_data.SysMemPitch = 0;
    subresource_data.SysMemSlicePitch = 0;
    hr = device->CreateBuffer(&buffer_desc, &subresource_data, vertex_buffer.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    buffer_desc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * index_count);
    buffer_desc.Usage = D3D11_USAGE_DEFAULT;
    buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    subresource_data.pSysMem = indices;
    hr = device->CreateBuffer(&buffer_desc, &subresource_data, index_buffer.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

void static_mesh::render(ID3D11DeviceContext * immediate_context,
    const DirectX::XMFLOAT4X4 & world,
    const DirectX::XMFLOAT4 & material_color,
    ID3D11PixelShader* alternative_pixel_shader
)
{
    uint32_t stride{ sizeof(vertex) };
    uint32_t offset{ 0 };
    immediate_context->IASetVertexBuffers(0, 1, vertex_buffer.GetAddressOf(), &stride, &offset);
    immediate_context->IASetIndexBuffer(index_buffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    immediate_context->IASetInputLayout(input_layout.Get());

    immediate_context->VSSetShader(vertex_shader.Get(), nullptr, 0);
    //immediate_context->PSSetShader(pixel_shader.Get(), nullptr, 0);

    if (alternative_pixel_shader != nullptr)
    {
        immediate_context->PSSetShader(alternative_pixel_shader, nullptr, 0);
    }
    else
    {
        immediate_context->PSSetShader(pixel_shader.Get(), nullptr, 0);
    }

    for (const material& material : materials)
    {
        // カラーマップをピクセルシェーダーの【スロット0】にセット
        immediate_context->PSSetShaderResources(0, 1, material.shader_resource_views[0].GetAddressOf());

        // バンプマップをピクセルシェーダーの【スロット1】にセット
        immediate_context->PSSetShaderResources(1, 1, material.shader_resource_views[1].GetAddressOf());

        constants data{ world, material_color };
        XMStoreFloat4(&data.material_color, XMLoadFloat4(&material_color) * XMLoadFloat4(&material.Kd));
        immediate_context->UpdateSubresource(constant_buffer.Get(), 0, 0, &data, 0, 0);
        
        // 定数バッファを頂点シェーダーにバインドする
        immediate_context->VSSetConstantBuffers(0, 1, constant_buffer.GetAddressOf());

        // ピクセルシェーダーにも同じ定数バッファをバインドする
        immediate_context->PSSetConstantBuffers(0, 1, constant_buffer.GetAddressOf());

        for (const subset& subset : subsets)
        {
            if (material.name == subset.usemtl)
            {
                immediate_context->DrawIndexed(subset.index_count, subset.index_start, 0);
            }
        }
    }

    /*D3D11_BUFFER_DESC buffer_desc{};
    index_buffer->GetDesc(&buffer_desc);
    immediate_context->DrawIndexed(buffer_desc.ByteWidth / sizeof(uint32_t), 0, 0);*/
}