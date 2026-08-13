#include "MenuScene.h"
#include <windows.h>
#include <cmath>
#include <algorithm>

namespace
{
	constexpr float screen_width = 1280.0f;
	constexpr float screen_height = 720.0f;
}

MenuScene::MenuScene() = default;
MenuScene::~MenuScene() = default;

bool MenuScene::initialize(ID3D11Device* device)
{
	next_scene_type = SceneType::MENU;
	selected_game = 0;
	locked_message_timer = 0.0f;
	background = std::make_unique<sprite>(device, L".\\resources\\cyberpunk.jpg");
	font = std::make_unique<sprite>(device, L".\\resources\\fonts\\font0.png");
	return true;
}

void MenuScene::update(float elapsed_time)
{
	locked_message_timer = (std::max)(locked_message_timer - elapsed_time, 0.0f);
	const bool left_pressed = ((GetAsyncKeyState(VK_LEFT) & 0x8000) || (GetAsyncKeyState('A') & 0x8000)) != 0;
	const bool right_pressed = ((GetAsyncKeyState(VK_RIGHT) & 0x8000) || (GetAsyncKeyState('D') & 0x8000)) != 0;
	const bool enter_pressed = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;
	if (left_pressed && !previous_left_pressed) selected_game = (selected_game + 2) % 3;
	if (right_pressed && !previous_right_pressed) selected_game = (selected_game + 1) % 3;
	if (enter_pressed && !previous_enter_pressed)
	{
		// BOOT直後は本編ではなく、アーケード筐体らしいタイトル／デモ画面へ入る。
		if (selected_game == 0) next_scene_type = SceneType::PACMAN_ATTRACT;
		else locked_message_timer = 1.5f;
	}
	previous_left_pressed = left_pressed;
	previous_right_pressed = right_pressed;
	previous_enter_pressed = enter_pressed;
}

void MenuScene::render(ID3D11DeviceContext* immediate_context, float)
{
	// 閭梧勹繧呈囓縺上＠縺ｦ縲√◎縺ｮ荳翫↓繧ｲ繝ｼ繝讖溘・遶ｯ譛ｫ逕ｻ髱｢繧帝㍾縺ｭ繧九・	background->render(immediate_context, 0.0f, 0.0f, screen_width, screen_height, 0.08f, 0.10f, 0.22f, 1.0f, 0.0f);
	const auto text = [this, immediate_context](const char* value, float x, float y, float size,
		float r, float g, float b)
	{
		font->textout(immediate_context, value, x, y, size, size, r, g, b, 1.0f);
	};

	text("NEXUS-01 GAME TERMINAL", 180, 60, 25, 0.25f, 1.0f, 0.85f);
	text("SYSTEM STATUS: ONLINE", 180, 100, 14, 0.45f, 0.80f, 1.0f);
	text("----------------------------------------------------------------", 150, 135, 10, 0.15f, 0.85f, 0.65f);

	const char* titles[] = { "CIRCUIT TRAX", "VOID RACER", "STAR RELAY" };
	const char* statuses[] = { "ONLINE", "LOCKED", "COMING SOON" };
	const float card_x[] = { 150.0f, 470.0f, 790.0f };
	for (int index = 0; index < 3; ++index)
	{
		const bool selected = selected_game == index;
		const float r = selected ? 0.15f : 0.35f;
		const float g = selected ? 1.00f : 0.35f;
		const float b = selected ? 0.65f : 0.55f;
		text(selected ? ">" : " ", card_x[index], 255, 24, r, g, b);
		text(titles[index], card_x[index] + 28, 255, selected ? 22.0f : 17.0f, r, g, b);
		text(statuses[index], card_x[index] + 28, 292, 14, r, g, b);
		text("[----------]", card_x[index] + 28, 325, 14, r, g, b);
	}

	text("CIRCUIT TRAX", 180, 450, 21, 0.25f, 1.0f, 0.85f);
	text("RESTORE THE LOST GRID.", 180, 485, 14, 0.75f, 0.90f, 1.0f);
	text("AVOID SECURITY DRONES. COMPLETE EVERY CIRCUIT.", 180, 512, 14, 0.75f, 0.90f, 1.0f);
	text("[LEFT][RIGHT]: SELECT     [ENTER]: BOOT", 180, 630, 15, 0.25f, 1.0f, 0.85f);
	if (locked_message_timer > 0.0f)
		text("GAME DATA LOCKED", 490, 570, 18, 1.0f, 0.25f, 0.25f);
}

void MenuScene::uninitialize()
{
	font.reset();
	background.reset();
}
