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
	car_mesh = std::make_unique<static_mesh>(device, L".\\resources\\cube.obj");
	stage_mesh = std::make_unique<static_mesh>(device, L".\\resources\\stage\\pac-man_level_namco_nes\\stage.obj");
	character_mesh = std::make_unique<skinned_mesh>(device, ".\\resources\\cube.000.fbx", true);
	debug_cube = std::make_unique<cube>(device);
	configure_object_transforms();

	// 共有の b1 定数バッファを1度だけ生成。中身は毎フレーム更新
	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = sizeof(SceneConstants);
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	if (FAILED(device->CreateBuffer(&buffer_desc, nullptr, scene_constant_buffer.GetAddressOf()))) return false;
	
	// 初期 Transform から描画用ワールド行列を作る。
	update_object_world_matrices();
	total_time = 0.0f;
	return true;
}

void RacingGameScene::update(float elapsed_time)
{
	// 右クリック中はステージ確認用のフリーカメラを優先し、車の操作と追従カメラを停止する。
	bool mouse_input_allowed = true;
#ifdef USE_IMGUI
	mouse_input_allowed = !ImGui::GetIO().WantCaptureMouse;
#endif
	const bool is_editing_camera = camera_controller->update_editor_camera(
		elapsed_time, GetActiveWindow(), editor_debug.enable_editor_camera, mouse_input_allowed);
	if (!is_editing_camera)
	{
		player->update(elapsed_time, nullptr);
		camera_controller->update(elapsed_time, player->get_position(), player->get_angle().y,
			player->get_current_speed(), player->is_drifting(), player->get_drift_direction());
	}
	update_object_world_matrices();
	total_time += elapsed_time;
}

void RacingGameScene::configure_object_transforms()
{
	// 各モデルの初期配置はこの関数だけで決める。
	// position: ワールド座標、rotation_degrees: X/Y/Z 軸の回転（度）、scale: 拡縮率。
	// 新しいモデルを追加したときも、同じ ObjectTransform を1つ用意してここで設定する。
	stage_transform = {
		{ 0.0f, 0.0f, 0.0f },  // position
		{ 0.0f, 0.0f, 0.0f },  // rotation_degrees
		{ 1.0f, 1.0f, 1.0f }   // scale
	};

	character_transform = {
		{ 4.0f, 0.0f, 4.0f },
		{ 0.0f, 0.0f, 0.0f },
		{ 1.0f, 1.0f, 1.0f }
	};

	// 車は Player が Transform を保持するため、Player の setter で設定する。
	player->set_position({ 0.0f, 0.0f, 0.0f });
	player->set_angle({ XMConvertToRadians(0.0f), XMConvertToRadians(0.0f), XMConvertToRadians(0.0f) });
	player->set_scale({ 1.0f, 1.0f, 1.0f });
}

void RacingGameScene::update_object_world_matrices()
{
	// 行列は「拡縮 → 回転 → 平行移動」の順に掛ける。
	// DirectXMath の回転関数はラジアンなので、UIで保持した度をここで変換する。
	const auto make_world_matrix = [](const ObjectTransform& transform)
	{
		const XMMATRIX scale = XMMatrixScaling(transform.scale.x, transform.scale.y, transform.scale.z);
		const XMMATRIX rotation = XMMatrixRotationRollPitchYaw(
			XMConvertToRadians(transform.rotation_degrees.x),
			XMConvertToRadians(transform.rotation_degrees.y),
			XMConvertToRadians(transform.rotation_degrees.z));
		const XMMATRIX translation = XMMatrixTranslation(
			transform.position.x, transform.position.y, transform.position.z);
		return scale * rotation * translation;
	};

	XMStoreFloat4x4(&stage_world, make_world_matrix(stage_transform));
	XMStoreFloat4x4(&character_world, make_world_matrix(character_transform));
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
	constants.light_direction = XMFLOAT4(light_settings.direction.x, light_settings.direction.y, light_settings.direction.z, 0.0f);
	const XMFLOAT3& eye = camera_controller->get_eye();
	constants.camera_position = XMFLOAT4(eye.x, eye.y, eye.z, 1.0f);
	constants.light_position_range = XMFLOAT4(
		light_settings.position.x, light_settings.position.y, light_settings.position.z, light_settings.range);
	constants.light_color_intensity = XMFLOAT4(
		light_settings.color.x, light_settings.color.y, light_settings.color.z, light_settings.intensity);
	constants.ambient_color_intensity = XMFLOAT4(
		light_settings.ambient_color.x, light_settings.ambient_color.y, light_settings.ambient_color.z,
		light_settings.ambient_intensity);
	constants.render_options = XMFLOAT4(
		light_settings.use_point_light ? 1.0f : 0.0f,
		light_settings.unlit_texture_check ? 1.0f : 0.0f, 0.0f, 0.0f);

	// static_mesh と skinned_mesh の両シェーダーがこの b1 定数バッファを参照する
	immediate_context->UpdateSubresource(scene_constant_buffer.Get(), 0, nullptr, &constants, 0, 0);
	immediate_context->VSSetConstantBuffers(1, 1, scene_constant_buffer.GetAddressOf());
	immediate_context->PSSetConstantBuffers(1, 1, scene_constant_buffer.GetAddressOf());
}

void RacingGameScene::draw_models(ID3D11DeviceContext* immediate_context)
{
	// 各モデルが自身のシェーダー、ジオメトリ、テクスチャ、および b0（オブジェクト固有の定数バッファ）をバインドして描画
	// Draw the course first, then draw the moving car on top of it using the depth buffer.
	stage_mesh->render(immediate_context, stage_world, XMFLOAT4(1, 1, 1, 1));
	car_mesh->render(immediate_context, player->get_transform(), XMFLOAT4(1, 1, 1, 1));
	//character_mesh->render(immediate_context, character_world, XMFLOAT4(1, 1, 1, 1));
	draw_editor_helpers(immediate_context);
}

void RacingGameScene::draw_editor_helpers(ID3D11DeviceContext* immediate_context)
{
	if (!editor_debug.show_grid && !editor_debug.show_axis_gizmo) return;

	// cube は中心原点・一辺1の形状。細長く拡縮して、線のように見せる。
	const auto draw_box = [&](const XMFLOAT3& position, const XMFLOAT3& scale, const XMFLOAT4& color)
	{
		XMFLOAT4X4 world{};
		XMStoreFloat4x4(&world,
			XMMatrixScaling(scale.x, scale.y, scale.z) *
			XMMatrixTranslation(position.x, position.y, position.z));
		debug_cube->render(immediate_context, world, color);
	};

	if (editor_debug.show_grid)
	{
		// 線の本数には上限を設け、誤操作で重くならないようにする。
		const float spacing = (std::max)(editor_debug.grid_spacing, 0.1f);
		const float half_size = (std::max)(editor_debug.grid_half_size, spacing);
		const int line_count = (std::min)(static_cast<int>(half_size / spacing), 10);
		const float length = line_count * spacing * 2.0f;
		const XMFLOAT4 grid_color{ 0.34f, 0.38f, 0.46f, 1.0f };

		for (int i = -line_count; i <= line_count; ++i)
		{
			const float offset = i * spacing;
			draw_box({ 0.0f, 0.01f, offset }, { length, 0.01f, 0.01f }, grid_color);
			draw_box({ offset, 0.01f, 0.0f }, { 0.01f, 0.01f, length }, grid_color);
		}
	}

	if (editor_debug.show_axis_gizmo)
	{
		const float length = (std::max)(editor_debug.axis_length, 0.1f);
		const float thickness = 0.06f;
		// X=赤、Y=緑、Z=青。ワールド原点と各軸の向きが一目で分かる。
		draw_box({ length * 0.5f, thickness, 0.0f }, { length, thickness, thickness }, { 1, 0.15f, 0.15f, 1 });
		draw_box({ 0.0f, length * 0.5f, 0.0f }, { thickness, length, thickness }, { 0.15f, 1, 0.15f, 1 });
		draw_box({ 0.0f, thickness, length * 0.5f }, { thickness, thickness, length }, { 0.2f, 0.4f, 1, 1 });
	}
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

	// This window edits the values that will be copied to the b1 scene constant buffer next frame.
	ImGui::SetNextWindowPos(ImVec2(20, 200), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(360, 0), ImGuiCond_Once);
	ImGui::Begin("Lighting / Texture Debug");
	ImGui::Checkbox("Use point light", &light_settings.use_point_light);
	if (light_settings.use_point_light)
	{
		ImGui::DragFloat3("Light position", &light_settings.position.x, 0.1f);
		ImGui::SliderFloat("Light range", &light_settings.range, 1.0f, 100.0f);
	}
	else
	{
		ImGui::DragFloat3("Light direction", &light_settings.direction.x, 0.05f, -1.0f, 1.0f);
	}
	ImGui::ColorEdit3("Light color", &light_settings.color.x);
	ImGui::SliderFloat("Intensity", &light_settings.intensity, 0.0f, 5.0f);
	ImGui::Separator();
	ImGui::TextUnformatted("Environment light (sky)");
	ImGui::ColorEdit3("Ambient color", &light_settings.ambient_color.x);
	ImGui::SliderFloat("Ambient intensity", &light_settings.ambient_intensity, 0.0f, 1.0f);
	ImGui::Separator();
	ImGui::Checkbox("Unlit texture check", &light_settings.unlit_texture_check);
	ImGui::TextWrapped("Enable this to view texture colors without lighting. White or gray areas may use a dummy texture.");
	ImGui::End();

	// 現在シーンに配置している各モデルの Transform を個別に確認・調整する画面。
	// 車は Player が保持するため、ここで設定してもその後は通常どおり操作入力で動かせる。
	ImGui::SetNextWindowPos(ImVec2(400, 20), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(330, 0), ImGuiCond_Once);
	ImGui::Begin("Object Transform Debug");
	const auto edit_transform = [](const char* name, ObjectTransform& transform)
	{
		if (!ImGui::CollapsingHeader(name, ImGuiTreeNodeFlags_DefaultOpen)) return;
		ImGui::PushID(name);
		ImGui::DragFloat3("Position", &transform.position.x, 0.1f);
		ImGui::DragFloat3("Rotation (degrees)", &transform.rotation_degrees.x, 1.0f);
		ImGui::DragFloat3("Scale", &transform.scale.x, 0.01f, 0.01f, 100.0f);
		ImGui::PopID();
	};

	edit_transform("Stage", stage_transform);
	edit_transform("Character", character_transform);

	if (ImGui::CollapsingHeader("Car / Player", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::PushID("CarPlayer");
		XMFLOAT3 position = player->get_position();
		if (ImGui::DragFloat3("Position", &position.x, 0.1f)) player->set_position(position);

		const XMFLOAT3 angle = player->get_angle();
		XMFLOAT3 rotation_degrees{
			XMConvertToDegrees(angle.x), XMConvertToDegrees(angle.y), XMConvertToDegrees(angle.z) };
		if (ImGui::DragFloat3("Rotation (degrees)", &rotation_degrees.x, 1.0f))
		{
			player->set_angle(XMFLOAT3(
				XMConvertToRadians(rotation_degrees.x),
				XMConvertToRadians(rotation_degrees.y),
				XMConvertToRadians(rotation_degrees.z)));
		}

		XMFLOAT3 scale = player->get_scale();
		if (ImGui::DragFloat3("Scale", &scale.x, 0.01f, 0.01f, 100.0f)) player->set_scale(scale);
		ImGui::PopID();
	}
	ImGui::End();

	ImGui::SetNextWindowPos(ImVec2(400, 430), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_Once);
	ImGui::Begin("Editor Helpers");
	ImGui::Checkbox("Enable RMB editor camera", &editor_debug.enable_editor_camera);
	ImGui::TextUnformatted("Hold RMB: look / WASD: move / Q,E: down,up / Shift: fast");
	ImGui::Separator();
	ImGui::Checkbox("Show grid", &editor_debug.show_grid);
	if (editor_debug.show_grid)
	{
		ImGui::DragFloat("Grid half size", &editor_debug.grid_half_size, 0.5f, 1.0f, 50.0f);
		ImGui::DragFloat("Grid spacing", &editor_debug.grid_spacing, 0.1f, 0.1f, 10.0f);
	}
	ImGui::Checkbox("Show XYZ axis gizmo", &editor_debug.show_axis_gizmo);
	if (editor_debug.show_axis_gizmo)
	{
		ImGui::DragFloat("Axis length", &editor_debug.axis_length, 0.1f, 0.1f, 20.0f);
	}
	ImGui::TextWrapped("Grid: XZ plane at world origin. Gizmo: X red, Y green, Z blue.");
	ImGui::End();
#endif
}

void RacingGameScene::uninitialize()
{
	// framework が管理している Direct3D デバイスが破棄される前に、GPUリソースを使うオブジェクトを解放
	car_mesh.reset();
	stage_mesh.reset();
	character_mesh.reset();
	debug_cube.reset();
	scene_constant_buffer.Reset();
	player.reset();
	if (camera_controller) camera_controller->stop_editor_camera();
	camera_controller.reset();
}
