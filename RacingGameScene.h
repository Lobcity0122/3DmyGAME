#pragma once

#include "Scene.h"
#include "Player.h"
#include "CameraController.h"
#include "static_mesh.h"
#include "skinned_mesh.h"
#include "geometric_primitive.h"
#include <memory>
#include <wrl.h>

// このクラスはレース専用の状態のみを管理。framework 側はどのモデルが使われているかを知らない。
class RacingGameScene final : public Scene
{
public:
	bool initialize(ID3D11Device* device) override;
	void update(float elapsed_time) override;
	void render(ID3D11DeviceContext* immediate_context, float elapsed_time) override;
	void uninitialize() override;
	SceneType get_type() const override { return SceneType::RACING; }

private:
	// ゲーム状態: Playerが入力/移動を扱い、CameraControllerがそれをビューカメラへ変換する。
	std::unique_ptr<Player> player;
	std::unique_ptr<CameraController> camera_controller;

	// 2つの独立したモデル読み込み用パス。ここに並べておくことで、違いや使い方が一目で比較できる。
	std::unique_ptr<static_mesh> car_mesh;          // OBJ（静的メッシュ：車など）
	std::unique_ptr<static_mesh> stage_mesh;        // OBJ（静的メッシュ：コース）
	std::unique_ptr<skinned_mesh> character_mesh;   // FBX（スキンメッシュ：キャラなど）
	std::unique_ptr<cube> debug_cube;               // グリッドと軸の線を描くための簡易プリミティブ

	// シェーダーレジスタ b1: シーン内の全モデルで共有されるデータ。
	// 各メッシュは独自の b0 バッファ（ワールド行列とマテリアルカラー）を作成・更新。
	struct SceneConstants
	{
		DirectX::XMFLOAT4X4 view_projection;
		DirectX::XMFLOAT4 light_direction;
		DirectX::XMFLOAT4 camera_position;
		DirectX::XMFLOAT4 light_position_range;
		DirectX::XMFLOAT4 light_color_intensity;
		DirectX::XMFLOAT4 ambient_color_intensity;
		DirectX::XMFLOAT4 render_options;
	};

	// シーン内の光源設定。これをシェーダーに渡す。
	struct LightSettings
	{
		DirectX::XMFLOAT3 direction{ -0.35f, -1.0f, 0.25f };
		DirectX::XMFLOAT3 position{ 0.0f, 8.0f, 0.0f };
		DirectX::XMFLOAT3 color{ 1.0f, 1.0f, 1.0f };
		DirectX::XMFLOAT3 ambient_color{ 0.60f, 0.72f, 0.90f };
		float range = 30.0f;
		float intensity = 1.25f;
		float ambient_intensity = 0.35f;
		bool use_point_light = false;
		bool unlit_texture_check = false;
	};
	LightSettings light_settings;

	// Unity / Unreal のSceneビューを参考にした、編集時だけ表示する補助線の設定。
	struct EditorDebugSettings
	{
		bool show_grid = true;
		bool show_axis_gizmo = true;
		float grid_half_size = 20.0f;
		float grid_spacing = 1.0f;
		float axis_length = 2.0f;
	};
	EditorDebugSettings editor_debug;

	// Player 以外の静的なオブジェクト用 Transform。
	// 回転は ImGui で扱いやすい「度」で保持し、描画直前に行列へ変換する。
	struct ObjectTransform
	{
		DirectX::XMFLOAT3 position{ 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 rotation_degrees{ 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 scale{ 1.0f, 1.0f, 1.0f };
	};
	ObjectTransform stage_transform;
	ObjectTransform character_transform;

	Microsoft::WRL::ComPtr<ID3D11Buffer> scene_constant_buffer;
	DirectX::XMFLOAT4X4 stage_world{};
	DirectX::XMFLOAT4X4 character_world{};
	float total_time = 0.0f;

	// 描画関数は毎フレーム同じ順序でこれらを呼び出す。
	void update_scene_constants(ID3D11DeviceContext* immediate_context);
	void configure_object_transforms();
	void update_object_world_matrices();
	void draw_models(ID3D11DeviceContext* immediate_context);
	void draw_editor_helpers(ID3D11DeviceContext* immediate_context);
	void draw_hud();
};
