#pragma once

#include <DirectXMath.h>

class CameraController
{
public:
	CameraController() = default;

	void update(float elapsedTime, const DirectX::XMFLOAT3& playerPos, float playerAngleY, float playerSpeed, bool isDrifting, float driftDir);

	const DirectX::XMFLOAT3& get_eye() const { return currentEye; }
	const DirectX::XMFLOAT3& get_focus() const { return currentFocus; }
	const DirectX::XMFLOAT3& get_up() const { return cameraUp; }
	float get_fov() const { return currentFov; }

private:
	DirectX::XMFLOAT3 currentEye = { 0, 3, -8 };
	DirectX::XMFLOAT3 currentFocus = { 0, 1, 0 };
	DirectX::XMFLOAT3 cameraUp = { 0, 1, 0 };

	float currentFov = 60.0f;
	float currentCameraAngleY = 0.0f;

	float baseRange = 6.0f;
	float baseHeight = 2.5f;
	float focusHeight = 0.8f;

	float baseFov = 60.0f;
	float maxFov = 85.0f;

	float positionFollowSpeed = 15.0f;
	float rotationFollowSpeed = 6.0f;
	float focusFollowSpeed = 8.0f;

	float shakeTimer = 0.0f;
};