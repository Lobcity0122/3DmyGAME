#include "MenuScene.h"
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

MenuScene::MenuScene() {}
MenuScene::~MenuScene() {}

bool MenuScene::initialize(ID3D11Device* device)
{
	next_scene_type = SceneType::MENU; // 初期化時に自分に戻す
	return true;
}

void MenuScene::update(float elapsed_time)
{
	// ロジック更新が必要ならここに書く
}

void MenuScene::render(ID3D11DeviceContext* immediate_context, float elapsed_time)
{
#ifdef USE_IMGUI
	// PC画面・OS風のUIウィンドウ
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(1280, 720));

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse;

	ImGui::Begin("DesktopUI", nullptr, flags);

	ImGui::Text("=== SELECT GAME ===");
	ImGui::Spacing();

	// レースゲーム起動ボタン
	if (ImGui::Button("Launch Racing Game", ImVec2(200, 50)))
	{
		next_scene_type = SceneType::RACING; // 遷移フラグを立てる
	}

	ImGui::Spacing();

	// OnlyUp起動ボタン
	if (ImGui::Button("Launch OnlyUp Game", ImVec2(200, 50)))
	{
		next_scene_type = SceneType::ONLYUP; // 遷移フラグを立てる
	}

	ImGui::End();
#endif
}

void MenuScene::uninitialize() {}
