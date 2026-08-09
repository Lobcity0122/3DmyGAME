#include "PacmanGameScene.h"
#include <cmath>
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

using namespace DirectX;

bool PacmanGameScene::initialize(ID3D11Device* device)
{
	// 1回のみ生成: CPUオブジェクトを生成し、そのコンストラクタ内でGPUリソースを作成
	player = std::make_unique<PacmanPlayer>();
	player->initialize();
	camera_controller = std::make_unique<CameraController>();
	player_mesh = std::make_unique<static_mesh>(device, L".\\resources\\cube.obj");
	// 描画モデルの実寸から、自機AABBのローカル範囲を取得する。
	XMFLOAT3 player_model_min{}, player_model_max{};
	player_mesh->get_bounding_box(player_model_min, player_model_max);
	player->set_collision_model_bounds(player_model_min, player_model_max);
	stage_mesh = std::make_unique<static_mesh>(device, L".\\resources\\stage\\pac-man_level_namco_nes\\stage.obj");
	background_mesh = std::make_unique<static_mesh>(device, L".\\resources\\skybox_side_chicken_gun\\haikei.obj");

	// 見た目用stage.objとは別に、壁だけを入れたOBJを非表示でロードする。
	collision_mesh = std::make_unique<static_mesh>(device, L".\\resources\\stage\\pac-man_level_namco_nes\\stage_collision.obj");
	debug_cube = std::make_unique<cube>(device);
	configure_object_transforms();

	// 共有の b1 定数バッファを1度だけ生成。中身は毎フレーム更新
	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = sizeof(SceneConstants);
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	if (FAILED(device->CreateBuffer(&buffer_desc, nullptr, scene_constant_buffer.GetAddressOf()))) return false;

	// 方向ライトの影を保存する深度テクスチャ。深度ビューとシェーダー読み込みビューを同じテクスチャから作る。
	if (!create_shadow_map(device, shadow_map_size)) return false;

	D3D11_SAMPLER_DESC shadow_sampler_desc{};
	shadow_sampler_desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	shadow_sampler_desc.AddressU = shadow_sampler_desc.AddressV = shadow_sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	shadow_sampler_desc.BorderColor[0] = shadow_sampler_desc.BorderColor[1] = shadow_sampler_desc.BorderColor[2] = shadow_sampler_desc.BorderColor[3] = 1.0f;
	shadow_sampler_desc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
	if (FAILED(device->CreateSamplerState(&shadow_sampler_desc, shadow_sampler_state.GetAddressOf()))) return false;

	D3D11_RASTERIZER_DESC shadow_rasterizer_desc{};
	shadow_rasterizer_desc.FillMode = D3D11_FILL_SOLID;
	shadow_rasterizer_desc.CullMode = D3D11_CULL_BACK;
	shadow_rasterizer_desc.DepthBias = 1000;
	shadow_rasterizer_desc.SlopeScaledDepthBias = 1.0f;
	shadow_rasterizer_desc.DepthBiasClamp = 0.0f;
	shadow_rasterizer_desc.DepthClipEnable = TRUE;
	if (FAILED(device->CreateRasterizerState(&shadow_rasterizer_desc, shadow_rasterizer_state.GetAddressOf()))) return false;

	// 通常のモデルは背面を省略するが、背景はカプセルの内側から見るので両面を描画する。
	D3D11_RASTERIZER_DESC background_rasterizer_desc{};
	background_rasterizer_desc.FillMode = D3D11_FILL_SOLID;
	background_rasterizer_desc.CullMode = D3D11_CULL_NONE;
	background_rasterizer_desc.DepthClipEnable = TRUE;
	if (FAILED(device->CreateRasterizerState(&background_rasterizer_desc,
		background_rasterizer_state.GetAddressOf()))) return false;

	// 当たり判定モデル確認専用。通常描画では使用せず、必要な時だけ赤い線で重ねる。
	D3D11_RASTERIZER_DESC collision_rasterizer_desc{};
	collision_rasterizer_desc.FillMode = D3D11_FILL_WIREFRAME;
	collision_rasterizer_desc.CullMode = D3D11_CULL_NONE;
	collision_rasterizer_desc.DepthClipEnable = TRUE;
	if (FAILED(device->CreateRasterizerState(&collision_rasterizer_desc,
		collision_wireframe_rasterizer_state.GetAddressOf()))) return false;
	
	// 初期 Transform から描画用ワールド行列を作る。
	update_object_world_matrices();
	total_time = 0.0f;
	return true;
}

void PacmanGameScene::update(float elapsed_time)
{
	// 背景だけをゆっくり回し、宇宙空間が流れているように見せる。
	if (editor_debug.rotate_background)
	{
		background_transform.rotation_degrees.y += editor_debug.background_rotation_speed * elapsed_time;
		if (background_transform.rotation_degrees.y > 360.0f) background_transform.rotation_degrees.y -= 360.0f;
		if (background_transform.rotation_degrees.y < -360.0f) background_transform.rotation_degrees.y += 360.0f;
	}
	// 右クリック中はステージ確認用のフリーカメラを優先し、車の操作と追従カメラを停止する。
	bool mouse_input_allowed = true;
#ifdef USE_IMGUI
	mouse_input_allowed = !ImGui::GetIO().WantCaptureMouse;
#endif
	const bool is_editing_camera = camera_controller->update_editor_camera(
		elapsed_time, GetActiveWindow(), editor_debug.enable_editor_camera, mouse_input_allowed);
	if (!is_editing_camera)
	{
		player->update(elapsed_time, collision_mesh.get(), stage_world);
		camera_controller->update(elapsed_time, player->get_position(), player->get_angle().y,
			player->get_move_speed(), false, 0.0f);
	}
	update_object_world_matrices();
	total_time += elapsed_time;
}

void PacmanGameScene::configure_object_transforms()
{
	// 各モデルの初期配置はこの関数だけで決める。
	// position: ワールド座標、rotation_degrees: X/Y/Z 軸の回転（度）、scale: 拡縮率。
	// 新しいモデルを追加したときも、同じ ObjectTransform を1つ用意してここで設定する。
	stage_transform = {
		{ 0.0f, 0.0f, 0.0f },  // position
		{ 0.0f, 0.0f, 0.0f },  // rotation_degrees
		{ 2.5f, 2.5f, 2.5f }   // scale
	};

	// 背景カプセルは原点を中心に大きく配置。ステージ全体を内部に収めるためのスケール。
	background_transform = {
		// Blenderのグリッド中心とゲームのワールド原点を一致させる。
		{ 0.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 0.0f },
		{ 100.0f, 100.0f, 100.0f }
	};

	// 車は Player が Transform を保持するため、Player の setter で設定する。
	// X=4.013 は左右壁の実測中点。Z=-4.0 の開始通路中央に置く。
	player->set_position({ 10.0f, 1.3f, -13.0f });
	player->set_angle({ XMConvertToRadians(0.0f), XMConvertToRadians(0.0f), XMConvertToRadians(0.0f) });
	// Cubeの実寸は一辺2なので、0.3倍で一辺0.6。
	// 開始地点の通路幅（約0.66?0.70）へ、ほぼ隙間なく収まる大きさにする。
	player->set_scale({ 0.6f, 0.6f, 0.6f });
}

void PacmanGameScene::update_object_world_matrices()
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
	XMStoreFloat4x4(&background_world, make_world_matrix(background_transform));
}

void PacmanGameScene::render(ID3D11DeviceContext* immediate_context, float)
{
	if (requested_shadow_map_size != shadow_map_size)
	{
		Microsoft::WRL::ComPtr<ID3D11Device> device;
		immediate_context->GetDevice(device.GetAddressOf());
		if (create_shadow_map(device.Get(), requested_shadow_map_size))
			shadow_map_size = requested_shadow_map_size;
		else
			requested_shadow_map_size = shadow_map_size;
	}
	// 固定された描画パイプライン順序: 共有カメラ/ライトデータ設定 -> 3Dモデル描画 -> UIコマンド発行
	render_shadow_map(immediate_context);
	update_scene_constants(immediate_context);
	draw_models(immediate_context);
	draw_hud();
}

bool PacmanGameScene::create_shadow_map(ID3D11Device* device, UINT size)
{
	// 一式を生成できてから差し替える。失敗しても現在使用中の影は残る。
	Microsoft::WRL::ComPtr<ID3D11Texture2D> new_texture;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> new_dsv;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> new_srv;
	D3D11_TEXTURE2D_DESC texture_desc{};
	texture_desc.Width = texture_desc.Height = size;
	texture_desc.MipLevels = 1;
	texture_desc.ArraySize = 1;
	texture_desc.Format = DXGI_FORMAT_R32_TYPELESS;
	texture_desc.SampleDesc.Count = 1;
	texture_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	if (FAILED(device->CreateTexture2D(&texture_desc, nullptr, new_texture.GetAddressOf()))) return false;
	D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
	dsv_desc.Format = DXGI_FORMAT_D32_FLOAT;
	dsv_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	if (FAILED(device->CreateDepthStencilView(new_texture.Get(), &dsv_desc, new_dsv.GetAddressOf()))) return false;
	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
	srv_desc.Format = DXGI_FORMAT_R32_FLOAT;
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srv_desc.Texture2D.MipLevels = 1;
	if (FAILED(device->CreateShaderResourceView(new_texture.Get(), &srv_desc, new_srv.GetAddressOf()))) return false;
	shadow_depth_texture = new_texture;
	shadow_depth_stencil_view = new_dsv;
	shadow_shader_resource_view = new_srv;
	return true;
}

XMMATRIX PacmanGameScene::calculate_light_view_projection() const
{
	// 現段階ではステージ全体を覆う1枚のシャドウマップ。後で近景/中景/遠景に分けるCSMへ拡張できる。
	const XMVECTOR target = XMVectorSet(0.0f, 5.0f, 0.0f, 1.0f);
	const XMVECTOR direction = XMVector3Normalize(XMLoadFloat3(&light_settings.direction));
	const XMVECTOR eye = XMVectorSubtract(target, XMVectorScale(direction, 45.0f));
	const XMMATRIX view = XMMatrixLookAtLH(eye, target, XMVectorSet(0, 1, 0, 0));
	const XMMATRIX projection = XMMatrixOrthographicLH(60.0f, 60.0f, 0.1f, 120.0f);
	return view * projection;
}

void PacmanGameScene::render_shadow_map(ID3D11DeviceContext* immediate_context)
{
	if (!light_settings.use_shadows || light_settings.use_point_light) return;

	// メイン画面のレンダーターゲットとビューポートを保存して、影描画後に必ず戻す。
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> previous_rtv;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> previous_dsv;
	immediate_context->OMGetRenderTargets(1, previous_rtv.GetAddressOf(), previous_dsv.GetAddressOf());
	D3D11_VIEWPORT previous_viewport{};
	UINT viewport_count = 1;
	immediate_context->RSGetViewports(&viewport_count, &previous_viewport);
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> previous_rasterizer;
	immediate_context->RSGetState(previous_rasterizer.GetAddressOf());

	D3D11_VIEWPORT shadow_viewport{};
	shadow_viewport.Width = static_cast<float>(shadow_map_size);
	shadow_viewport.Height = static_cast<float>(shadow_map_size);
	shadow_viewport.MinDepth = 0.0f;
	shadow_viewport.MaxDepth = 1.0f;
	immediate_context->OMSetRenderTargets(0, nullptr, shadow_depth_stencil_view.Get());
	immediate_context->RSSetViewports(1, &shadow_viewport);
	immediate_context->RSSetState(shadow_rasterizer_state.Get());
	immediate_context->ClearDepthStencilView(shadow_depth_stencil_view.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

	SceneConstants shadow_constants{};
	XMStoreFloat4x4(&shadow_constants.view_projection, calculate_light_view_projection());
	immediate_context->UpdateSubresource(scene_constant_buffer.Get(), 0, nullptr, &shadow_constants, 0, 0);
	immediate_context->VSSetConstantBuffers(1, 1, scene_constant_buffer.GetAddressOf());

	// 背景は影を落とさない。ステージと車だけを深度として記録する。
	stage_mesh->render(immediate_context, stage_world, XMFLOAT4(1, 1, 1, 1), nullptr, true);
	player_mesh->render(immediate_context, player->get_transform(), XMFLOAT4(1, 1, 1, 1), nullptr, true);

	immediate_context->OMSetRenderTargets(1, previous_rtv.GetAddressOf(), previous_dsv.Get());
	immediate_context->RSSetViewports(1, &previous_viewport);
	immediate_context->RSSetState(previous_rasterizer.Get());
}

void PacmanGameScene::update_scene_constants(ID3D11DeviceContext* immediate_context)
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
		light_settings.unlit_texture_check ? 1.0f : 0.0f,
		light_settings.use_pbr_lighting ? 1.0f : 0.0f, 0.0f);
	XMStoreFloat4x4(&constants.light_view_projection, calculate_light_view_projection());
	constants.shadow_settings = XMFLOAT4(0.0015f,
		(!light_settings.use_point_light && light_settings.use_shadows) ? 1.0f : 0.0f, 0.0f, 0.0f);
	constants.post_process_settings = XMFLOAT4(light_settings.exposure_ev, 0.0f, 0.0f, 0.0f);

	// static_mesh と skinned_mesh の両シェーダーがこの b1 定数バッファを参照する
	immediate_context->UpdateSubresource(scene_constant_buffer.Get(), 0, nullptr, &constants, 0, 0);
	immediate_context->VSSetConstantBuffers(1, 1, scene_constant_buffer.GetAddressOf());
	immediate_context->PSSetConstantBuffers(1, 1, scene_constant_buffer.GetAddressOf());
	immediate_context->PSSetShaderResources(3, 1, shadow_shader_resource_view.GetAddressOf());
	immediate_context->PSSetSamplers(3, 1, shadow_sampler_state.GetAddressOf());
}

void PacmanGameScene::draw_models(ID3D11DeviceContext* immediate_context)
{
	// 各モデルが自身のシェーダー、ジオメトリ、テクスチャ、および b0（オブジェクト固有の定数バッファ）をバインドして描画
	// Draw the course first, then draw the moving car on top of it using the depth buffer.
	stage_mesh->render(immediate_context, stage_world, XMFLOAT4(1, 1, 1, 1));
	player_mesh->render(immediate_context, player->get_transform(), XMFLOAT4(1, 1, 1, 1));
	if (editor_debug.show_collision_model)
	{
		Microsoft::WRL::ComPtr<ID3D11RasterizerState> previous_rasterizer;
		immediate_context->RSGetState(previous_rasterizer.GetAddressOf());
		immediate_context->RSSetState(collision_wireframe_rasterizer_state.Get());
		collision_mesh->render(immediate_context, stage_world, XMFLOAT4(1.0f, 0.05f, 0.05f, 1.0f));
		immediate_context->RSSetState(previous_rasterizer.Get());
	}

	// 背景カプセルは内側から見えるよう、背面カリングを無効にして描画する。
	immediate_context->RSSetState(background_rasterizer_state.Get());
	background_mesh->render(immediate_context, background_world, XMFLOAT4(1, 1, 1, 1));
	draw_editor_helpers(immediate_context);
}

void PacmanGameScene::draw_editor_helpers(ID3D11DeviceContext* immediate_context)
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

void PacmanGameScene::draw_hud()
{
#ifdef USE_IMGUI
	// framework 側が Scene::update の前に ImGui の新規フレームを開始し、Scene::render の後に描画を確定させる
	ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(300, 160), ImGuiCond_FirstUseEver);
	// HUDもほかのデバッグウィンドウと同様に、移動・サイズ変更・折り畳みを許可する。
	ImGui::Begin("Pacman HUD");
	ImGui::Text("MOVE SPEED: %.1f", player->get_move_speed());
	ImGui::Text("AUTO MOVE  A: left  D: right  S: reverse");
	const int minutes = static_cast<int>(total_time) / 60;
	const float seconds = std::fmod(total_time, 60.0f);
	ImGui::Text("TIME: %02d:%05.2f", minutes, seconds);
	ImGui::End();

	// This window edits the values that will be copied to the b1 scene constant buffer next frame.
	ImGui::SetNextWindowPos(ImVec2(20, 200), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(360, 0), ImGuiCond_FirstUseEver);
	ImGui::Begin("Lighting / Texture Debug");
	ImGui::Checkbox("Use point light", &light_settings.use_point_light);
	ImGui::Checkbox("Enable directional shadows", &light_settings.use_shadows);
	const char* shadow_size_labels[] = { "1024 x 1024", "2048 x 2048", "4096 x 4096" };
	int shadow_size_index = shadow_map_size == 1024 ? 0 : shadow_map_size == 4096 ? 2 : 1;
	if (ImGui::Combo("Shadow texture / viewport", &shadow_size_index, shadow_size_labels, IM_ARRAYSIZE(shadow_size_labels)))
	{
		const UINT shadow_sizes[] = { 1024, 2048, 4096 };
		requested_shadow_map_size = shadow_sizes[shadow_size_index];
	}
	ImGui::Text("Active shadow map: %u x %u", shadow_map_size, shadow_map_size);
	ImGui::Checkbox("Use PBR lighting", &light_settings.use_pbr_lighting);
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
	ImGui::SliderFloat("Exposure (EV)", &light_settings.exposure_ev, -4.0f, 4.0f);
	ImGui::Separator();
	ImGui::Checkbox("Unlit texture check", &light_settings.unlit_texture_check);
	ImGui::TextWrapped("Enable this to view texture colors without lighting. White or gray areas may use a dummy texture.");
	ImGui::End();

	// 現在シーンに配置している各モデルの Transform を個別に確認・調整する画面。
	// 車は Player が保持するため、ここで設定してもその後は通常どおり操作入力で動かせる。
	ImGui::SetNextWindowPos(ImVec2(400, 20), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(330, 0), ImGuiCond_FirstUseEver);
	ImGui::Begin("Object Transform Debug");
	const auto edit_transform = [](const char* name, ObjectTransform& transform)
	{
		if (!ImGui::CollapsingHeader(name, ImGuiTreeNodeFlags_DefaultOpen)) return false;
		ImGui::PushID(name);
		const bool changed =
			ImGui::DragFloat3("Position", &transform.position.x, 0.1f) |
			ImGui::DragFloat3("Rotation (degrees)", &transform.rotation_degrees.x, 1.0f) |
			ImGui::DragFloat3("Scale", &transform.scale.x, 0.01f, 0.01f, 100.0f);
		ImGui::PopID();
		return changed;
	};

	edit_transform("Stage", stage_transform);
	edit_transform("Background", background_transform);

	if (ImGui::CollapsingHeader("Pacman Player", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::PushID("PacmanPlayer");
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
		float hover_amplitude = player->get_hover_amplitude();
		if (ImGui::DragFloat("Hover amplitude", &hover_amplitude, 0.005f, 0.0f, 1.0f))
			player->set_hover_amplitude(hover_amplitude);
		float hover_frequency = player->get_hover_frequency();
		if (ImGui::DragFloat("Hover frequency (Hz)", &hover_frequency, 0.05f, 0.0f, 10.0f))
			player->set_hover_frequency(hover_frequency);
		ImGui::PopID();
	}
	ImGui::End();

	ImGui::SetNextWindowPos(ImVec2(400, 430), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);
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
	ImGui::Checkbox("Show collision model (red wireframe)", &editor_debug.show_collision_model);
	ImGui::Checkbox("Rotate background", &editor_debug.rotate_background);
	if (editor_debug.rotate_background)
	{
		ImGui::DragFloat("Background rotation speed (deg/s)", &editor_debug.background_rotation_speed,
			0.1f, -90.0f, 90.0f, "%.2f");
	}
	ImGui::TextWrapped("Grid: XZ plane at world origin. Gizmo: X red, Y green, Z blue.");
	ImGui::End();
#endif
}

void PacmanGameScene::uninitialize()
{
	// framework が管理している Direct3D デバイスが破棄される前に、GPUリソースを使うオブジェクトを解放
	player_mesh.reset();
	stage_mesh.reset();
	collision_mesh.reset();
	background_mesh.reset();
	debug_cube.reset();
	scene_constant_buffer.Reset();
	shadow_rasterizer_state.Reset();
	shadow_sampler_state.Reset();
	shadow_shader_resource_view.Reset();
	shadow_depth_stencil_view.Reset();
	shadow_depth_texture.Reset();
	background_rasterizer_state.Reset();
	collision_wireframe_rasterizer_state.Reset();
	player.reset();
	if (camera_controller) camera_controller->stop_editor_camera();
	camera_controller.reset();
}
