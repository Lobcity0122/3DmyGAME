#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <directxmath.h>
#include <vector>
#include <string>
#include <unordered_map> // std::unordered_map 用に追加
#include <filesystem>    // std::filesystem 用に追加
#include <algorithm>     // std::min,std::max用に追加
#include <fbxsdk.h>

using namespace Microsoft::WRL;

// シーン全体、ノード階層を管理する構造体
struct scene
{
	// FBXシーンのノード(ボーン、メッシュ、ライト等)を表す構造体
	struct node
	{
		uint64_t unique_id{ 0 };                        // ノード固有の識別ID
		std::string name;                               // ノード名
		FbxNodeAttribute::EType attribute{ FbxNodeAttribute::EType::eUnknown }; // 属性タイプ（メッシュ、ボーン等）
		int64_t parent_index{ -1 };                     // 親ノードの配列インデックス（ルートの場合は -1）
	};

	std::vector<node> nodes;

	// 指定したUniqueIDからノード配列のインデックスを探索するヘルパー関数
	int64_t indexof(uint64_t unique_id) const
	{
		int64_t index{ 0 };
		for (const node& node : nodes)
		{
			if (node.unique_id == unique_id)
			{
				return index;
			}

			++index;
		}
		return -1; // 見つからない場合は－1
	}
};

// スキンメッシュ(3Dキャラクター等)を管理、描画するクラス
class skinned_mesh
{
public:
	// 頂点データ構造体(位置、法線、テクスチャ座標)
	struct vertex
	{
		DirectX::XMFLOAT3 position;                     // 頂点座標
		DirectX::XMFLOAT3 normal{ 0, 1, 0 };            // 法線ベクトル
		DirectX::XMFLOAT2 texcoord{ 0, 0 };             // UV座標
	};

	// 定数バッファ用構造体(ワールド行列やマテリアルカラー等)
	struct constants
	{
		DirectX::XMFLOAT4X4 world;
		DirectX::XMFLOAT4 material_color;
	};

	// skinned_meshクラスにメッシュ構造体
	struct mesh 
	{
		uint64_t unique_id{ 0 };
		std::string name;
		// 'node_index' is an index that refers to the node array of the scene.
		int64_t node_index{ 0 };

		Microsoft::WRL::ComPtr<ID3D11Buffer> vertex_buffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> index_buffer;

		std::vector<vertex> vertices;
		std::vector<uint32_t> indices;

		// メッシュのサブセット構造体の定義
		struct subset
		{
			uint64_t material_unique_id{ 0 }; // マテリアルID
			std::string material_name;        // マテリアル名

			uint32_t start_index_location{ 0 }; // インデックスバッファ内の開始位置
			uint32_t index_count{ 0 };        // インデックス数
		};

		std::vector<subset> subsets; // サブセット構造体のリスト

		DirectX::XMFLOAT4X4 default_global_transform
		{ 
			1, 0, 0, 0, 0,
			1, 0, 0, 0, 0, 
			1, 0, 0, 0, 0,
			1 
		}; // デフォルトのグローバルトランスフォーム
	};

	std::vector<mesh> meshes; // メッシュ構造体のリスト

	// マテリアル構造体の定義
	struct material 
	{
		uint16_t unique_id{ 0 }; // マテリアルID
		std::string name;          // マテリアル名

		DirectX::XMFLOAT4 Ka{ 0.2f, 0.2f, 0.2f, 1.0f }; // アンビエントカラー
		DirectX::XMFLOAT4 Kd{ 0.8f, 0.8f, 0.8f, 1.0f }; // ディフューズカラー
		DirectX::XMFLOAT4 Ks{ 1.0f, 1.0f, 1.0f, 1.0f }; // スペキュラーカラー

		std::string texture_filenames[4]; // テクスチャファイル名の配列
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_views[4]; // テクスチャSRVの配列
	};

	std::unordered_map<uint64_t, material> materials; // マテリアルIDをキーとしたマテリアル構造体のマップ

private:
	Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex_shader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel_shader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> input_layout;
	Microsoft::WRL::ComPtr<ID3D11Buffer> constant_buffer;
	friend class skinned_mesh;

	// バウンディングボックス用のメンバ変数の追加
	DirectX::XMFLOAT3 bounding_box_min{ FLT_MAX, FLT_MAX, FLT_MAX };    // 最小座標
	DirectX::XMFLOAT3 bounding_box_max{ -FLT_MAX, -FLT_MAX, -FLT_MAX }; // 最大座標

public:
	// コンストラクタ：FBXファイルを読み込み、シーンやノードツリーを構築する
	skinned_mesh(ID3D11Device * device, const char* fbx_filename, bool triangulate = false);
	virtual ~skinned_mesh() = default;

	// FBXからメッシュ（頂点・インデックス）抽出
	void fetch_meshes(FbxScene* fbx_scene, std::vector<mesh>& meshes);

	// GPUバッファ（頂点/インデックスバッファ）生成
	void create_com_objects(ID3D11Device* device,const char* fbx_filename);

	// メッシュを描画する
	void render(ID3D11DeviceContext* immediate_context, 
		const DirectX::XMFLOAT4X4& world, 
		const DirectX::XMFLOAT4& material_color);

	// マテリアル抽出関数
	void fetch_materials(FbxScene* fbx_scene, 
		std::unordered_map<uint64_t, material>& materials
	);

	// バウンディングボックスの最小、最大座標を取得
	void get_bounding_box(DirectX::XMFLOAT3& min_vertex, DirectX::XMFLOAT3& max_vertex) const
	{
		min_vertex = bounding_box_min;
		max_vertex = bounding_box_max;
	}
	
protected:
	scene scene_view; // シーンビュー(ノード階層情報)
};