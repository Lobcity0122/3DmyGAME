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

	float speedRatio = std::min(playerSpeed / 30.0f, 1.0f);

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

	// 振動効果
	if (playerSpeed > 2.0f)
	{
		shakeTimer += elapsedTime * 15.0f;
		float intensity = (playerSpeed / 30.0f) * 0.1f * (isDrifting ? 1.5f : 1.0f);
		currentEye.x += std::sinf(shakeTimer) * intensity * 0.05f;
		currentEye.y += std::cosf(shakeTimer * 1.3f) * intensity * 0.05f;
	}

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