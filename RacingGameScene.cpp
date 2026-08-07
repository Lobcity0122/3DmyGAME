#include "RacingGameScene.h"
#include <cmath>
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

using namespace DirectX;

RacingGameScene::RacingGameScene() {}
RacingGameScene::~RacingGameScene() {}

bool RacingGameScene::initialize(ID3D11Device* device)
{
	player = std::make_unique<Player>();
	player->initialize();

	cameraController = std::make_unique<CameraController>();
	totalTime = 0.0f;

	// static_mesh (.obj繝｢繝・Ν) 縺ｮ隱ｭ縺ｿ霎ｼ縺ｿ
	// 窶ｻ縺頑戟縺｡縺ｮ .obj 繝輔ぃ繧､繝ｫ繝代せ縺ｫ蜷医ｏ縺帙※菫ｮ豁｣縺励※縺上□縺輔＞ (萓・ L".\\resources\\car\\car.obj")
	carMesh = std::make_unique<static_mesh>(device, L".\\resources\\car\\123.obj");
	characterMesh = std::make_unique<skinned_mesh>(device, ".\\resources\\cube.000.fbx", true);

	D3D11_BUFFER_DESC cb_desc{};
	cb_desc.ByteWidth = sizeof(SceneConstants);
	cb_desc.Usage = D3D11_USAGE_DEFAULT;
	cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	if (FAILED(device->CreateBuffer(&cb_desc, nullptr, sceneConstantBuffer.GetAddressOf()))) return false;
	XMStoreFloat4x4(&characterWorld, XMMatrixTranslation(4.0f, 0.0f, 4.0f));

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
	// 1. 螳壽焚繝舌ャ繝輔ぃ・医き繝｡繝ｩ縺ｮ繝薙Η繝ｼ繝ｻ繝励Ο繧ｸ繧ｧ繧ｯ繧ｷ繝ｧ繝ｳ陦悟・・峨・GPU騾∽ｿ｡
	// ----------------------------------------------------
	if (cameraController)
	{
		D3D11_VIEWPORT viewport{};
		UINT num_viewports{ 1 };
		immediate_context->RSGetViewports(&num_viewports, &viewport);

		float aspect_ratio = viewport.Width / viewport.Height;

		XMMATRIX P = XMMatrixPerspectiveFovLH(XMConvertToRadians(cameraController->get_fov()), aspect_ratio, 0.1f, 300.0f);

		XMVECTOR eye = XMLoadFloat3(&cameraController->get_eye());
		XMVECTOR focus = XMLoadFloat3(&cameraController->get_focus());
		XMVECTOR up = XMLoadFloat3(&cameraController->get_up());
		XMMATRIX V = XMMatrixLookAtLH(eye, focus, up);

		SceneConstants scene_data{};
		XMStoreFloat4x4(&scene_data.view_projection, V * P);
		scene_data.light_direction = XMFLOAT4(0.0f, 1.0f, 1.0f, 0.0f);
		scene_data.camera_position = XMFLOAT4(cameraController->get_eye().x, cameraController->get_eye().y, cameraController->get_eye().z, 1.0f);

		immediate_context->UpdateSubresource(sceneConstantBuffer.Get(), 0, nullptr, &scene_data, 0, 0);
		immediate_context->VSSetConstantBuffers(1, 1, sceneConstantBuffer.GetAddressOf());
		immediate_context->PSSetConstantBuffers(1, 1, sceneConstantBuffer.GetAddressOf());
	}

	// ----------------------------------------------------
	// 2. static_mesh (3D繝｢繝・Ν) 縺ｮ謠冗判
	// ----------------------------------------------------
	if (carMesh && player)
	{
		XMFLOAT4X4 carWorld = player->get_transform();
		XMFLOAT4 materialColor = { 1.0f, 1.0f, 1.0f, 1.0f };

		// static_mesh 縺ｮ謠冗判
		carMesh->render(immediate_context, carWorld, materialColor);
	}

	if (characterMesh)
	{
		characterMesh->render(immediate_context, characterWorld, XMFLOAT4(1, 1, 1, 1));
	}

#ifdef USE_IMGUI
	// 2D HUD (ImGui)
	ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(300, 160), ImGuiCond_Once);

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
	characterMesh.reset();
	sceneConstantBuffer.Reset();
	player.reset();
	cameraController.reset();
}
