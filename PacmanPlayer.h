#pragma once

#include <DirectXMath.h>
class static_mesh;

// パックマン型のプレイヤー。
// 加速・ドリフトではなく、迷路のマス目に沿った一定速度の4方向移動を管理する。
class PacmanPlayer
{
public:
	void initialize();
	void update(float elapsed_time, const static_mesh* stage_mesh, const DirectX::XMFLOAT4X4& stage_world);

	const DirectX::XMFLOAT3& get_position() const { return position; }
	void set_position(const DirectX::XMFLOAT3& value) { position = value; }
	const DirectX::XMFLOAT3& get_angle() const { return angle; }
	void set_angle(const DirectX::XMFLOAT3& value) { angle = value; }
	const DirectX::XMFLOAT3& get_scale() const { return scale; }
	void set_scale(const DirectX::XMFLOAT3& value) { scale = value; }
	const DirectX::XMFLOAT4X4& get_transform() const { return transform; }
	float get_move_speed() const { return move_speed; }

private:
	void read_direction_input();
	void update_transform();

	DirectX::XMFLOAT3 position{ 4.0f, 0.5f, -4.225f };
	DirectX::XMFLOAT3 angle{ 0.0f, 0.0f, 0.0f };
	// cube.obj は一辺2。迷路の通路幅より余裕を持たせた一辺約0.4にする。
	DirectX::XMFLOAT3 scale{ 0.2f, 0.2f, 0.2f };
	DirectX::XMFLOAT2 move_direction{ 0.0f, 1.0f };
	DirectX::XMFLOAT2 requested_direction{ 0.0f, 1.0f };
	DirectX::XMFLOAT4X4 transform{};
	float move_speed = 3.5f;
	float grid_size = 1.0f;
	bool previous_left_pressed = false;
	bool previous_right_pressed = false;
	bool previous_reverse_pressed = false;
};
