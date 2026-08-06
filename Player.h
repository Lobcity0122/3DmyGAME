#pragma once

#include <memory>
#include <DirectXMath.h>
#include "VehicleStatus.h"

class Stage;

class Player
{
public:
	Player();
	~Player();

	void initialize();
	void update(float elapsedTime, Stage* stage);

	const DirectX::XMFLOAT3& get_position() const { return position; }
	void set_position(const DirectX::XMFLOAT3& pos) { position = pos; }

	const DirectX::XMFLOAT3& get_angle() const { return angle; }
	void set_angle(const DirectX::XMFLOAT3& ang) { angle = ang; }

	float get_current_speed() const { return currentSpeed; }
	float get_top_speed() const { return topSpeed; }
	bool is_drifting() const { return isDrifting; }
	float get_drift_direction() const { return driftDirection; }
	float get_visual_drift_angle() const { return visualDriftAngle; }

	const DirectX::XMFLOAT4X4& get_transform() const { return transform; }

	void apply_status(const VehicleStatus& status);

private:
	void handle_input(float elapsedTime);
	void update_velocity(float elapsedTime, Stage* stage);

private:
	DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 angle = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 velocity = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT4X4 transform;

	bool isDrifting = false;
	float visualDriftAngle = 0.0f;
	float currentSpeed = 0.0f;
	float driftDirection = 0.0f;
	float smoothedInputX = 0.0f;

	// ÉpÉâÉÅÅ[É^
	float topSpeed = 30.0f;
	float acceleration = 12.0f;
	float normalTurnSpeed = 45.0f;
	float turnSpeed = 80.0f;
	float inputSmoothSpeed = 6.0f;
	float driftMinSpeed = 8.0f;
	float driftGripPower = 4.5f;
	float driftAutoTurnPower = 0.35f;
	float driftTurnPowerIn = 1.2f;
	float driftTurnPowerOut = 0.6f;
	float driftSpeedMultiplier = 0.9f;
	float driftDeceleration = 4.0f;
	float friction = 5.0f;

	float radius = 0.5f;
	float gravity = -60.0f;
	bool isGround = false;
};