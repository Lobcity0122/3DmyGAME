#pragma once

#include <DirectXMath.h>
class static_mesh;

// パックマン型のプレイヤー。
// 加速・ドリフトではなく、迷路のマス目に沿った一定速度の4方向移動を管理する。
class PacmanPlayer
{
public:
	void initialize();
	// collision_mesh は描画しない壁専用OBJ。指定時だけ前方レイとの交差を調べる。
	void update(float elapsed_time, const static_mesh* collision_mesh, const DirectX::XMFLOAT4X4& collision_world);
	// 入力を使わない敵用の移動。壁に着いた時だけ進める方向をランダムに選ぶ。
	void update_enemy(float elapsed_time, const static_mesh* collision_mesh, const DirectX::XMFLOAT4X4& collision_world);

	const DirectX::XMFLOAT3& get_position() const { return position; }
	void set_position(const DirectX::XMFLOAT3& value) { position = value; }
	const DirectX::XMFLOAT3& get_angle() const { return angle; }
	void set_angle(const DirectX::XMFLOAT3& value) { angle = value; }
	const DirectX::XMFLOAT3& get_scale() const { return scale; }
	void set_scale(const DirectX::XMFLOAT3& value);
	void set_collision_model_bounds(const DirectX::XMFLOAT3& minimum, const DirectX::XMFLOAT3& maximum);
	float get_hover_amplitude() const { return hover_amplitude; }
	float get_hover_frequency() const { return hover_frequency; }
	void set_hover_amplitude(float value) { hover_amplitude = value; }
	void set_hover_frequency(float value) { hover_frequency = value; }
	const DirectX::XMFLOAT4X4& get_transform() const { return transform; }
	float get_move_speed() const { return move_speed; }
	float get_turn_snap_distance() const { return turn_snap_distance; }
	const DirectX::XMFLOAT2& get_collision_half_extent() const { return collision_half_extent; }
	void set_turn_snap_distance(float value) { turn_snap_distance = value; }

private:
	void read_direction_input();
	void update_transform();
	void rebuild_collision_aabb();

	DirectX::XMFLOAT3 position{ 4.0f, 0.5f, -4.0f };
	DirectX::XMFLOAT3 angle{ 0.0f, 0.0f, 0.0f };
	// cube.obj は一辺2。迷路の通路幅より余裕を持たせた一辺約0.4にする。
	DirectX::XMFLOAT3 scale{ 0.2f, 0.2f, 0.2f };
	DirectX::XMFLOAT2 move_direction{ 0.0f, 1.0f };
	DirectX::XMFLOAT2 requested_direction{ 0.0f, 1.0f };
	DirectX::XMFLOAT4X4 transform{};
	float move_speed = 6.5f;
	// 静的メッシュの描画行列だけを上下させる、軽い浮遊演出。
	// position は変えないため、移動・当たり判定の座標には影響しない。
	float hover_time = 0.0f;
	float hover_amplitude = 0.1f;
	float hover_frequency = 0.5f; // cycles / second
	DirectX::XMFLOAT3 collision_model_min{ -1.0f, -1.0f, -1.0f };
	DirectX::XMFLOAT3 collision_model_max{ 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT2 collision_half_extent{ 0.2f, 0.2f };
	// 曲がる入力を受け付ける、通路中心からの最大ずれ。大きすぎると不自然な補正になる。
	float turn_snap_distance = 0.40f;
	bool previous_left_pressed = false;
	bool previous_right_pressed = false;
	bool previous_reverse_pressed = false;
	unsigned int ai_random_state = 0x9E3779B9u;
};
