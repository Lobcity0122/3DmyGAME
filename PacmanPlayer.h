#pragma once

#include <DirectXMath.h>

class static_mesh;

// 迷路の上を一定速度で移動するアクター。自機と敵は同じ移動・衝突処理を使い、
// 自機だけはキーボード入力、敵だけは経路選択AIによって方向を決める。
class PacmanPlayer
{
public:
	void initialize();

	// 自機用の更新。A/Dで曲がり、Sで反転する。
	void update(float elapsed_time, const static_mesh* collision_mesh,
		const DirectX::XMFLOAT4X4& collision_world);
	// 敵用の更新。目標が追跡範囲にいるときだけ、目標へ近づく経路を優先する。
	void update_enemy(float elapsed_time, const static_mesh* collision_mesh,
		const DirectX::XMFLOAT4X4& collision_world, const DirectX::XMFLOAT3* target_position = nullptr,
		float chase_range = 0.0f);

	// -------------------------------------------------------------------------
	// シーン側から利用する状態の取得・設定
	// -------------------------------------------------------------------------
	const DirectX::XMFLOAT3& get_position() const { return position; }
	const DirectX::XMFLOAT3& get_angle() const { return angle; }
	const DirectX::XMFLOAT3& get_scale() const { return scale; }
	const DirectX::XMFLOAT4X4& get_transform() const { return transform; }
	const DirectX::XMFLOAT2& get_collision_half_extent() const { return collision_half_extent; }
	float get_move_speed() const { return move_speed; }
	float get_turn_snap_distance() const { return turn_snap_distance; }
	float get_hover_amplitude() const { return hover_amplitude; }
	float get_hover_frequency() const { return hover_frequency; }

	void set_position(const DirectX::XMFLOAT3& value);
	void set_angle(const DirectX::XMFLOAT3& value) { angle = value; }
	void set_scale(const DirectX::XMFLOAT3& value);
	void set_collision_model_bounds(const DirectX::XMFLOAT3& minimum, const DirectX::XMFLOAT3& maximum);
	void set_turn_snap_distance(float value) { turn_snap_distance = value; }
	void set_hover_amplitude(float value) { hover_amplitude = value; }
	void set_hover_frequency(float value) { hover_frequency = value; }

private:
	// -------------------------------------------------------------------------
	// 入力・移動・衝突判定
	// -------------------------------------------------------------------------
	void read_direction_input();
	// 次の位置までAABBを移動させる。壁に衝突した場合はtrueを返す。
	bool move_with_collision(float elapsed_time, const static_mesh* collision_mesh,
		const DirectX::XMFLOAT4X4& collision_world);
	void rebuild_collision_aabb();
	void update_transform();

	// -------------------------------------------------------------------------
	// 共有アクター状態
	// -------------------------------------------------------------------------
	DirectX::XMFLOAT3 position{ 4.0f, 0.5f, -4.0f };
	DirectX::XMFLOAT3 angle{ 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 scale{ 0.2f, 0.2f, 0.2f };
	DirectX::XMFLOAT2 move_direction{ 0.0f, 1.0f };
	DirectX::XMFLOAT2 requested_direction{ 0.0f, 1.0f };
	DirectX::XMFLOAT4X4 transform{};
	float move_speed = 4.5f;

	// 描画だけを上下させる軽い浮遊演出。positionは変えないので、移動と
	// 当たり判定の座標には影響しない。
	float hover_time = 0.0f;
	float hover_amplitude = 0.1f;
	float hover_frequency = 0.5f; // 1秒あたりの周期数。

	// 描画モデルのローカル境界から計算した、XZ平面での当たり判定半サイズ。
	DirectX::XMFLOAT3 collision_model_min{ -1.0f, -1.0f, -1.0f };
	DirectX::XMFLOAT3 collision_model_max{ 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT2 collision_half_extent{ 0.2f, 0.2f };
	float turn_snap_distance = 0.40f;

	// -------------------------------------------------------------------------
	// 自機入力と敵AIだけが使用する個別状態
	// -------------------------------------------------------------------------
	bool previous_left_pressed = false;
	bool previous_right_pressed = false;
	bool previous_reverse_pressed = false;
	unsigned int ai_random_state = 0x9E3779B9u;
	float ai_decision_cooldown = 0.0f;
};
