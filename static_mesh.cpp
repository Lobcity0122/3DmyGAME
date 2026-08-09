#include <filesystem>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
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
	XMFLOAT3 current_object_min{ FLT_MAX, FLT_MAX, FLT_MAX };
	XMFLOAT3 current_object_max{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
	bool current_object_has_vertices = false;
	const auto finish_current_object = [this, &current_object_min, &current_object_max, &current_object_has_vertices]()
	{
		if (!current_object_has_vertices) return;
		object_bounding_boxes.push_back({ current_object_min, current_object_max });
		current_object_min = { FLT_MAX, FLT_MAX, FLT_MAX };
		current_object_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
		current_object_has_vertices = false;
	};

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

			current_object_min.x = (std::min)(current_object_min.x, x);
			current_object_min.y = (std::min)(current_object_min.y, y);
			current_object_min.z = (std::min)(current_object_min.z, z);
			current_object_max.x = (std::max)(current_object_max.x, x);
			current_object_max.y = (std::max)(current_object_max.y, y);
			current_object_max.z = (std::max)(current_object_max.z, z);
			current_object_has_vertices = true;
            fin.ignore(1024, L'\n');
        }
        else if (0 == wcscmp(command, L"o") || 0 == wcscmp(command, L"g"))
        {
			// 次のオブジェクトへ切り替わる前に、直前の頂点範囲を保存する。
			finish_current_object();
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
            // OBJの面は三角形に限らない。四角形・n角形も扇形分割してGPU用の三角形へ変換する。
            std::wstring face_line;
            std::getline(fin, face_line);
            std::wistringstream face_stream(face_line);
            std::vector<vertex> face_vertices;
            std::wstring token;
            while (face_stream >> token)
            {
                vertex parsed_vertex{};
                const size_t first_slash = token.find(L'/');
                const size_t second_slash = first_slash == std::wstring::npos ? std::wstring::npos : token.find(L'/', first_slash + 1);
                const int position_index = std::stoi(token.substr(0, first_slash));
                parsed_vertex.position = positions.at(position_index - 1);

                if (first_slash != std::wstring::npos && second_slash != first_slash + 1)
                {
                    const int texcoord_index = std::stoi(token.substr(first_slash + 1, second_slash - first_slash - 1));
                    parsed_vertex.texcoord = texcoords.at(texcoord_index - 1);
                }
                if (second_slash != std::wstring::npos && second_slash + 1 < token.size())
                {
                    const int normal_index = std::stoi(token.substr(second_slash + 1));
                    parsed_vertex.normal = normals.at(normal_index - 1);
                }
                face_vertices.push_back(parsed_vertex);
            }
            for (size_t index = 1; index + 1 < face_vertices.size(); ++index)
            {
                vertices.push_back(face_vertices[0]);
                vertices.push_back(face_vertices[index]);
                vertices.push_back(face_vertices[index + 1]);
                indices.push_back(current_index++);
                indices.push_back(current_index++);
                indices.push_back(current_index++);
            }
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
    finish_current_object();
    if (subsets.empty() && !indices.empty())
    {
        // MTL/usemtl を持たないOBJでも、描画・衝突用データを最後まで生成する。
        subsets.push_back({ L"default", 0, static_cast<uint32_t>(indices.size()) });
    }
    else if (!subsets.empty())
    {
        std::vector<subset>::reverse_iterator iterator = subsets.rbegin();
        iterator->index_count = static_cast<uint32_t>(indices.size()) - iterator->index_start;
        for (iterator = subsets.rbegin() + 1; iterator != subsets.rend(); ++iterator)
        {
            iterator->index_count = (iterator - 1)->index_start - iterator->index_start;
        }
    }

    fin.close();

	// MTLは任意。衝突専用OBJのようにMTLが無い場合は、既定マテリアルで続行する。
    if (!mtl_filenames.empty())
    {
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
		else if (0 == wcscmp(command, L"Ns"))
		{
			// OBJ/MTLのPhong光沢値。後でPBRのroughnessへ変換する。
			fin >> materials.rbegin()->Ns;
			fin.ignore(1024, L'\n');
		}
            else
            {
                fin.ignore(1024, L'\n');
            }
        }
        fin.close();  // ファイルを閉じる
    }

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
	cpu_vertices = vertices;
	cpu_indices = indices;

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
    ID3D11PixelShader* alternative_pixel_shader,
    bool depth_only
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

    if (depth_only)
    {
        // シャドウマップ描画では色は不要。深度バッファだけを更新する。
        immediate_context->PSSetShader(nullptr, nullptr, 0);
    }
    else if (alternative_pixel_shader != nullptr)
    {
        immediate_context->PSSetShader(alternative_pixel_shader, nullptr, 0);
    }
    else
    {
        immediate_context->PSSetShader(pixel_shader.Get(), nullptr, 0);
    }

	for (const material& material : materials)
	{
		if (!depth_only)
		{
			// カラーマップをピクセルシェーダーの【スロット0】にセット
			immediate_context->PSSetShaderResources(0, 1, material.shader_resource_views[0].GetAddressOf());

			// バンプマップをピクセルシェーダーの【スロット1】にセット
			immediate_context->PSSetShaderResources(1, 1, material.shader_resource_views[1].GetAddressOf());
		}

		constants data{};
		data.world = world;
		XMStoreFloat4(&data.material_color, XMLoadFloat4(&material_color) * XMLoadFloat4(&material.Kd));
		// PhongのNsが大きいほど表面は滑らか。PBRのroughnessは逆の関係になる。
		const float roughness = (std::max)(0.08f, (std::min)(1.0f, std::sqrt(2.0f / (material.Ns + 2.0f))));
		data.material_params = { 0.0f, roughness, 0.0f, 0.0f };
        immediate_context->UpdateSubresource(constant_buffer.Get(), 0, 0, &data, 0, 0);
        
        // 定数バッファを頂点シェーダーにバインドする
        immediate_context->VSSetConstantBuffers(0, 1, constant_buffer.GetAddressOf());

		if (!depth_only)
		{
			// ピクセルシェーダーにも同じ定数バッファをバインドする
			immediate_context->PSSetConstantBuffers(0, 1, constant_buffer.GetAddressOf());
		}

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
