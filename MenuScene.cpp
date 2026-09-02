#include "MenuScene.h"

#include <windows.h>
#include <cmath>
#include <cstdio>

namespace
{
	constexpr float screen_width = 1280.0f;
	constexpr float screen_height = 720.0f;
	constexpr float attract_start_delay = 18.0f;
}

bool MenuScene::initialize(ID3D11Device* device)
{
	next_scene_type = SceneType::MENU;
	idle_seconds = 0.0f;
	title_pulse_time = 0.0f;
	previous_enter_pressed = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;
	background = std::make_unique<sprite>(device, L".\\resources\\cyberpunk.jpg");
	font = std::make_unique<sprite>(device, L".\\resources\\fonts\\font0.png");
	return true;
}

void MenuScene::update(float elapsed_time)
{
	title_pulse_time += elapsed_time;
	const bool enter_pressed = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;
	if (enter_pressed && !previous_enter_pressed)
	{
		next_scene_type = SceneType::PACMAN;
		return;
	}
	previous_enter_pressed = enter_pressed;

	// 放置時だけ3Dデモへ移り、デモ中のEnterはそのまま本編開始になる。
	if (enter_pressed) idle_seconds = 0.0f;
	else idle_seconds += elapsed_time;
	if (idle_seconds >= attract_start_delay) next_scene_type = SceneType::PACMAN_ATTRACT;
}

void MenuScene::render(ID3D11DeviceContext* immediate_context, float)
{
	background->render(immediate_context, 0.0f, 0.0f, screen_width, screen_height,
		0.05f, 0.07f, 0.16f, 1.0f, 0.0f);
	const auto text = [this, immediate_context](const char* value, float x, float y, float size,
		float r, float g, float b)
	{
		font->textout(immediate_context, value, x, y, size, size, r, g, b, 1.0f);
	};

	const float pulse = 0.72f + 0.28f * (0.5f + 0.5f * std::sinf(title_pulse_time * 2.0f));
	text("CIRCUIT TRAX", 395.0f, 185.0f, 42.0f, 0.15f, pulse, 0.64f);
	text("RESTORE THE LOST GRID", 455.0f, 250.0f, 17.0f, 0.76f, 0.92f, 1.0f);
	text("AVOID SECURITY DRONES. COMPLETE EVERY CIRCUIT.", 325.0f, 335.0f, 15.0f, 0.72f, 0.86f, 1.0f);
	text("REBUILD THE NETWORK. SURVIVE THE SYSTEM.", 360.0f, 365.0f, 15.0f, 0.72f, 0.86f, 1.0f);

	char score_line[64]{};
	std::snprintf(score_line, sizeof(score_line), "HIGH SCORE %06d", session_high_score);
	text(score_line, 505.0f, 445.0f, 17.0f, 1.0f, 0.82f, 0.20f);
	if (std::fmod(title_pulse_time, 1.0f) < 0.75f)
		text("[ENTER] START GAME", 482.0f, 585.0f, 20.0f, 0.15f, 1.0f, 0.65f);
	text("IDLE: LIVE DEMO", 545.0f, 630.0f, 12.0f, 0.42f, 0.78f, 0.88f);
}

void MenuScene::uninitialize()
{
	font.reset();
	background.reset();
}
