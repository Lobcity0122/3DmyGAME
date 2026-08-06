#define NOMINMAX // Windowsマクロの min/max 汚染を防止
#include "Player.h"
#include <windows.h>
#include <cmath>
#include <algorithm>

using namespace DirectX;

// 定数の定義
const float status_base_angle = 0.45f;
const float status_add_angle = 0.15f;
const float status_lerp_speed = 4.5f;

Player::Player()
{
	DirectX::XMStoreFloat4x4(&transform, DirectX::XMMatrixIdentity());
	apply_status(GetDefaultCar());
}

Player::~Player() {}

void Player::initialize()
{
	position = { 0.0f, 0.0f, 0.0f };
	angle = { 0.0f, 0.0f, 0.0f };
	velocity = { 0.0f, 0.0f, 0.0f };
	currentSpeed = 0.0f;
	isDrifting = false;
	visualDriftAngle = 0.0f;
}

void Player::apply_status(const VehicleStatus& status)
{
	topSpeed = status.topSpeed;
	acceleration = status.acceleration;
	normalTurnSpeed = status.normalTurnSpeed;
	turnSpeed = status.turnSpeed;
	inputSmoothSpeed = status.inputSmoothSpeed;
	driftMinSpeed = status.driftMinSpeed;
	driftGripPower = status.driftGripPower;
	driftAutoTurnPower = status.driftAutoTurnPower;
	driftTurnPowerIn = status.driftTurnPowerIn;
	driftTurnPowerOut = status.driftTurnPowerOut;
	driftSpeedMultiplier = status.driftSpeedMultiplier;
	driftDeceleration = status.driftDeceleration;
	friction = status.friction;
}

void Player::handle_input(float elapsedTime)
{
	// 左右ステアリング入力
	float rawInputX = 0.0f;
	if ((GetAsyncKeyState('A') & 0x8000) || (GetAsyncKeyState(VK_LEFT) & 0x8000)) rawInputX -= 1.0f;
	if ((GetAsyncKeyState('D') & 0x8000) || (GetAsyncKeyState(VK_RIGHT) & 0x8000)) rawInputX += 1.0f;

	// アクセル / ブレーキ入力
	bool isAccel = (GetAsyncKeyState('W') & 0x8000) || (GetAsyncKeyState(VK_UP) & 0x8000);
	bool isBrake = (GetAsyncKeyState('S') & 0x8000) || (GetAsyncKeyState(VK_DOWN) & 0x8000);
	bool isDriftKey = (GetAsyncKeyState(VK_SHIFT) & 0x8000) || (GetAsyncKeyState(VK_SPACE) & 0x8000);

	// スムージング処理
	smoothedInputX += (rawInputX - smoothedInputX) * inputSmoothSpeed * elapsedTime;

	// ドリフト判定
	if (!isDrifting)
	{
		if (isDriftKey && std::fabs(rawInputX) > 0.1f && currentSpeed > driftMinSpeed)
		{
			driftDirection = (rawInputX < 0.0f) ? -1.0f : 1.0f;
			isDrifting = true;
		}
	}
	else
	{
		if (!isDriftKey || currentSpeed < 2.0f)
		{
			isDrifting = false;
			driftDirection = 0.0f;
		}
	}

	// 速度の加速・減速
	float targetTopSpeed = isDrifting ? (topSpeed * driftSpeedMultiplier) : topSpeed;

	if (isAccel)
	{
		if (currentSpeed < targetTopSpeed)
		{
			currentSpeed += acceleration * elapsedTime;
		}
		else
		{
			currentSpeed -= friction * elapsedTime;
		}
	}
	else if (isBrake)
	{
		currentSpeed -= acceleration * 1.5f * elapsedTime;
		if (currentSpeed < -topSpeed * 0.3f) // バック限界
		{
			currentSpeed = -topSpeed * 0.3f;
		}
	}
	else
	{
		// 自然減速
		if (currentSpeed > 0.0f)
		{
			currentSpeed -= friction * elapsedTime;
			if (currentSpeed < 0.0f) currentSpeed = 0.0f;
		}
		else if (currentSpeed < 0.0f)
		{
			currentSpeed += friction * elapsedTime;
			if (currentSpeed > 0.0f) currentSpeed = 0.0f;
		}
	}

	// 旋回計算
	float speedDampingFactor = 0.015f;
	float steerDamping = 1.0f + (currentSpeed * speedDampingFactor);

	if (!isDrifting)
	{
		float actualNormalTurn = normalTurnSpeed / steerDamping;
		float baseNormalTurnSpeed = DirectX::XMConvertToRadians(actualNormalTurn);
		angle.y += baseNormalTurnSpeed * smoothedInputX * elapsedTime;

		visualDriftAngle += (0.0f - visualDriftAngle) * status_lerp_speed * elapsedTime;
	}
	else
	{
		float actualDriftTurn = turnSpeed / (1.0f + (currentSpeed * speedDampingFactor * 0.5f));
		float baseDriftTurnSpeed = DirectX::XMConvertToRadians(actualDriftTurn);
		float inputForce = smoothedInputX * driftDirection;
		float turnRate = baseDriftTurnSpeed * driftAutoTurnPower;

		if (inputForce > 0.0f)
		{
			turnRate += baseDriftTurnSpeed * driftTurnPowerIn * inputForce;
		}
		else if (inputForce < 0.0f)
		{
			float counterRate = baseDriftTurnSpeed * driftTurnPowerOut * (-inputForce);
			turnRate -= counterRate;
		}

		angle.y += turnRate * driftDirection * elapsedTime;

		float targetVisualAngle = driftDirection * status_base_angle;
		targetVisualAngle += driftDirection * (inputForce > 0.0f ? status_add_angle * inputForce : status_add_angle * 0.8f * inputForce);
		visualDriftAngle += (targetVisualAngle - visualDriftAngle) * 4.5f * elapsedTime;
	}

	// 進行方向ベクトルの算出
	float frontX = std::sinf(angle.y);
	float frontZ = std::cosf(angle.y);
	float targetVx = frontX * currentSpeed;
	float targetVz = frontZ * currentSpeed;

	// グリップ力による速度ベクトルの追従
	float gripFactor = (isDrifting ? driftGripPower : 15.0f) * elapsedTime;
	gripFactor = std::min(gripFactor, 1.0f);

	velocity.x = velocity.x * (1.0f - gripFactor) + targetVx * gripFactor;
	velocity.z = velocity.z * (1.0f - gripFactor) + targetVz * gripFactor;
}

void Player::update_velocity(float elapsedTime, Stage* stage)
{
	position.x += velocity.x * elapsedTime;
	position.z += velocity.z * elapsedTime;

	// Y軸（重力と接地判定）
	velocity.y += gravity * elapsedTime;
	position.y += velocity.y * elapsedTime;

	if (position.y <= 0.0f) // 簡易接地
	{
		position.y = 0.0f;
		velocity.y = 0.0f;
		isGround = true;
	}
}

void Player::update(float elapsedTime, Stage* stage)
{
	handle_input(elapsedTime);
	update_velocity(elapsedTime, stage);

	// ワールド行列の更新
	XMMATRIX S = XMMatrixScaling(scale.x, scale.y, scale.z);
	XMMATRIX R = XMMatrixRotationRollPitchYaw(angle.x, angle.y + visualDriftAngle, angle.z);
	XMMATRIX T = XMMatrixTranslation(position.x, position.y, position.z);
	XMStoreFloat4x4(&transform, S * R * T);
}