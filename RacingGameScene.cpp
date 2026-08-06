#include "framework.h"
#include "RacingGameScene.h"
#include <cmath> //（std::fmod 用）

RacingGameScene::RacingGameScene() {}
RacingGameScene::~RacingGameScene() {}

bool RacingGameScene::initialize(ID3D11Device* device)
{
	player = std::make_unique<Player>();
	player->initialize();

	cameraController = std::make_unique<CameraController>();
	totalTime = 0.0f;

	return true;
}

void RacingGameScene::update(float elapsedTime)
{
	if (player)
	{
		player->update(elapsedTime, nullptr);
		totalTime += elapsedTime;

		if (cameraController)
		{
			cameraController->update(
				elapsedTime,
				player->get_position(),
				player->get_angle().y,
				player->get_current_speed(),
				player->is_drifting(),
				player->get_drift_direction()
			);
		}
	}
}

void RacingGameScene::render(ID3D11DeviceContext* immediate_context, float elapsedTime)
{
#ifdef USE_IMGUI
	// 2D HUD (ImGui を使用したスピードメーターとタイム表示)
	ImGui::SetNextWindowPos(ImVec2(20, 20));
	ImGui::SetNextWindowSize(ImVec2(300, 160));

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
	ImGui::Begin("Racing HUD", nullptr, flags);

	if (player)
	{
		ImGui::Text("SPEED: %.1f km/h", player->get_current_speed() * 3.6f);
		ImGui::ProgressBar(player->get_current_speed() / player->get_top_speed(), ImVec2(-1, 20));

		if (player->is_drifting())
		{
			ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "DRIFT!");
		}
		else
		{
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "DRIVE");
		}
	}

	int minutes = static_cast<int>(totalTime) / 60;
	float seconds = std::fmod(totalTime, 60.0f);
	ImGui::Text("TIME: %02d:%05.2f", minutes, seconds);

	ImGui::End();
#endif
}

void RacingGameScene::uninitialize()
{
	player.reset();
	cameraController.reset();
}