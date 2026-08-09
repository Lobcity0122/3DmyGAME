#define NOMINMAX
#include "PacmanPlayer.h"
#include "Collision.h"
#include <windows.h>
#include <cmath>

using namespace DirectX;

void PacmanPlayer::initialize()
{
	position = { 4.0f, 0.5f, -4.225f };
	angle = { 0.0f, 0.0f, 0.0f };
	move_direction = { 0.0f, 1.0f };
	requested_direction = move_direction;
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

void PacmanPlayer::update(float elapsed_time, const static_mesh* stage_mesh, const XMFLOAT4X4& stage_world)
{
	read_direction_input();

	// 方向転換時は、横方向の座標を最寄りのグリッドへ吸着する。
	// これにより自由移動ではなく、迷路通路を曲がる感覚になる。
	if (requested_direction.x != move_direction.x || requested_direction.y != move_direction.y)
	{
		if (requested_direction.x != 0.0f) position.z = std::round(position.z / grid_size) * grid_size;
		if (requested_direction.y != 0.0f) position.x = std::round(position.x / grid_size) * grid_size;
		move_direction = requested_direction;
	}

	const XMFLOAT3 previous_position = position;
	XMFLOAT3 next_position = position;
	next_position.x += move_direction.x * move_speed * elapsed_time;
	next_position.z += move_direction.y * move_speed * elapsed_time;

	// ステージの全三角形に対して、進行先へのレイが壁へ交差するか調べる。
	// 交差したフレームは前進せず、壁の手前で止まる。
	XMFLOAT3 hit_position{}, hit_normal{};
	if (!Collision::RayCastStaticMesh(previous_position, next_position, stage_world, stage_mesh, hit_position, hit_normal))
	{
		position = next_position;
	}
	angle.y = std::atan2(move_direction.x, move_direction.y);
	update_transform();
}

void PacmanPlayer::update_transform()
{
	XMStoreFloat4x4(&transform,
		XMMatrixScaling(scale.x, scale.y, scale.z) *
		XMMatrixRotationY(angle.y) *
		XMMatrixTranslation(position.x, position.y, position.z));
}
