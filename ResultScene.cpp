#include "ResultScene.h"
#include <windows.h>
#include <cstdio>
#include <cmath>

namespace
{
	// リザルト画面専用の固定解像度レイアウト。画面サイズ対応を行う場合は、
	// この2値を基準に描画座標をスケーリングする。
	constexpr float screen_width = 1280.0f;
	constexpr float screen_height = 720.0f;
}

bool ResultScene::initialize(ID3D11Device* device)
{
	next_scene_type = SceneType::PACMAN_RESULT;
	blink_time = 0.0f;
	previous_enter_pressed = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;
	background = std::make_unique<sprite>(device, L".\\resources\\cyberpunk.jpg");
	font = std::make_unique<sprite>(device, L".\\resources\\fonts\\font0.png");
	return true;
}

void ResultScene::update(float elapsed_time)
{
	// Enterを押した瞬間だけ遷移する。押しっぱなしで連続遷移しないようにする。
	blink_time += elapsed_time;
	const bool enter_pressed = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;
	if (enter_pressed && !previous_enter_pressed)
		next_scene_type = SceneType::PACMAN_ATTRACT;
	previous_enter_pressed = enter_pressed;
}

void ResultScene::render(ID3D11DeviceContext* immediate_context, float)
{
	// 背景、見出し、プレイ結果、戻る案内の順に重ねて描画する。
	background->render(immediate_context, 0.0f, 0.0f, screen_width, screen_height,
		0.035f, 0.045f, 0.10f, 1.0f, 0.0f);
	const auto text = [this, immediate_context](const char* value, float x, float y,
		float size, float r, float g, float b)
	{
		font->textout(immediate_context, value, x, y, size, size, r, g, b, 1.0f);
	};

	const GameResultData& result = latest_game_result;
	const float title_r = result.cleared ? 0.15f : 1.0f;
	const float title_g = result.cleared ? 1.0f : 0.18f;
	const float title_b = result.cleared ? 0.60f : 0.18f;
	text("CIRCUIT TRAX", 455.0f, 105.0f, 30.0f, 0.35f, 0.85f, 1.0f);
	text(result.cleared ? "CIRCUIT RESTORED" : "SYSTEM FAILURE", 395.0f, 185.0f, 30.0f, title_r, title_g, title_b);
	text("----------------------------------------------", 335.0f, 235.0f, 12.0f, 0.2f, 0.75f, 0.72f);

	char line[96]{};
	std::snprintf(line, sizeof(line), "CIRCUIT RECOVERY  %d / %d", result.recovered_circuits, result.total_circuits);
	text(line, 435.0f, 300.0f, 17.0f, 0.85f, 0.92f, 1.0f);
	std::snprintf(line, sizeof(line), "REMAINING LIVES   %d", result.remaining_lives);
	text(line, 435.0f, 338.0f, 17.0f, 0.85f, 0.92f, 1.0f);
	const int minutes = static_cast<int>(result.elapsed_seconds) / 60;
	const float seconds = std::fmod(result.elapsed_seconds, 60.0f);
	std::snprintf(line, sizeof(line), "MISSION TIME     %02d:%05.2f", minutes, seconds);
	text(line, 435.0f, 376.0f, 17.0f, 0.85f, 0.92f, 1.0f);
	std::snprintf(line, sizeof(line), "SCORE            %d", result.score);
	text(line, 435.0f, 414.0f, 17.0f, 0.15f, 1.0f, 0.60f);
	std::snprintf(line, sizeof(line), "HIGH SCORE       %d", result.high_score);
	text(line, 435.0f, 452.0f, 17.0f, 1.0f, 0.82f, 0.20f);

	if (std::fmod(blink_time, 1.0f) < 0.72f)
		text("[ENTER] RETURN TO TITLE", 445.0f, 570.0f, 18.0f, 0.15f, 1.0f, 0.60f);
}

void ResultScene::uninitialize()
{
	// unique_ptrのリセットにより、シーン専用のGPUリソースを解放する。
	font.reset();
	background.reset();
}
