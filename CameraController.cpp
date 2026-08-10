#include "CameraController.h"
#include <cmath>
#include <algorithm>

namespace
{
	float Lerp(float a, float b, float t) { return a + (b - a) * t; }
	float Damp(float a, float b, float lambda, float dt) { return a + (b - a) * (1.0f - std::expf(-lambda * dt)); }
	float NormalizeAngleDiff(float diff)
	{
		while (diff < -DirectX::XM_PI) diff += DirectX::XM_2PI;
		while (diff > DirectX::XM_PI) diff -= DirectX::XM_2PI;
		return diff;
	}
}

void CameraController::update(float elapsedTime, const DirectX::XMFLOAT3& playerPos, float playerAngleY, float playerSpeed, bool isDrifting, float driftDir)
{
	if (elapsedTime <= 0.0f) elapsedTime = 0.001f;

	float speedRatio = (std::min)(playerSpeed / 30.0f, 1.0f);

	// 回転の追従
	float angleDiff = NormalizeAngleDiff(playerAngleY - currentCameraAngleY);
	currentCameraAngleY += angleDiff * rotationFollowSpeed * elapsedTime;

	float frontX = std::sinf(currentCameraAngleY);
	float frontZ = std::cosf(currentCameraAngleY);
	float rightX = std::cosf(currentCameraAngleY);
	float rightZ = -std::sinf(currentCameraAngleY);

	// 速度連動 FOV
	float targetFov = baseFov + (maxFov - baseFov) * speedRatio;
	currentFov = Damp(currentFov, targetFov, 2.5f, elapsedTime);

	// カメラ目標位置と注視点
	float dynamicRange = baseRange + speedRatio * 1.5f;
	float driftOffset = isDrifting ? 0.3f * driftDir : 0.0f;

	DirectX::XMFLOAT3 targetEye;
	targetEye.x = playerPos.x - frontX * dynamicRange + rightX * driftOffset;
	targetEye.y = playerPos.y + baseHeight;
	targetEye.z = playerPos.z - frontZ * dynamicRange + rightZ * driftOffset;

	DirectX::XMFLOAT3 idealFocus;
	idealFocus.x = playerPos.x + frontX * 3.0f;
	idealFocus.y = playerPos.y + focusHeight;
	idealFocus.z = playerPos.z + frontZ * 3.0f;

	currentEye.x = Damp(currentEye.x, targetEye.x, positionFollowSpeed, elapsedTime);
	currentEye.y = Damp(currentEye.y, targetEye.y, positionFollowSpeed, elapsedTime);
	currentEye.z = Damp(currentEye.z, targetEye.z, positionFollowSpeed, elapsedTime);

	currentFocus.x = Damp(currentFocus.x, idealFocus.x, focusFollowSpeed, elapsedTime);
	currentFocus.y = Damp(currentFocus.y, idealFocus.y, focusFollowSpeed, elapsedTime);
	currentFocus.z = Damp(currentFocus.z, idealFocus.z, focusFollowSpeed, elapsedTime);

	// ドリフト時の傾き（ロール）
	float targetRoll = 0.0f;
	if (playerSpeed > 5.0f)
	{
		targetRoll = -angleDiff * (isDrifting ? 0.1f : 0.05f);
	}
	static float currentRoll = 0.0f;
	currentRoll = Lerp(currentRoll, targetRoll, 5.0f * elapsedTime);

	cameraUp = { std::sinf(currentRoll), std::cosf(currentRoll), 0.0f };
}

void CameraController::update_cinematic_camera(float elapsed_time, const DirectX::XMFLOAT3& eye, const DirectX::XMFLOAT3& focus)
{
	if (elapsed_time <= 0.0f) elapsed_time = 0.001f;
	// 大きく視点が動くクリア演出なので、通常追従より少しゆっくり目に補間する。
	currentEye.x = Damp(currentEye.x, eye.x, 3.0f, elapsed_time);
	currentEye.y = Damp(currentEye.y, eye.y, 3.0f, elapsed_time);
	currentEye.z = Damp(currentEye.z, eye.z, 3.0f, elapsed_time);
	currentFocus.x = Damp(currentFocus.x, focus.x, 3.0f, elapsed_time);
	currentFocus.y = Damp(currentFocus.y, focus.y, 3.0f, elapsed_time);
	currentFocus.z = Damp(currentFocus.z, focus.z, 3.0f, elapsed_time);
	currentFov = Damp(currentFov, 52.0f, 3.0f, elapsed_time);
	cameraUp = { 0.0f, 1.0f, 0.0f };
}

bool CameraController::update_editor_camera(float elapsedTime, HWND hwnd, bool enabled, bool mouse_input_allowed)
{
	if (hwnd == nullptr)
	{
		stop_editor_camera();
		return false;
	}
	const bool right_button_down = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
	// 一度フリーカメラに入った後は、マウスがUI上に移動しても右クリックを離すまで継続する。
	const bool should_activate = enabled && right_button_down && (editor_camera_active || mouse_input_allowed);
	if (!should_activate)
	{
		stop_editor_camera();
		return false;
	}

	RECT client_rect{};
	GetClientRect(hwnd, &client_rect);
	POINT center{ (client_rect.right - client_rect.left) / 2, (client_rect.bottom - client_rect.top) / 2 };
	ClientToScreen(hwnd, &center);
	if (!editor_camera_active)
	{
		// 今の追従カメラの向きを引き継ぐため、切り替えた瞬間に視点が飛ばない。
		const DirectX::XMVECTOR eye = DirectX::XMLoadFloat3(&currentEye);
		const DirectX::XMVECTOR focus = DirectX::XMLoadFloat3(&currentFocus);
		DirectX::XMFLOAT3 direction{};
		DirectX::XMStoreFloat3(&direction,
			DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(focus, eye)));
		editor_position = currentEye;
		editor_yaw = std::atan2f(direction.x, direction.z);
		editor_pitch = std::asinf(direction.y);
		editor_camera_active = true;
		SetCapture(hwnd);
		ShowCursor(FALSE);
		SetCursorPos(center.x, center.y);
	}

	POINT cursor{};
	GetCursorPos(&cursor);
	const float delta_x = static_cast<float>(cursor.x - center.x);
	const float delta_y = static_cast<float>(cursor.y - center.y);
	editor_yaw += delta_x * editor_mouse_sensitivity;
	editor_pitch -= delta_y * editor_mouse_sensitivity;
	// 真上・真下を越えると上下が反転して操作しづらくなるため制限する。
	editor_pitch = (std::max)(-1.5f, (std::min)(editor_pitch, 1.5f));
	SetCursorPos(center.x, center.y);

	const float cos_pitch = std::cosf(editor_pitch);
	const DirectX::XMFLOAT3 forward{
		std::sinf(editor_yaw) * cos_pitch,
		std::sinf(editor_pitch),
		std::cosf(editor_yaw) * cos_pitch
	};
	const DirectX::XMFLOAT3 right{ std::cosf(editor_yaw), 0.0f, -std::sinf(editor_yaw) };
	float speed = editor_move_speed * elapsedTime;
	if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) speed *= 3.0f;
	if ((GetAsyncKeyState('W') & 0x8000) != 0) { editor_position.x += forward.x * speed; editor_position.y += forward.y * speed; editor_position.z += forward.z * speed; }
	if ((GetAsyncKeyState('S') & 0x8000) != 0) { editor_position.x -= forward.x * speed; editor_position.y -= forward.y * speed; editor_position.z -= forward.z * speed; }
	if ((GetAsyncKeyState('D') & 0x8000) != 0) { editor_position.x += right.x * speed; editor_position.z += right.z * speed; }
	if ((GetAsyncKeyState('A') & 0x8000) != 0) { editor_position.x -= right.x * speed; editor_position.z -= right.z * speed; }
	if ((GetAsyncKeyState('E') & 0x8000) != 0) editor_position.y += speed;
	if ((GetAsyncKeyState('Q') & 0x8000) != 0) editor_position.y -= speed;

	currentEye = editor_position;
	currentFocus = { editor_position.x + forward.x, editor_position.y + forward.y, editor_position.z + forward.z };
	cameraUp = { 0.0f, 1.0f, 0.0f };
	currentFov = baseFov;
	return true;
}

void CameraController::stop_editor_camera()
{
	if (!editor_camera_active) return;
	editor_camera_active = false;
	ReleaseCapture();
	ShowCursor(TRUE);
}
