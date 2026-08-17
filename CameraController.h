#pragma once

#include <windows.h>
#include <DirectXMath.h>

class CameraController
{
public:
	CameraController() = default;

	void update(float elapsedTime, const DirectX::XMFLOAT3& playerPos, float playerAngleY, float playerSpeed, bool isDrifting, float driftDir);
	// クリア演出用。プレイヤー追従ではなく、指定したステージ俯瞰カメラへ滑らかに移す。
	void update_cinematic_camera(float elapsed_time, const DirectX::XMFLOAT3& eye, const DirectX::XMFLOAT3& focus);
	// 右クリック中だけ使う、ステージ確認用の自由カメラ。
	// 戻り値が true の間は、通常のプレイヤー追従カメラを更新しない。
	bool update_editor_camera(float elapsedTime, HWND hwnd, bool enabled, bool mouse_input_allowed);
	void stop_editor_camera();

	const DirectX::XMFLOAT3& get_eye() const { return currentEye; }
	const DirectX::XMFLOAT3& get_focus() const { return currentFocus; }
	const DirectX::XMFLOAT3& get_up() const { return cameraUp; }
	float get_fov() const { return currentFov; }
	float get_follow_range() const { return baseRange; }
	float get_follow_height() const { return baseHeight; }
	float get_follow_focus_height() const { return focusHeight; }
	void set_follow_range(float value) { baseRange = value; }
	void set_follow_height(float value) { baseHeight = value; }
	void set_follow_focus_height(float value) { focusHeight = value; }
	bool get_follow_rotation() const { return follow_rotation; }
	void set_follow_rotation(bool value) { follow_rotation = value; }

private:
	DirectX::XMFLOAT3 currentEye = { 0, 3, -8 };
	DirectX::XMFLOAT3 currentFocus = { 0, 1, 0 };
	DirectX::XMFLOAT3 cameraUp = { 0, 1, 0 };

	float currentFov = 60.0f;
	float currentCameraAngleY = 0.0f;

	// 迷路全体を見失わない、やや高めの斜め見下ろし追従カメラ。
	float baseRange = 7.0f;
	float baseHeight = 8.0f;
	float focusHeight = 0.0f;

	float baseFov = 60.0f;

	float positionFollowSpeed = 15.0f;
	float rotationFollowSpeed = 6.0f;
	float focusFollowSpeed = 8.0f;
	// 自機の進行方向には追従するが、FOVと画面ロールは固定して酔いを抑える。
	bool follow_rotation = true;

	bool editor_camera_active = false;
	DirectX::XMFLOAT3 editor_position{};
	float editor_yaw = 0.0f;
	float editor_pitch = 0.0f;
	float editor_move_speed = 8.0f;
	float editor_mouse_sensitivity = 0.003f;
};
