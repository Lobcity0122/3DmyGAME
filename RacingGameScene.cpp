#include "RacingGameScene.h"
#include <cmath>
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

using namespace DirectX;

bool RacingGameScene::initialize(ID3D11Device* device)
{
	// 1回のみ生成: CPUオブジェクトを生成し、そのコンストラクタ内でGPUリソースを作成
	player = std::make_unique<Player>();
	player->initialize();
	camera_controller = std::make_unique<CameraController>();
	car_mesh = std::make_unique<static_mesh>(device, L".\\resources\\car\\123.obj");
	character_mesh = std::make_unique<skinned_mesh>(device, ".\\resources\\cube.000.fbx", true);

	// 共有の b1 定数バッファを1度だけ生成。中身は毎フレーム更新
	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = sizeof(SceneConstants);
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	if (FAILED(device->CreateBuffer(&buffer_desc, nullptr, scene_constant_buffer.GetAddressOf()))) return false;

	// OBJの車とFBXを同時に確認できるように、FBXの位置をずらします。
	XMStoreFloat4x4(&character_world, XMMatrixTranslation(4.0f, 0.0f, 4.0f));
	total_time = 0.0f;
	return true;
}

void RacingGameScene::update(float elapsed_time)
{
	// 最初にゲーム状態を更新。描画処理は後からこの結果（プレイヤーとカメラの状態）を読み込み
	player->update(elapsed_time, nullptr);
	total_time += elapsed_time;
	camera_controller->update(elapsed_time, player->get_position(), player->get_angle().y,
		player->get_current_speed(), player->is_drifting(), player->get_drift_direction());
}

void RacingGameScene::render(ID3D11DeviceContext* immediate_context, float)
{
	// 固定された描画パイプライン順序: 共有カメラ/ライトデータ設定 -> 3Dモデル描画 -> UIコマンド発行
	update_scene_constants(immediate_context);
	draw_models(immediate_context);
	draw_hud();
}

void RacingGameScene::update_scene_constants(ID3D11DeviceContext* immediate_context)
{
	D3D11_VIEWPORT viewport{};
	UINT viewport_count = 1;
	immediate_context->RSGetViewports(&viewport_count, &viewport);
	const float aspect_ratio = viewport.Width / viewport.Height;

	const XMMATRIX projection = XMMatrixPerspectiveFovLH(
		XMConvertToRadians(camera_controller->get_fov()), aspect_ratio, 0.1f, 300.0f);
	const XMMATRIX view = XMMatrixLookAtLH(
		XMLoadFloat3(&camera_controller->get_eye()),
		XMLoadFloat3(&camera_controller->get_focus()),
		XMLoadFloat3(&camera_controller->get_up()));

	SceneConstants constants{};
	XMStoreFloat4x4(&constants.view_projection, view * projection);
	constants.light_direction = XMFLOAT4(0.0f, 1.0f, 1.0f, 0.0f);
	const XMFLOAT3& eye = camera_controller->get_eye();
	constants.camera_position = XMFLOAT4(eye.x, eye.y, eye.z, 1.0f);

	// static_mesh と skinned_mesh の両シェーダーがこの b1 定数バッファを参照する
	immediate_context->UpdateSubresource(scene_constant_buffer.Get(), 0, nullptr, &constants, 0, 0);
	immediate_context->VSSetConstantBuffers(1, 1, scene_constant_buffer.GetAddressOf());
	immediate_context->PSSetConstantBuffers(1, 1, scene_constant_buffer.GetAddressOf());
}

void RacingGameScene::draw_models(ID3D11DeviceContext* immediate_context)
{
	// 各モデルが自身のシェーダー、ジオメトリ、テクスチャ、および b0（オブジェクト固有の定数バッファ）をバインドして描画
	car_mesh->render(immediate_context, player->get_transform(), XMFLOAT4(1, 1, 1, 1));
	character_mesh->render(immediate_context, character_world, XMFLOAT4(1, 1, 1, 1));
}

void RacingGameScene::draw_hud()
{
#ifdef USE_IMGUI
	// framework 側が Scene::update の前に ImGui の新規フレームを開始し、Scene::render の後に描画を確定させる
	ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(300, 160), ImGuiCond_Once);
	const ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
	ImGui::Begin("Racing HUD", nullptr, flags);
	ImGui::Text("SPEED: %.1f km/h", player->get_current_speed() * 3.6f);
	ImGui::ProgressBar(player->get_current_speed() / player->get_top_speed(), ImVec2(-1, 20));
	ImGui::TextColored(player->is_drifting() ? ImVec4(1, 0.5f, 0, 1) : ImVec4(0.5f, 0.5f, 0.5f, 1),
		player->is_drifting() ? "DRIFT!" : "DRIVE");
	const int minutes = static_cast<int>(total_time) / 60;
	const float seconds = std::fmod(total_time, 60.0f);
	ImGui::Text("TIME: %02d:%05.2f", minutes, seconds);
	ImGui::End();
#endif
}

void RacingGameScene::uninitialize()
{
	// framework が管理している Direct3D デバイスが破棄される前に、GPUリソースを使うオブジェクトを解放
	car_mesh.reset();
	character_mesh.reset();
	scene_constant_buffer.Reset();
	player.reset();
	camera_controller.reset();
}
