#pragma once

#include "CameraController.h"
#include "PacmanPlayer.h"
#include "Scene.h"
#include "geometric_primitive.h"
#include "skinned_mesh.h"
#include "sprite.h"
#include "static_mesh.h"

#include <memory>
#include <vector>
#include <wrl.h>

// CIRCUIT TRAX の1プレイ分を管理する。通常プレイと、タイトルで流す
// 無操作のアトラクトデモは同じルール・描画経路を共有する。
class PacmanGameScene final : public Scene
{
public:
	// falseなら通常プレイ、trueなら同じ迷路を使うタイトル用アトラクトモード。
	explicit PacmanGameScene(bool is_attract_mode = false) : attract_mode(is_attract_mode) {}

	bool initialize(ID3D11Device* device) override;
	void update(float elapsed_time) override;
	void render(ID3D11DeviceContext* immediate_context, float elapsed_time) override;
	void uninitialize() override;

	SceneType get_type() const override
	{
		return attract_mode ? SceneType::PACMAN_ATTRACT : SceneType::PACMAN;
	}
	SceneType get_next_scene() const override { return next_scene_type; }

private:
	// -------------------------------------------------------------------------
	// シーンオブジェクトとGPUリソース
	// -------------------------------------------------------------------------
	std::unique_ptr<PacmanPlayer> player;
	std::unique_ptr<PacmanPlayer> enemy;
	std::unique_ptr<PacmanPlayer> enemy_second;
	std::unique_ptr<CameraController> camera_controller;
	std::unique_ptr<sprite> hud_font;

	std::unique_ptr<static_mesh> player_mesh;
	std::unique_ptr<static_mesh> stage_mesh;
	std::unique_ptr<static_mesh> background_mesh;
	std::unique_ptr<static_mesh> collision_mesh; // 描画しない当たり判定専用OBJ。
	std::unique_ptr<static_mesh> circuit_mesh;   // 回路復旧に使う通路セル。
	std::unique_ptr<cube> debug_cube;

	// static_mesh.hlsli のSCENE_CONSTANT_BUFFER（レジスタb1）と一致する。
	// 全モデルがここから共通のカメラ・ライト・影データを読む。
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

	Microsoft::WRL::ComPtr<ID3D11Buffer> scene_constant_buffer;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> shadow_depth_texture;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> shadow_depth_stencil_view;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shadow_shader_resource_view;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> shadow_sampler_state;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> shadow_rasterizer_state;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> background_rasterizer_state;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> collision_wireframe_rasterizer_state;

	// -------------------------------------------------------------------------
	// シーンの見た目に関する設定
	// -------------------------------------------------------------------------
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
	// ライト編集値を背景と同じY回転で描画へ渡す。元の編集値は保持するため、
	// 背景回転を止めてもライトが累積してずれることはない。
	bool rotate_light_with_background = true;

	struct EditorDebugSettings
	{
		bool enable_editor_camera = true;
		bool show_grid = true;
		bool show_axis_gizmo = true;
		bool show_collision_model = false;
		bool rotate_background = true;
		float background_rotation_speed = 2.0f; // 1秒あたりの回転角度（度）。
		float grid_half_size = 20.0f;
		float grid_spacing = 1.0f;
		float axis_length = 2.0f;
	};
	EditorDebugSettings editor_debug;

	// 編集しやすい「度」で保持する。update_object_world_matrices() が
	// 描画器で使う行列へ変換する。
	struct ObjectTransform
	{
		DirectX::XMFLOAT3 position{ 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 rotation_degrees{ 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 scale{ 1.0f, 1.0f, 1.0f };
	};
	ObjectTransform stage_transform;
	ObjectTransform background_transform;
	DirectX::XMFLOAT4X4 stage_world{};
	DirectX::XMFLOAT4X4 background_world{};

	UINT shadow_map_size = 2048;
	UINT requested_shadow_map_size = 2048;

	// -------------------------------------------------------------------------
	// プレイ状態、スコア、AI調整値
	// -------------------------------------------------------------------------
	enum class GameState { Playing, Respawning, GameOverFade, GameClearFade };
	GameState game_state = GameState::Playing;
	int lives = 3;
	float state_timer = 0.0f;
	float total_time = 0.0f;
	bool player_visible = true;
	bool exit_requested = false;

	int score = 0;
	float survival_bonus_timer = 0.0f;
	int recovery_chain = 0;
	float recovery_chain_time_remaining = 0.0f;
	static constexpr float recovery_chain_window_seconds = 4.5f;

	float near_miss_radius = 2.50f;
	float near_miss_cooldown_seconds = 1.50f;
	float enemy_near_miss_cooldown = 0.0f;
	float enemy_second_near_miss_cooldown = 0.0f;
	float near_miss_popup_time = 0.0f;
	int near_miss_popup_score = 0;
	// ニアミス成功を認識しやすくするため、得点表示と画面枠を少し長めに残す。
	static constexpr float near_miss_effect_duration = 1.25f;
	// 接近と被弾を画面全体で分かりやすく伝える短い演出タイマー。
	float damage_flash_time = 0.0f;
	static constexpr float damage_flash_duration = 0.42f;

	// HUDとライティング演出だけを変える。敵AIの速度は変えない。
	int system_alert_level = 0;
	float system_alert_popup_time = 0.0f;
	float enemy_chase_range = 12.0f;
	float enemy_intercept_distance = 6.0f;

	// -------------------------------------------------------------------------
	// 迷路データとワープゲート
	// -------------------------------------------------------------------------
	struct CircuitSegment
	{
		DirectX::XMFLOAT3 start;
		DirectX::XMFLOAT3 end;
	};
	struct CircuitCell
	{
		DirectX::XMFLOAT3 minimum;
		DirectX::XMFLOAT3 maximum;
		bool recovered = false;
	};
	std::vector<CircuitSegment> player_circuit_segments;
	std::vector<CircuitCell> circuit_cells;

	// 座標はstage_transform適用後のワールド座標。出口を判定範囲の内側へ
	// 置くことで、次フレームに即座に逆ワープしないようにしている。
	struct WarpTunnelSettings
	{
		bool enabled = true;
		float center_z = -0.003f;
		float half_width_z = 0.75f;
		float left_trigger_x = -11.20f;
		float right_trigger_x = 10.90f;
		float exit_at_left_x = -10.95f;
		float exit_at_right_x = 10.70f;
	};
	WarpTunnelSettings warp_tunnel;

	// -------------------------------------------------------------------------
	// 入力、シーン遷移、戦術マップ表示
	// -------------------------------------------------------------------------
	bool attract_mode = false;
	SceneType next_scene_type = SceneType::PACMAN;
	bool previous_enter_pressed = false;
	bool previous_escape_pressed = false;
	bool show_development_debug = false;
	bool previous_debug_toggle_pressed = false;
	bool paused = false;
	bool previous_pause_pressed = false;

	bool show_minimap = true;
	bool rotate_minimap_with_player = true;
	float minimap_rotation_angle = 0.0f;
	float minimap_rotation_follow_speed = 5.0f;

	DirectX::XMFLOAT3 player_spawn_position{};
	DirectX::XMFLOAT3 enemy_spawn_position{};
	DirectX::XMFLOAT3 enemy_second_spawn_position{};

	// -------------------------------------------------------------------------
	// 更新・ゲームルール用ヘルパー
	// -------------------------------------------------------------------------
	void configure_object_transforms();
	void update_object_world_matrices();
	void update_minimap_rotation(float elapsed_time);
	void update_system_alert(float elapsed_time);
	void record_player_circuit(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end);
	void build_circuit_cells();
	int recover_circuit_cells_at(const DirectX::XMFLOAT3& position);
	int get_recovered_circuit_cell_count() const;
	int get_recovery_chain_multiplier() const;
	bool apply_warp_tunnel(PacmanPlayer& actor);
	bool is_player_touching_enemy() const;
	bool award_near_miss_if_needed(const PacmanPlayer& other, float& cooldown);
	void begin_respawn_or_game_over();
	void begin_game_clear();
	void finish_to_result();

	// -------------------------------------------------------------------------
	// 描画用ヘルパー
	// -------------------------------------------------------------------------
	bool create_shadow_map(ID3D11Device* device, UINT size);
	LightSettings get_render_light_settings() const;
	DirectX::XMMATRIX calculate_light_view_projection() const;
	void render_shadow_map(ID3D11DeviceContext* immediate_context);
	void update_scene_constants(ID3D11DeviceContext* immediate_context);
	void draw_models(ID3D11DeviceContext* immediate_context);
	void draw_player_circuit(ID3D11DeviceContext* immediate_context);
	void draw_editor_helpers(ID3D11DeviceContext* immediate_context);
	void draw_hud(ID3D11DeviceContext* immediate_context);
	void draw_gameplay_hud(ID3D11DeviceContext* immediate_context);
	void draw_attract_hud(ID3D11DeviceContext* immediate_context);
	void draw_minimap();
};
