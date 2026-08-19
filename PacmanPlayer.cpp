#define NOMINMAX
#include "PacmanPlayer.h"
#include "Collision.h"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <vector>

using namespace DirectX;

void PacmanPlayer::initialize()
{
	position = { 4.0f, 0.5f, -4.0f };
	angle = { 0.0f, 0.0f, 0.0f };
	move_direction = { 0.0f, 1.0f };
	requested_direction = move_direction;
	hover_time = 0.0f;
	ai_decision_cooldown = 0.0f;
	rebuild_collision_aabb();
	update_transform();
}

void PacmanPlayer::set_scale(const XMFLOAT3& value)
{
	scale = value;
	rebuild_collision_aabb();
}

void PacmanPlayer::set_position(const XMFLOAT3& value)
{
	position = value;
	// ワープ・リスポーン直後の座標も、そのフレーム内に描画へ反映する。
	update_transform();
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

bool PacmanPlayer::move_with_collision(float elapsed_time, const static_mesh* collision_mesh,
	const XMFLOAT4X4& collision_world)
{
	XMFLOAT3 next_position = position;
	next_position.x += move_direction.x * move_speed * elapsed_time;
	next_position.z += move_direction.y * move_speed * elapsed_time;

	float allowed_fraction = 1.0f;
	XMFLOAT3 hit_normal{};
	const bool hit_wall = collision_mesh != nullptr && Collision::SweepAABB2D(
		position, next_position, collision_half_extent, collision_world, collision_mesh,
		allowed_fraction, hit_normal);
	position.x += (next_position.x - position.x) * allowed_fraction;
	position.z += (next_position.z - position.z) * allowed_fraction;
	return hit_wall;
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
	// 自機AABBを壁AABBへスイープする。壁側を自機の半サイズぶん膨らませて
	// 中心の移動線分と交差させるため、キューブ全体が壁に入らない。
	move_with_collision(elapsed_time, collision_mesh, collision_world);
	angle.y = std::atan2(move_direction.x, move_direction.y);
	update_transform();
}

void PacmanPlayer::update_enemy(float elapsed_time, const static_mesh* collision_mesh,
	const XMFLOAT4X4& collision_world, const XMFLOAT3* target_position, float chase_range)
{
	hover_time += elapsed_time;
	ai_decision_cooldown = (std::max)(ai_decision_cooldown - elapsed_time, 0.0f);
	const bool has_target = target_position != nullptr && chase_range > 0.0f;
	const float target_dx = has_target ? target_position->x - position.x : 0.0f;
	const float target_dz = has_target ? target_position->z - position.z : 0.0f;
	const bool is_chasing = has_target &&
		(target_dx * target_dx + target_dz * target_dz <= chase_range * chase_range);
	const float step = move_speed * elapsed_time;

	// 交差点または行き止まりで次の方向を選ぶ。通常の交差点では反転を除外し、
	// 前方が塞がれたときだけ反転を許可して、無意味な往復を防ぐ。
	const auto choose_direction = [&](bool allow_reverse, bool require_side_turn)
	{
		struct Candidate
		{
			XMFLOAT2 direction;
			XMFLOAT3 snapped_position;
			float distance_squared;
			bool side_turn;
		};
		std::vector<Candidate> candidates;
		const XMFLOAT2 directions[] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
		const float probe_distance = (std::max)(step, 0.30f);
		for (const XMFLOAT2& candidate_direction : directions)
		{
			const float dot = move_direction.x * candidate_direction.x + move_direction.y * candidate_direction.y;
			if (!allow_reverse && dot < -0.5f) continue;

			XMFLOAT3 candidate_position = position;
			const bool side_turn = std::fabs(dot) < 0.5f;
			if (side_turn && collision_mesh != nullptr)
			{
				if (!Collision::FindTurnSnapPosition2D(position, candidate_direction, collision_half_extent,
					collision_world, collision_mesh, turn_snap_distance, candidate_position))
					continue;
			}

			XMFLOAT3 test_end = candidate_position;
			test_end.x += candidate_direction.x * probe_distance;
			test_end.z += candidate_direction.y * probe_distance;
			float test_fraction = 1.0f;
			XMFLOAT3 test_normal{};
			if (collision_mesh != nullptr && Collision::SweepAABB2D(candidate_position, test_end,
				collision_half_extent, collision_world, collision_mesh, test_fraction, test_normal))
				continue;

			const float dx = has_target ? test_end.x - target_position->x : 0.0f;
			const float dz = has_target ? test_end.z - target_position->z : 0.0f;
			candidates.push_back({ candidate_direction, candidate_position, dx * dx + dz * dz, side_turn });
		}

		bool has_side_turn = false;
		for (const Candidate& candidate : candidates) has_side_turn |= candidate.side_turn;
		if (candidates.empty() || (require_side_turn && !has_side_turn)) return false;

		ai_random_state ^= ai_random_state << 13;
		ai_random_state ^= ai_random_state >> 17;
		ai_random_state ^= ai_random_state << 5;
		size_t chosen_index = ai_random_state % candidates.size();
		if (is_chasing)
		{
			for (size_t index = 1; index < candidates.size(); ++index)
				if (candidates[index].distance_squared < candidates[chosen_index].distance_squared)
					chosen_index = index;
		}
		position = candidates[chosen_index].snapped_position;
		move_direction = candidates[chosen_index].direction;
		return true;
	};

	// 横方向へ有効な通路があれば本物の交差点。壁へ届く前に選択することで、
	// パトロールが単なるUターンだけにならず迷路全体を巡回する。
	if (ai_decision_cooldown <= 0.0f && choose_direction(false, true))
		ai_decision_cooldown = 0.20f;

	const bool hit_wall = move_with_collision(elapsed_time, collision_mesh, collision_world);
	if (hit_wall)
	{
		choose_direction(true, false);
		ai_decision_cooldown = 0.20f;
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
