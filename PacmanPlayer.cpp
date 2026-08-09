#define NOMINMAX
#include "PacmanPlayer.h"
#include "Collision.h"
#include <windows.h>
#include <algorithm>
#include <cmath>

using namespace DirectX;

void PacmanPlayer::initialize()
{
	position = { 4.0f, 0.5f, -4.0f };
	angle = { 0.0f, 0.0f, 0.0f };
	move_direction = { 0.0f, 1.0f };
	requested_direction = move_direction;
	hover_time = 0.0f;
	rebuild_collision_aabb();
	update_transform();
}

void PacmanPlayer::set_scale(const XMFLOAT3& value)
{
	scale = value;
	rebuild_collision_aabb();
}

void PacmanPlayer::set_collision_model_bounds(const XMFLOAT3& minimum, const XMFLOAT3& maximum)
{
	collision_model_min = minimum;
	collision_model_max = maximum;
	rebuild_collision_aabb();
}

void PacmanPlayer::rebuild_collision_aabb()
{
	// 描画モデルのローカル最大範囲を、現在のスケールでワールド半サイズへ変換する。
	const float half_x = (std::max)(std::fabs(collision_model_min.x), std::fabs(collision_model_max.x)) * std::fabs(scale.x);
	const float half_z = (std::max)(std::fabs(collision_model_min.z), std::fabs(collision_model_max.z)) * std::fabs(scale.z);
	collision_half_extent = { half_x, half_z };
}

void PacmanPlayer::read_direction_input()
{
	// 常に前進するパックマン型の操作。
	// Wは使わず、A/Dで左右の通路へ曲がり、Sで反対方向へ折り返す。
	const bool left_pressed = ((GetAsyncKeyState('A') & 0x8000) || (GetAsyncKeyState(VK_LEFT) & 0x8000)) != 0;
	const bool right_pressed = ((GetAsyncKeyState('D') & 0x8000) || (GetAsyncKeyState(VK_RIGHT) & 0x8000)) != 0;
	const bool reverse_pressed = ((GetAsyncKeyState('S') & 0x8000) || (GetAsyncKeyState(VK_DOWN) & 0x8000)) != 0;

	// キーを押した瞬間だけ方向を変える。押しっぱなしで毎フレーム回転しないようにする。
	if (left_pressed && !previous_left_pressed)
	{
		// 左回転: (x, z) -> (-z, x)
		requested_direction = { -move_direction.y, move_direction.x };
	}
	if (right_pressed && !previous_right_pressed)
	{
		// 右回転: (x, z) -> (z, -x)
		requested_direction = { move_direction.y, -move_direction.x };
	}
	if (reverse_pressed && !previous_reverse_pressed)
	{
		requested_direction = { -move_direction.x, -move_direction.y };
	}

	previous_left_pressed = left_pressed;
	previous_right_pressed = right_pressed;
	previous_reverse_pressed = reverse_pressed;
}

void PacmanPlayer::update(float elapsed_time, const static_mesh* collision_mesh, const XMFLOAT4X4& collision_world)
{
	hover_time += elapsed_time;
	read_direction_input();

	// 反転はいつでも即時に行える。左右旋回は交差点まで予約しておく。
	if (requested_direction.x != move_direction.x || requested_direction.y != move_direction.y)
	{
		const float direction_dot = move_direction.x * requested_direction.x +
			move_direction.y * requested_direction.y;
		if (direction_dot < -0.5f)
		{
			move_direction = requested_direction;
		}
		else if (collision_mesh != nullptr)
		{
			XMFLOAT3 snapped_position{};
			if (Collision::FindTurnSnapPosition2D(position, requested_direction, collision_half_extent,
				collision_world, collision_mesh, turn_snap_distance, snapped_position))
			{
				// 交差点の中心線へだけ補正してから旋回する。早押しでも壁には入らない。
				position = snapped_position;
				move_direction = requested_direction;
			}
		}
		else
		{
			move_direction = requested_direction;
		}
	}
	XMFLOAT3 next_position = position;
	next_position.x += move_direction.x * move_speed * elapsed_time;
	next_position.z += move_direction.y * move_speed * elapsed_time;

	// 自機AABBを壁AABBへスイープする。壁側を自機の半サイズぶん膨らませて
	// 中心の移動線分と交差させるため、キューブ全体が壁に入らない。
	const float movement_x = next_position.x - position.x;
	const float movement_z = next_position.z - position.z;
	float allowed_fraction = 1.0f;
	XMFLOAT3 hit_normal{};
	if (collision_mesh != nullptr)
	{
		Collision::SweepAABB2D(position, next_position, collision_half_extent,
			collision_world, collision_mesh, allowed_fraction, hit_normal);
	}
	position.x += movement_x * allowed_fraction;
	position.z += movement_z * allowed_fraction;
	angle.y = std::atan2(move_direction.x, move_direction.y);
	update_transform();
}

void PacmanPlayer::update_enemy(float elapsed_time, const static_mesh* collision_mesh, const XMFLOAT4X4& collision_world)
{
	hover_time += elapsed_time;
	const float step = move_speed * elapsed_time;
	XMFLOAT3 next_position = position;
	next_position.x += move_direction.x * step;
	next_position.z += move_direction.y * step;
	float allowed_fraction = 1.0f;
	XMFLOAT3 hit_normal{};
	const bool hit_wall = collision_mesh != nullptr && Collision::SweepAABB2D(
		position, next_position, collision_half_extent, collision_world, collision_mesh,
		allowed_fraction, hit_normal);

	position.x += (next_position.x - position.x) * allowed_fraction;
	position.z += (next_position.z - position.z) * allowed_fraction;
	if (hit_wall)
	{
		// xorshift: <random>に依存せず、敵ごとに異なる順番で4方向を試す。
		ai_random_state ^= ai_random_state << 13;
		ai_random_state ^= ai_random_state >> 17;
		ai_random_state ^= ai_random_state << 5;
		const XMFLOAT2 directions[] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
		const unsigned int first = ai_random_state % 4;
		for (unsigned int offset = 0; offset < 4; ++offset)
		{
			const XMFLOAT2 candidate_direction = directions[(first + offset) % 4];
			XMFLOAT3 candidate_position = position;
			// 直角方向へ曲がる場合だけ、プレイヤーと同じ交差点中心補正を利用する。
			const float dot = move_direction.x * candidate_direction.x + move_direction.y * candidate_direction.y;
			if (std::fabs(dot) < 0.5f && collision_mesh != nullptr)
			{
				if (!Collision::FindTurnSnapPosition2D(position, candidate_direction, collision_half_extent,
					collision_world, collision_mesh, turn_snap_distance, candidate_position))
					continue;
			}
			XMFLOAT3 test_end = candidate_position;
			test_end.x += candidate_direction.x * step;
			test_end.z += candidate_direction.y * step;
			float test_fraction = 1.0f;
			XMFLOAT3 test_normal{};
			if (collision_mesh == nullptr || !Collision::SweepAABB2D(candidate_position, test_end,
				collision_half_extent, collision_world, collision_mesh, test_fraction, test_normal))
			{
				position = candidate_position;
				move_direction = candidate_direction;
				break;
			}
		}
	}
	angle.y = std::atan2(move_direction.x, move_direction.y);
	update_transform();
}

void PacmanPlayer::update_transform()
{
	const float hover_offset = std::sinf(hover_time * hover_frequency * XM_2PI) * hover_amplitude;
	XMStoreFloat4x4(&transform,
		XMMatrixScaling(scale.x, scale.y, scale.z) *
		XMMatrixRotationY(angle.y) *
		XMMatrixTranslation(position.x, position.y + hover_offset, position.z));
}
