#define NOMINMAX
#include "PacmanPlayer.h"
#include "Collision.h"
#include <windows.h>
#include <cmath>

using namespace DirectX;

void PacmanPlayer::initialize()
{
	position = { 4.0f, 0.5f, -4.0f };
	angle = { 0.0f, 0.0f, 0.0f };
	move_direction = { 0.0f, 1.0f };
	requested_direction = move_direction;
	hover_time = 0.0f;
	update_transform();
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

	// 当たり判定を持たない基礎状態では、入力された方向へすぐに切り替えて前進する。
	if (requested_direction.x != move_direction.x || requested_direction.y != move_direction.y)
	{
		move_direction = requested_direction;
	}
	XMFLOAT3 next_position = position;
	next_position.x += move_direction.x * move_speed * elapsed_time;
	next_position.z += move_direction.y * move_speed * elapsed_time;

	// まずは自機中心から前方へ1本だけレイを飛ばす、最小構成の判定。
	// 壁専用モデル以外は判定しないため、見た目の床・装飾には引っ掛からない。
	XMFLOAT3 hit_position{}, hit_normal{};
	if (collision_mesh == nullptr || !Collision::RayCastStaticMesh(position, next_position,
		collision_world, collision_mesh, hit_position, hit_normal))
	{
		position = next_position;
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
