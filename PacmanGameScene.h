#pragma once

#include "Scene.h"
#include "PacmanPlayer.h"
#include "CameraController.h"
#include "static_mesh.h"
#include "skinned_mesh.h"
#include "geometric_primitive.h"
#include <memory>
#include <vector>
#include <wrl.h>

// このクラスは3Dパックマンライクなゲーム状態だけを管理する。
class PacmanGameScene final : public Scene
{
public:
	// false は通常プレイ、true はタイトル用の無操作デモ。
	explicit PacmanGameScene(bool attract_mode = false) : attract_mode(attract_mode) {}
	bool initialize(ID3D11Device* device) override;
	void update(float elapsed_time) override;
	void render(ID3D11DeviceContext* immediate_context, float elapsed_time) override;
	void uninitialize() override;
	SceneType get_type() const override { return attract_mode ? SceneType::PACMAN_ATTRACT : SceneType::PACMAN; }
	SceneType get_next_scene() const override { return next_scene_type; }

private:
	// PacmanPlayerがグリッド移動を扱い、CameraControllerが追従ビューを作る。
	std::unique_ptr<PacmanPlayer> player;
	std::unique_ptr<PacmanPlayer> enemy;
	std::unique_ptr<PacmanPlayer> enemy_second;
	std::unique_ptr<CameraController> camera_controller;

	// 2つの独立したモデル読み込み用パス。ここに並べておくことで、違いや使い方が一目で比較できる。
	std::unique_ptr<static_mesh> player_mesh;       // OBJ（プレイヤー）
	std::unique_ptr<static_mesh> stage_mesh;        // OBJ（静的メッシュ：コース）
	std::unique_ptr<static_mesh> collision_mesh;    // 描画しない壁専用OBJ（当たり判定）
	std::unique_ptr<static_mesh> circuit_mesh;      // 描画しない通路専用OBJ（回路復旧判定）
	std::unique_ptr<static_mesh> background_mesh;  // OBJ（背景用の静的メッシュ）
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
		DirectX::XMFLOAT4X4 light_view_projection;
		DirectX::XMFLOAT4 shadow_settings;
		DirectX::XMFLOAT4 post_process_settings;
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
		float exposure_ev = 1.0f;
		bool use_point_light = false;
		bool use_shadows = true;
		bool use_pbr_lighting = true;
		bool unlit_texture_check = false;
	};
	LightSettings light_settings;

	// Unity / Unreal のSceneビューを参考にした、編集時だけ表示する補助線の設定。
	struct EditorDebugSettings
	{
		bool enable_editor_camera = true;
		bool show_grid = true;
		bool show_axis_gizmo = true;
		bool show_collision_model = false;
		bool rotate_background = true;
		float background_rotation_speed = 1.0f; // degrees / second
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
	ObjectTransform background_transform;

	Microsoft::WRL::ComPtr<ID3D11Buffer> scene_constant_buffer;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> shadow_depth_texture;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> shadow_depth_stencil_view;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shadow_shader_resource_view;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> shadow_sampler_state;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> shadow_rasterizer_state;
	// カプセル背景を内側から見るため、背面も描画する専用ラスタライザ。
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> background_rasterizer_state;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> collision_wireframe_rasterizer_state;
	DirectX::XMFLOAT4X4 stage_world{};
	DirectX::XMFLOAT4X4 background_world{};
	float total_time = 0.0f;
	// Score belongs to the play scene. ResultScene only displays the completed record.
	int score = 0;
	float survival_bonus_timer = 0.0f;
	// Enemy switches from patrol to chase only inside this horizontal range.
	float enemy_chase_range = 12.0f;

	// 敵接触後の処理を、通常プレイから分離して分かりやすく管理する。
	enum class GameState { Playing, Respawning, GameOverFade, GameClearFade };
	GameState game_state = GameState::Playing;
	int lives = 3;
	float state_timer = 0.0f;
	bool player_visible = true;
	bool exit_requested = false;
	// デモは本編と同一の更新・描画経路を使い、プレイヤーの入力だけをAIへ差し替える。
	bool attract_mode = false;
	SceneType next_scene_type = SceneType::PACMAN;
	bool previous_enter_pressed = false;
	bool previous_escape_pressed = false;
	DirectX::XMFLOAT3 player_spawn_position{};
	DirectX::XMFLOAT3 enemy_spawn_position{};
	DirectX::XMFLOAT3 enemy_second_spawn_position{};

	// Make Trax型の「復旧済み回路」。敵ではなく自機が通過した区間だけを保持する。
	struct CircuitSegment
	{
		DirectX::XMFLOAT3 start;
		DirectX::XMFLOAT3 end;
	};
	std::vector<CircuitSegment> player_circuit_segments;
	struct CircuitCell
	{
		DirectX::XMFLOAT3 minimum;
		DirectX::XMFLOAT3 maximum;
		bool recovered = false;
	};
	std::vector<CircuitCell> circuit_cells;
	// ImGuiで選んだ解像度は、次フレームの開始時にGPUテクスチャへ反映する。
	UINT shadow_map_size = 2048;
	UINT requested_shadow_map_size = 2048;

	// 描画関数は毎フレーム同じ順序でこれらを呼び出す。
	void update_scene_constants(ID3D11DeviceContext* immediate_context);
	void render_shadow_map(ID3D11DeviceContext* immediate_context);
	bool create_shadow_map(ID3D11Device* device, UINT size);
	DirectX::XMMATRIX calculate_light_view_projection() const;
	void configure_object_transforms();
	void update_object_world_matrices();
	void draw_models(ID3D11DeviceContext* immediate_context);
	void draw_editor_helpers(ID3D11DeviceContext* immediate_context);
	void draw_hud();
	void record_player_circuit(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end);
	void draw_player_circuit(ID3D11DeviceContext* immediate_context);
	void build_circuit_cells();
	// Returns how many previously-unrecovered corridor cells were touched this frame.
	int recover_circuit_cells_at(const DirectX::XMFLOAT3& position);
	int get_recovered_circuit_cell_count() const;
	bool is_player_touching_enemy() const;
	void begin_respawn_or_game_over();
	void begin_game_clear();
	void finish_to_result();
};
