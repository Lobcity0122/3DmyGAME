#include "framework.h"
#include "RacingGameScene.h"
#include <cmath> //（std::fmod 用）

using namespace DirectX;

RacingGameScene::RacingGameScene() {}
RacingGameScene::~RacingGameScene() {}

bool RacingGameScene::initialize(ID3D11Device* device)
{
	player = std::make_unique<Player>();
	player->initialize();

	cameraController = std::make_unique<CameraController>();
	totalTime = 0.0f;

	// FBXモデルの読み込み (ファイルパスはプロジェクト内の配置に合わせて調整)
	// 例: ".\\resources\\cube.000.fbx" や ".\\resources\\cube.001.fbx" など
	carMesh = std::make_unique<skinned_mesh>(device, ".\\resources\\desktop\\desktop.fbx", true);

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
	// ----------------------------------------------------
	// 1. 定数バッファ（カメラのビュー・プロジェクション行列）の設定
	// ----------------------------------------------------
	if (cameraController)
	{
		D3D11_VIEWPORT viewport{};
		UINT num_viewports{ 1 };
		immediate_context->RSGetViewports(&num_viewports, &viewport);

		float aspect_ratio = viewport.Width / viewport.Height;

		// CameraController が計算した FOV、カメラ位置(Eye)、注視点(Focus)、上方向(Up)を使用
		XMMATRIX P = XMMatrixPerspectiveFovLH(XMConvertToRadians(cameraController->get_fov()), aspect_ratio, 0.1f, 300.0f);

		XMVECTOR eye = XMLoadFloat3(&cameraController->get_eye());
		XMVECTOR focus = XMLoadFloat3(&cameraController->get_focus());
		XMVECTOR up = XMLoadFloat3(&cameraController->get_up());

		XMMATRIX V = XMMatrixLookAtLH(eye, focus, up);

		// 定数バッファの更新 (frameworkの構造体に合わせる)
		framework::scene_constans scene_data{};
		XMStoreFloat4x4(&scene_data.view_projection, V * P);
		scene_data.light_direction = XMFLOAT4(0.0f, 1.0f, 1.0f, 0.0f); // ライトの向き
		scene_data.camera_position = XMFLOAT4(cameraController->get_eye().x, cameraController->get_eye().y, cameraController->get_eye().z, 1.0f);

		// スロット1の定数バッファ（VS / PS）にセット
		// ※framework側の constant_buffers[0] を利用する場合は framework 経由またはシーン内で更新
	}

	// ----------------------------------------------------
	// 2. 3Dモデル（車体）の描画
	// ----------------------------------------------------
	if (carMesh && player)
	{
		// Player が計算したワールド行列（位置・Y軸回転・ドリフト傾き）を取得
		XMFLOAT4X4 carWorld = player->get_transform();

		// 車体カラー（RGBA）
		XMFLOAT4 materialColor = { 1.0f, 1.0f, 1.0f, 1.0f };

		// 3D描画用のステート設定
		// （※framework 側でバインドされているステートでそのまま描画できます）
		carMesh->render(immediate_context, carWorld, materialColor);
	}

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
	carMesh.reset();
	player.reset();
	cameraController.reset();
}