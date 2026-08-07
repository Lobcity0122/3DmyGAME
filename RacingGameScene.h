#pragma once

#include "Scene.h"
#include "Player.h"
#include "CameraController.h"
#include "static_mesh.h"
#include "skinned_mesh.h"
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

	// シェーダーレジスタ b1: シーン内の全モデルで共有されるデータ。
	// 各メッシュは独自の b0 バッファ（ワールド行列とマテリアルカラー）を作成・更新。
	struct SceneConstants
	{
		DirectX::XMFLOAT4X4 view_projection;
		DirectX::XMFLOAT4 light_direction;
		DirectX::XMFLOAT4 camera_position;
		DirectX::XMFLOAT4 light_position_range;
		DirectX::XMFLOAT4 light_color_intensity;
		DirectX::XMFLOAT4 render_options;
	};

	// Values exposed by the ImGui debug window. They are copied into SceneConstants each frame.
	struct LightSettings
	{
		DirectX::XMFLOAT3 direction{ 0.0f, 1.0f, 1.0f };
		DirectX::XMFLOAT3 position{ 0.0f, 8.0f, 0.0f };
		DirectX::XMFLOAT3 color{ 1.0f, 1.0f, 1.0f };
		float range = 30.0f;
		float intensity = 1.0f;
		bool use_point_light = true;
		bool unlit_texture_check = false;
	};
	LightSettings light_settings;
	Microsoft::WRL::ComPtr<ID3D11Buffer> scene_constant_buffer;
	DirectX::XMFLOAT4X4 stage_world{};
	DirectX::XMFLOAT4X4 character_world{};
	float total_time = 0.0f;

	// 描画関数は毎フレーム同じ順序でこれらを呼び出す。
	void update_scene_constants(ID3D11DeviceContext* immediate_context);
	void draw_models(ID3D11DeviceContext* immediate_context);
	void draw_hud();
};
