#include "PacmanGameScene.h"
#include <cmath>
#include <algorithm>
#include <cfloat>
#include <windows.h>
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

using namespace DirectX;

bool PacmanGameScene::initialize(ID3D11Device* device)
{
	game_state = GameState::Playing;
	lives = 3;
	state_timer = 0.0f;
	player_visible = true;
	exit_requested = false;
	score = 0;
	survival_bonus_timer = 0.0f;
	next_scene_type = get_type();
	// メニューでBOOTを押したEnterは、遷移直後にもまだ押されていることがある。
	// 現在の状態を初期値にしておけば、いったんキーを離してから次に押すまで
	// タイトルデモが本編へスキップされない。
	previous_enter_pressed = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;
	previous_escape_pressed = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
	player_circuit_segments.clear();
	circuit_cells.clear();

	player = std::make_unique<PacmanPlayer>();
	player->initialize();
	enemy = std::make_unique<PacmanPlayer>();
	enemy->initialize();
	enemy_second = std::make_unique<PacmanPlayer>();
	enemy_second->initialize();
	camera_controller = std::make_unique<CameraController>();
	player_mesh = std::make_unique<static_mesh>(device, L".\\resources\\cube.obj");

	XMFLOAT3 player_model_min{}, player_model_max{};
	player_mesh->get_bounding_box(player_model_min, player_model_max);
	player->set_collision_model_bounds(player_model_min, player_model_max);
	enemy->set_collision_model_bounds(player_model_min, player_model_max);
	enemy_second->set_collision_model_bounds(player_model_min, player_model_max);
	stage_mesh = std::make_unique<static_mesh>(device, L".\\resources\\stage\\pac-man_level_namco_nes\\stage.obj");
	background_mesh = std::make_unique<static_mesh>(device, L".\\resources\\skybox_side_chicken_gun\\haikei.obj");


	collision_mesh = std::make_unique<static_mesh>(device, L".\\resources\\stage\\pac-man_level_namco_nes\\stage_collision.obj");

	circuit_mesh = std::make_unique<static_mesh>(device, L".\\resources\\stage\\pac-man_level_namco_nes\\stage_circuit.obj");
	debug_cube = std::make_unique<cube>(device);
	configure_object_transforms();


	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = sizeof(SceneConstants);
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	if (FAILED(device->CreateBuffer(&buffer_desc, nullptr, scene_constant_buffer.GetAddressOf()))) return false;


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


	D3D11_RASTERIZER_DESC background_rasterizer_desc{};
	background_rasterizer_desc.FillMode = D3D11_FILL_SOLID;
	background_rasterizer_desc.CullMode = D3D11_CULL_NONE;
	background_rasterizer_desc.DepthClipEnable = TRUE;
	if (FAILED(device->CreateRasterizerState(&background_rasterizer_desc,
		background_rasterizer_state.GetAddressOf()))) return false;


	D3D11_RASTERIZER_DESC collision_rasterizer_desc{};
	collision_rasterizer_desc.FillMode = D3D11_FILL_WIREFRAME;
	collision_rasterizer_desc.CullMode = D3D11_CULL_NONE;
	collision_rasterizer_desc.DepthClipEnable = TRUE;
	if (FAILED(device->CreateRasterizerState(&collision_rasterizer_desc,
		collision_wireframe_rasterizer_state.GetAddressOf()))) return false;
	

	update_object_world_matrices();
	build_circuit_cells();
	total_time = 0.0f;
	return true;
}

void PacmanGameScene::update(float elapsed_time)
{
	// アトラクトモードはEnterで本編へ、Escでゲーム選択画面へ戻れる。
	// 押下した瞬間だけを拾うため、シーン切り替えを連続発生させない。
	if (attract_mode)
	{
		const bool enter_pressed = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;
		const bool escape_pressed = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
		if (enter_pressed && !previous_enter_pressed) next_scene_type = SceneType::PACMAN;
		if (escape_pressed && !previous_escape_pressed) next_scene_type = SceneType::MENU;
		previous_enter_pressed = enter_pressed;
		previous_escape_pressed = escape_pressed;
	}

	if (editor_debug.rotate_background)
	{
		background_transform.rotation_degrees.y += editor_debug.background_rotation_speed * elapsed_time;
		if (background_transform.rotation_degrees.y > 360.0f) background_transform.rotation_degrees.y -= 360.0f;
		if (background_transform.rotation_degrees.y < -360.0f) background_transform.rotation_degrees.y += 360.0f;
	}

	bool mouse_input_allowed = true;
#ifdef USE_IMGUI
	mouse_input_allowed = !ImGui::GetIO().WantCaptureMouse;
#endif
	bool is_editing_camera = false;
	if (game_state == GameState::GameClearFade)
	{
		// クリア演出中はフリーカメラよりも演出用カメラを優先する。
		camera_controller->stop_editor_camera();
	}
	else
	{
		is_editing_camera = camera_controller->update_editor_camera(
			elapsed_time, GetActiveWindow(), editor_debug.enable_editor_camera, mouse_input_allowed);
	}
	if (!is_editing_camera)
	{
		state_timer += elapsed_time;
		if (game_state == GameState::Playing)
		{
			const XMFLOAT3 player_previous_position = player->get_position();
			// デモ中の白い自機も敵と同じ壁回避AIで動かす。
			// 描画・コリジョン・カメラは本編と共通なので、宣伝映像と実際のゲームが一致する。
			if (attract_mode)
				player->update_enemy(elapsed_time, collision_mesh.get(), stage_world);
			else
				player->update(elapsed_time, collision_mesh.get(), stage_world);
			record_player_circuit(player_previous_position, player->get_position());
			if (!attract_mode)
			{
				// A corridor scores only once, no matter how often the player revisits it.
				score += recover_circuit_cells_at(player->get_position()) * 100;
				survival_bonus_timer += elapsed_time;
				while (survival_bonus_timer >= 1.0f)
				{
					score += 10;
					survival_bonus_timer -= 1.0f;
				}
			}
			enemy->update_enemy(elapsed_time, collision_mesh.get(), stage_world,
				&player->get_position(), enemy_chase_range);
			// The orange drone does not target the player's current position. It targets
			// a point ahead of the current heading, so it tends to cut off an escape route.
			const XMFLOAT3& player_position = player->get_position();
			const float player_heading = player->get_angle().y;
			const XMFLOAT3 intercept_target{
				player_position.x + std::sinf(player_heading) * enemy_intercept_distance,
				player_position.y,
				player_position.z + std::cosf(player_heading) * enemy_intercept_distance };
			enemy_second->update_enemy(elapsed_time, collision_mesh.get(), stage_world,
				&intercept_target, enemy_chase_range);
			if (!attract_mode)
			{
				if (is_player_touching_enemy()) begin_respawn_or_game_over();
				else if (!circuit_cells.empty() && get_recovered_circuit_cell_count() == static_cast<int>(circuit_cells.size())) begin_game_clear();
			}
		}
		else if (game_state == GameState::Respawning)
		{

			player_visible = std::fmod(state_timer, 0.18f) < 0.09f;
			if (state_timer >= 1.5f)
			{
				player->set_position(player_spawn_position);
				enemy->set_position(enemy_spawn_position);
				enemy_second->set_position(enemy_second_spawn_position);
				game_state = GameState::Playing;
				state_timer = 0.0f;
				player_visible = true;
			}
		}
		else if ((game_state == GameState::GameOverFade || game_state == GameState::GameClearFade) &&
			state_timer >= 2.8f && !exit_requested)
		{
			// Fade-out ends at a result screen instead of closing the application.
			// The result scene owns the "press Enter to return" step.
			finish_to_result();
		}
		if (game_state == GameState::GameClearFade)
		{
			// ステージの原点ギズモを中心に、迷路全体を上空から見せるクリア用アングル。
			camera_controller->update_cinematic_camera(elapsed_time,
				{ 0.0f, 40.0f, -12.0f }, { 0.0f, 0.0f, 0.0f });
		}
		else
		{
			camera_controller->update(elapsed_time, player->get_position(), player->get_angle().y,
				player->get_move_speed(), false, 0.0f);
		}
	}
	update_object_world_matrices();
	total_time += elapsed_time;
}

void PacmanGameScene::configure_object_transforms()
{



	stage_transform = {
		{ 0.0f, 0.0f, 0.0f },  // position
		{ 0.0f, 0.0f, 0.0f },  // rotation_degrees
		{ 2.5f, 2.5f, 2.5f }   // scale
	};


	background_transform = {

		{ 0.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 0.0f },
		{ 100.0f, 100.0f, 100.0f }
	};



	player_spawn_position = { 10.0f, 1.3f, -13.0f };
	player->set_position(player_spawn_position);
	player->set_angle({ XMConvertToRadians(0.0f), XMConvertToRadians(0.0f), XMConvertToRadians(0.0f) });


	player->set_scale({ 0.6f, 0.6f, 0.6f });


	enemy_spawn_position = { -10.0f, 1.3f, -13.0f };
	enemy->set_position(enemy_spawn_position);
	enemy->set_angle({ 0.0f, 0.0f, 0.0f });
	enemy->set_scale({ 0.6f, 0.6f, 0.6f });

	// A second drone begins on the opposing lower corridor. Its orange color
	// makes it readable without requiring a separate mesh asset.
	enemy_second_spawn_position = { -10.0f, 1.3f, 13.0f };
	enemy_second->set_position(enemy_second_spawn_position);
	enemy_second->set_angle({ 0.0f, DirectX::XM_PI, 0.0f });
	enemy_second->set_scale({ 0.6f, 0.6f, 0.6f });
}

void PacmanGameScene::update_object_world_matrices()
{


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

	render_shadow_map(immediate_context);
	update_scene_constants(immediate_context);
	draw_models(immediate_context);
	draw_hud();
}

bool PacmanGameScene::create_shadow_map(ID3D11Device* device, UINT size)
{

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


	stage_mesh->render(immediate_context, stage_world, XMFLOAT4(1, 1, 1, 1), nullptr, true);
	if (player_visible)
		player_mesh->render(immediate_context, player->get_transform(), XMFLOAT4(1, 1, 1, 1), nullptr, true);
	player_mesh->render(immediate_context, enemy->get_transform(), XMFLOAT4(1, 0.08f, 0.08f, 1), nullptr, true);
	player_mesh->render(immediate_context, enemy_second->get_transform(), XMFLOAT4(1, 0.55f, 0.05f, 1), nullptr, true);

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


	immediate_context->UpdateSubresource(scene_constant_buffer.Get(), 0, nullptr, &constants, 0, 0);
	immediate_context->VSSetConstantBuffers(1, 1, scene_constant_buffer.GetAddressOf());
	immediate_context->PSSetConstantBuffers(1, 1, scene_constant_buffer.GetAddressOf());
	immediate_context->PSSetShaderResources(3, 1, shadow_shader_resource_view.GetAddressOf());
	immediate_context->PSSetSamplers(3, 1, shadow_sampler_state.GetAddressOf());
}

void PacmanGameScene::draw_models(ID3D11DeviceContext* immediate_context)
{

	// Draw the course first, then draw the moving car on top of it using the depth buffer.
	stage_mesh->render(immediate_context, stage_world, XMFLOAT4(1, 1, 1, 1));
	draw_player_circuit(immediate_context);
	if (player_visible)
		player_mesh->render(immediate_context, player->get_transform(), XMFLOAT4(1, 1, 1, 1));
	player_mesh->render(immediate_context, enemy->get_transform(), XMFLOAT4(1, 0.08f, 0.08f, 1));
	player_mesh->render(immediate_context, enemy_second->get_transform(), XMFLOAT4(1, 0.55f, 0.05f, 1));
	if (editor_debug.show_collision_model)
	{
		Microsoft::WRL::ComPtr<ID3D11RasterizerState> previous_rasterizer;
		immediate_context->RSGetState(previous_rasterizer.GetAddressOf());
		immediate_context->RSSetState(collision_wireframe_rasterizer_state.Get());
		collision_mesh->render(immediate_context, stage_world, XMFLOAT4(1.0f, 0.05f, 0.05f, 1.0f));
		immediate_context->RSSetState(previous_rasterizer.Get());
	}


	immediate_context->RSSetState(background_rasterizer_state.Get());
	background_mesh->render(immediate_context, background_world, XMFLOAT4(1, 1, 1, 1));
	draw_editor_helpers(immediate_context);
}

void PacmanGameScene::record_player_circuit(const XMFLOAT3& start, const XMFLOAT3& end)
{
	const float move_x = end.x - start.x;
	const float move_z = end.z - start.z;
	const float length_squared = move_x * move_x + move_z * move_z;
	if (length_squared < 0.0004f) return; // 螢√・蜑阪〒豁｢縺ｾ縺｣縺溘ヵ繝ｬ繝ｼ繝縺ｯ蝗櫁ｷｯ縺ｫ縺励↑縺・・

	CircuitSegment new_segment{ start, end };
	if (!player_circuit_segments.empty())
	{
		CircuitSegment& previous = player_circuit_segments.back();
		const float previous_x = previous.end.x - previous.start.x;
		const float previous_z = previous.end.z - previous.start.z;
		const float previous_length = std::sqrt(previous_x * previous_x + previous_z * previous_z);
		const float new_length = std::sqrt(length_squared);
		const bool connects = std::fabs(previous.end.x - start.x) < 0.001f &&
			std::fabs(previous.end.z - start.z) < 0.001f;
		const float direction_dot = (previous_x * move_x + previous_z * move_z) /
			(std::max)(previous_length * new_length, 0.0001f);
		if (connects && direction_dot > 0.999f)
		{
			previous.end = end;
			return;
		}
	}
	player_circuit_segments.push_back(new_segment);
}

void PacmanGameScene::build_circuit_cells()
{


	if (circuit_mesh == nullptr) return;
	const XMMATRIX world = XMLoadFloat4x4(&stage_world);
	for (const static_mesh::bounding_box& local_box : circuit_mesh->get_object_bounding_boxes())
	{
		CircuitCell cell{};
		cell.minimum = { FLT_MAX, FLT_MAX, FLT_MAX };
		cell.maximum = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
		for (int x = 0; x < 2; ++x)
			for (int y = 0; y < 2; ++y)
				for (int z = 0; z < 2; ++z)
			{
				const XMFLOAT3 local{
					x == 0 ? local_box.minimum.x : local_box.maximum.x,
					y == 0 ? local_box.minimum.y : local_box.maximum.y,
					z == 0 ? local_box.minimum.z : local_box.maximum.z };
				XMFLOAT3 point{};
				XMStoreFloat3(&point, XMVector3TransformCoord(XMLoadFloat3(&local), world));
				cell.minimum.x = (std::min)(cell.minimum.x, point.x);
				cell.minimum.y = (std::min)(cell.minimum.y, point.y);
				cell.minimum.z = (std::min)(cell.minimum.z, point.z);
				cell.maximum.x = (std::max)(cell.maximum.x, point.x);
				cell.maximum.y = (std::max)(cell.maximum.y, point.y);
				cell.maximum.z = (std::max)(cell.maximum.z, point.z);
			}
		circuit_cells.push_back(cell);
	}
}

int PacmanGameScene::recover_circuit_cells_at(const XMFLOAT3& position)
{
	int newly_recovered = 0;
	const XMFLOAT2& player_half_extent = player->get_collision_half_extent();
	const float player_min_x = position.x - player_half_extent.x;
	const float player_max_x = position.x + player_half_extent.x;
	const float player_min_z = position.z - player_half_extent.y;
	const float player_max_z = position.z + player_half_extent.y;
	for (CircuitCell& cell : circuit_cells)
	{
		if (!cell.recovered && player_max_x >= cell.minimum.x && player_min_x <= cell.maximum.x &&
			player_max_z >= cell.minimum.z && player_min_z <= cell.maximum.z)
		{
			cell.recovered = true;
			++newly_recovered;
		}
	}
	return newly_recovered;
}

int PacmanGameScene::get_recovered_circuit_cell_count() const
{
	return static_cast<int>(std::count_if(circuit_cells.begin(), circuit_cells.end(),
		[](const CircuitCell& cell) { return cell.recovered; }));
}

void PacmanGameScene::draw_player_circuit(ID3D11DeviceContext* immediate_context)
{


	const float circuit_height = stage_transform.position.y + 0.2f * stage_transform.scale.y - 0.85f;

	const float circuit_thickness = (std::max)(player->get_scale().x * 3.0f, 1.0f);
	const XMFLOAT4 circuit_color{ 0.05f, 1.0f, 0.16f, 1.0f };
	for (const CircuitSegment& segment : player_circuit_segments)
	{
		const float dx = segment.end.x - segment.start.x;
		const float dz = segment.end.z - segment.start.z;
		const float length = std::sqrt(dx * dx + dz * dz);
		if (length <= 0.001f) continue;
		const float angle_y = std::atan2(dx, dz);
		const XMFLOAT3 center{
			(segment.start.x + segment.end.x) * 0.5f,
			circuit_height,
			(segment.start.z + segment.end.z) * 0.5f };
		XMFLOAT4X4 world{};
		XMStoreFloat4x4(&world,
			XMMatrixScaling(circuit_thickness, circuit_thickness, length + circuit_thickness) *
			XMMatrixRotationY(angle_y) *
			XMMatrixTranslation(center.x, center.y, center.z));
		debug_cube->render(immediate_context, world, circuit_color);
	}
}

void PacmanGameScene::draw_editor_helpers(ID3D11DeviceContext* immediate_context)
{
	if (!editor_debug.show_grid && !editor_debug.show_axis_gizmo) return;


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

		draw_box({ length * 0.5f, thickness, 0.0f }, { length, thickness, thickness }, { 1, 0.15f, 0.15f, 1 });
		draw_box({ 0.0f, length * 0.5f, 0.0f }, { thickness, length, thickness }, { 0.15f, 1, 0.15f, 1 });
		draw_box({ 0.0f, thickness, length * 0.5f }, { thickness, thickness, length }, { 0.2f, 0.4f, 1, 1 });
	}
}

void PacmanGameScene::draw_hud()
{
#ifdef USE_IMGUI
	if (attract_mode)
	{
		// タイトルの文字はImGuiで重ねるだけ。背後の3Dデモは通常の本編描画そのもの。
		ImGui::SetNextWindowPos(ImVec2(32, 28), ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.62f);
		ImGui::Begin("CIRCUIT TRAX - ATTRACT MODE", nullptr,
			ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);
		ImGui::TextColored(ImVec4(0.15f, 1.0f, 0.60f, 1.0f), "CIRCUIT TRAX");
		ImGui::TextUnformatted("RESTORE THE LOST GRID");
		ImGui::Separator();
		ImGui::TextUnformatted("ATTRACT MODE / LIVE DEMO");
		ImGui::TextUnformatted("[ENTER] START GAME     [ESC] BACK TO TERMINAL");
		ImGui::End();
		return;
	}

	ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(300, 160), ImGuiCond_FirstUseEver);

	ImGui::Begin("Pacman HUD");
	ImGui::Text("MOVE SPEED: %.1f", player->get_move_speed());
	ImGui::Text("LIVES: %d / 3", lives);
	ImGui::Text("CIRCUIT: %d / %d", get_recovered_circuit_cell_count(), static_cast<int>(circuit_cells.size()));
	ImGui::Text("SCORE: %d   HI: %d", score, session_high_score);
	ImGui::DragFloat("Enemy chase range", &enemy_chase_range, 0.1f, 0.0f, 40.0f);
	ImGui::DragFloat("Orange intercept distance", &enemy_intercept_distance, 0.1f, 0.0f, 20.0f);
	ImGui::Text("AUTO MOVE  A: left  D: right  S: reverse");
	const int minutes = static_cast<int>(total_time) / 60;
	const float seconds = std::fmod(total_time, 60.0f);
	ImGui::Text("TIME: %02d:%05.2f", minutes, seconds);
	if (game_state == GameState::Playing && ImGui::Button("DEBUG: Clear circuit now"))
	{
		// ボタンでも通常クリアと同じ状態遷移へ入るので、演出の確認に使える。
		for (CircuitCell& cell : circuit_cells) cell.recovered = true;
		begin_game_clear();
	}
	if (ImGui::CollapsingHeader("Follow camera"))
	{
		float range = camera_controller->get_follow_range();
		float height = camera_controller->get_follow_height();
		float focus_height = camera_controller->get_follow_focus_height();
		bool follow_rotation = camera_controller->get_follow_rotation();
		if (ImGui::DragFloat("Camera distance", &range, 0.1f, 2.0f, 20.0f)) camera_controller->set_follow_range(range);
		if (ImGui::DragFloat("Camera height", &height, 0.1f, 0.5f, 20.0f)) camera_controller->set_follow_height(height);
		if (ImGui::DragFloat("Focus height", &focus_height, 0.05f, -5.0f, 10.0f)) camera_controller->set_follow_focus_height(focus_height);
		if (ImGui::Checkbox("Follow player rotation", &follow_rotation)) camera_controller->set_follow_rotation(follow_rotation);
	}
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

	if (game_state == GameState::GameOverFade || game_state == GameState::GameClearFade)
	{
		const bool is_clear = game_state == GameState::GameClearFade;
		const float fade_time = is_clear ? (std::max)(state_timer - 0.8f, 0.0f) : state_timer;
		const float alpha = (std::min)(fade_time / 2.0f, 1.0f);
		ImDrawList* foreground = ImGui::GetOverlayDrawList();
		const ImVec2 screen_size = ImGui::GetIO().DisplaySize;
		foreground->AddRectFilled(ImVec2(0, 0), screen_size, IM_COL32(0, 0, 0, static_cast<int>(alpha * 255.0f)));
		if (is_clear || alpha > 0.35f)
			foreground->AddText(ImVec2(screen_size.x * 0.5f - (is_clear ? 30.0f : 42.0f), screen_size.y * 0.5f),
				is_clear ? IM_COL32(80, 255, 150, 255) : IM_COL32(255, 80, 80, 255),
				is_clear ? "CLEAR!" : "GAME OVER");
	}
#endif
}

bool PacmanGameScene::is_player_touching_enemy() const
{


	const DirectX::XMFLOAT3& player_position = player->get_position();
	const float player_half_size = (std::max)(player->get_scale().x, player->get_scale().z);
	const auto touches = [&player_position, player_half_size](const PacmanPlayer& other)
	{
		const DirectX::XMFLOAT3& other_position = other.get_position();
		const float other_half_size = (std::max)(other.get_scale().x, other.get_scale().z);
		const float limit = player_half_size + other_half_size;
		return std::fabs(player_position.x - other_position.x) < limit &&
			std::fabs(player_position.z - other_position.z) < limit;
	};
	return touches(*enemy) || touches(*enemy_second);
}

void PacmanGameScene::begin_respawn_or_game_over()
{
	--lives;
	state_timer = 0.0f;
	player_visible = false;
	if (lives > 0)
	{
		game_state = GameState::Respawning;
	}
	else
	{
		game_state = GameState::GameOverFade;
	}
}

void PacmanGameScene::begin_game_clear()
{

	game_state = GameState::GameClearFade;
	state_timer = 0.0f;
	player_visible = true;
}

void PacmanGameScene::finish_to_result()
{
	if (exit_requested) return;
	exit_requested = true;
	latest_game_result.cleared = game_state == GameState::GameClearFade;
	latest_game_result.recovered_circuits = get_recovered_circuit_cell_count();
	latest_game_result.total_circuits = static_cast<int>(circuit_cells.size());
	latest_game_result.remaining_lives = lives;
	latest_game_result.elapsed_seconds = total_time;
	if (latest_game_result.cleared)
	{
		// Reaching the objective rewards remaining lives, not merely elapsed time.
		score += 5000 + lives * 1000;
	}
	session_high_score = (std::max)(session_high_score, score);
	latest_game_result.score = score;
	latest_game_result.high_score = session_high_score;
	next_scene_type = SceneType::PACMAN_RESULT;
}

void PacmanGameScene::uninitialize()
{

	player_mesh.reset();
	stage_mesh.reset();
	collision_mesh.reset();
	circuit_mesh.reset();
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
	enemy.reset();
	enemy_second.reset();
	if (camera_controller) camera_controller->stop_editor_camera();
	camera_controller.reset();
}
