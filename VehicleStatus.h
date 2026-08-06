#pragma once

#include <string>

struct VehicleStatus
{
	std::string name;
	std::string modelPath;
	float scale = 50.0f;

	float topSpeed = 25.0f;
	float acceleration = 10.0f;
	float normalTurnSpeed = 35.0f;
	float turnSpeed = 70.0f;
	float inputSmoothSpeed = 5.0f;

	float driftMinSpeed = 8.0f;
	float driftGripPower = 5.0f;
	float driftAutoTurnPower = 0.3f;
	float driftTurnPowerIn = 1.0f;
	float driftTurnPowerOut = 0.5f;
	float driftSpeedMultiplier = 0.85f;
	float driftDeceleration = 5.0f;

	float driftVisualBaseAngle = 0.45f;
	float driftVisualAddAngle = 0.15f;
	float visualDriftLerpSpeed = 4.5f;
	float normalVisualLerpSpeed = 4.5f;

	float friction = 6.0f;

	VehicleStatus() = default;
};

inline VehicleStatus GetDefaultCar()
{
	VehicleStatus status;
	status.name = "Default Race Car";
	status.topSpeed = 30.0f;
	status.acceleration = 12.0f;
	status.normalTurnSpeed = 45.0f;
	status.turnSpeed = 80.0f;
	status.inputSmoothSpeed = 6.0f;
	status.driftMinSpeed = 8.0f;
	status.driftGripPower = 4.5f;
	status.driftAutoTurnPower = 0.35f;
	status.driftTurnPowerIn = 1.2f;
	status.driftTurnPowerOut = 0.6f;
	status.driftSpeedMultiplier = 0.9f;
	status.driftDeceleration = 4.0f;
	status.friction = 5.0f;
	return status;
}