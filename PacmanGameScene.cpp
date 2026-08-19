#include "PacmanGameScene.h"
#include "GameSave.h"
#include <cmath>
#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <windows.h>
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

using namespace DirectX;

// =============================================================================
// シーンの寿命管理とフレームごとの更新
// =============================================================================
bool PacmanGameScene::initialize(ID3D11Device* device)
{
	game_state = GameState::Playing;
	lives = 3;
	state_timer = 0.0f;
	player_visible = true;
	exit_requested = false;
	score = 0;
	survival_bonus_timer = 0.0f;
	recovery_chain = 0;
	recovery_chain_time_remaining = 0.0f;
	enemy_near_miss_cooldown = 0.0f;
	enemy_second_near_miss_cooldown = 0.0f;
	near_miss_popup_time = 0.0f;
	near_miss_popup_score = 0;
	damage_flash_time = 0.0f;
	system_alert_level = 0;
	system_alert_popup_time = 0.0f;
	minimap_rotation_angle = 0.0f;
	next_scene_type = get_type();
	// メニューでBOOTを押したEnterは、遷移直後にもまだ押されていることがある。
	// 現在の状態を初期値にしておけば、いったんキーを離してから次に押すまで
	// タイトルデモが本編へスキップされない。
	previous_enter_pressed = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;
	previous_escape_pressed = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
	previous_debug_toggle_pressed = (GetAsyncKeyState(VK_F3) & 0x8000) != 0;
	previous_pause_pressed = (GetAsyncKeyState('P') & 0x8000) != 0;
	paused = false;
	player_circuit_segments.clear();
	circuit_cells.clear();

	const auto create_actor = [] { auto actor = std::make_unique<PacmanPlayer>(); actor->initialize(); return actor; };
	player = create_actor();
	enemy = create_actor();
	enemy_second = create_actor();
	camera_controller = std::make_unique<CameraController>();
	player_mesh = std::make_unique<static_mesh>(device, L".\\resources\\cube.obj");
	hud_font = std::make_unique<sprite>(device, L".\\resources\\fonts\\font0.png");

	XMFLOAT3 player_model_min{}, player_model_max{};
	player_mesh->get_bounding_box(player_model_min, player_model_max);
	for (PacmanPlayer* actor : { player.get(), enemy.get(), enemy_second.get() })
		actor->set_collision_model_bounds(player_model_min, player_model_max);
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
	// タイマー、AI、カメラ、自機を更新する前にポーズを処理する。
	// 画面を隠すだけではなく、ゲーム全体を確実に停止させるためである。
	if (!attract_mode && (game_state == GameState::Playing || game_state == GameState::Respawning))
	{
		const bool pause_pressed = (GetAsyncKeyState('P') & 0x8000) != 0;
		if (pause_pressed && !previous_pause_pressed)
			paused = !paused;
		previous_pause_pressed = pause_pressed;
		if (paused)
		{
			camera_controller->stop_editor_camera();
			return;
		}
	}

	if (!attract_mode)
	{
		enemy_near_miss_cooldown = (std::max)(enemy_near_miss_cooldown - elapsed_time, 0.0f);
		enemy_second_near_miss_cooldown = (std::max)(enemy_second_near_miss_cooldown - elapsed_time, 0.0f);
		near_miss_popup_time = (std::max)(near_miss_popup_time - elapsed_time, 0.0f);
		damage_flash_time = (std::max)(damage_flash_time - elapsed_time, 0.0f);
		system_alert_popup_time = (std::max)(system_alert_popup_time - elapsed_time, 0.0f);
	}
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
	else
	{
		const bool debug_toggle_pressed = (GetAsyncKeyState(VK_F3) & 0x8000) != 0;
		if (debug_toggle_pressed && !previous_debug_toggle_pressed)
			show_development_debug = !show_development_debug;
		previous_debug_toggle_pressed = debug_toggle_pressed;
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
	if (game_state == GameState::GameClearFade || attract_mode)
	{
		// クリア時とタイトルデモは、自機追従・編集カメラではない演出用カメラを使う。
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
			// ワープの入口と出口は見た目上つながっていないため、迷路を横断する
			// 長い緑の回路線が描かれないようにする。
			const bool player_warped = apply_warp_tunnel(*player);
			if (!player_warped)
				record_player_circuit(player_previous_position, player->get_position());
			if (!attract_mode)
			{
				// 新しいセルだけが短いチェイン受付時間を延長する。復旧済み通路は延長しない。
				const int newly_recovered = recover_circuit_cells_at(player->get_position());
				if (newly_recovered > 0)
				{
					recovery_chain += newly_recovered;
					recovery_chain_time_remaining = recovery_chain_window_seconds;
					score += newly_recovered * 100 * get_recovery_chain_multiplier();
				}
				else
				{
					recovery_chain_time_remaining = (std::max)(recovery_chain_time_remaining - elapsed_time, 0.0f);
					if (recovery_chain_time_remaining <= 0.0f) recovery_chain = 0;
				}
				update_system_alert(elapsed_time);
				survival_bonus_timer += elapsed_time;
				while (survival_bonus_timer >= 1.0f)
				{
					score += 10;
					survival_bonus_timer -= 1.0f;
				}
			}
			enemy->update_enemy(elapsed_time, collision_mesh.get(), stage_world,
				&player->get_position(), enemy_chase_range);
			apply_warp_tunnel(*enemy);
			// オレンジの敵は自機の現在位置ではなく、進行方向の少し先を狙う。
			// これにより逃げ道を先回りして塞ぎやすくなる。
			const XMFLOAT3& player_position = player->get_position();
			const float player_heading = player->get_angle().y;
			const XMFLOAT3 intercept_target{
				player_position.x + std::sinf(player_heading) * enemy_intercept_distance,
				player_position.y,
				player_position.z + std::cosf(player_heading) * enemy_intercept_distance };
			enemy_second->update_enemy(elapsed_time, collision_mesh.get(), stage_world,
				&intercept_target, enemy_chase_range);
			apply_warp_tunnel(*enemy_second);
			if (!attract_mode)
			{
				award_near_miss_if_needed(*enemy, enemy_near_miss_cooldown);
				award_near_miss_if_needed(*enemy_second, enemy_second_near_miss_cooldown);
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
			// フェードアウト後はアプリを終了せず、リザルト画面へ遷移する。
			// タイトルへ戻るEnter入力はリザルト画面側で扱う。
			finish_to_result();
		}
		if (game_state == GameState::GameClearFade)
		{
			// ステージの原点ギズモを中心に、迷路全体を上空から見せるクリア用アングル。
			camera_controller->update_cinematic_camera(elapsed_time,
				{ 0.0f, 40.0f, -12.0f }, { 0.0f, 0.0f, 0.0f });
		}
		else if (attract_mode)
		{
			// タイトルデモでは、迷路全体を高い位置からゆっくり周回する。
			// 本編の追従カメラを使わず、別シーンだと分かる見せ方にする。
			const float orbit_angle = total_time * 0.1f;
			const XMFLOAT3 title_eye{
				std::sinf(orbit_angle) * 42.0f,
				27.0f,
				std::cosf(orbit_angle) * 42.0f };
			camera_controller->update_cinematic_camera(elapsed_time, title_eye, { 0.0f, 0.0f, 0.0f });
		}
		else
		{
			camera_controller->update(elapsed_time, player->get_position(), player->get_angle().y,
				player->get_move_speed(), false, 0.0f);
		}
	}
	update_minimap_rotation(elapsed_time);
	update_object_world_matrices();
	total_time += elapsed_time;
}

// =============================================================================
// 初期配置とトランスフォームの変換
// =============================================================================
void PacmanGameScene::configure_object_transforms()
{
	stage_transform = {
		{ 0.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 0.0f },
		{ 2.5f, 2.5f, 2.5f }
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

	// 2体目の敵は反対側の下通路から開始する。別モデルを用意せずとも、
	// オレンジ色にすることで見分けられる。
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
	draw_hud(immediate_context);
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
	const LightSettings render_light = get_render_light_settings();
	const XMVECTOR target = XMVectorSet(0.0f, 5.0f, 0.0f, 1.0f);
	const XMVECTOR direction = XMVector3Normalize(XMLoadFloat3(&render_light.direction));
	const XMVECTOR eye = XMVectorSubtract(target, XMVectorScale(direction, 45.0f));
	const XMMATRIX view = XMMatrixLookAtLH(eye, target, XMVectorSet(0, 1, 0, 0));
	const XMMATRIX projection = XMMatrixOrthographicLH(60.0f, 60.0f, 0.1f, 120.0f);
	return view * projection;
}

PacmanGameScene::LightSettings PacmanGameScene::get_render_light_settings() const
{
	LightSettings render_light = light_settings;
	if (!rotate_light_with_background) return render_light;

	// 背景を回すY軸回転を、方向光の向きとポイントライトの位置へ同じように適用する。
	const XMMATRIX rotation = XMMatrixRotationY(
		XMConvertToRadians(background_transform.rotation_degrees.y));
	XMStoreFloat3(&render_light.direction,
		XMVector3TransformNormal(XMLoadFloat3(&light_settings.direction), rotation));
	XMStoreFloat3(&render_light.position,
		XMVector3TransformCoord(XMLoadFloat3(&light_settings.position), rotation));
	return render_light;
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
	const LightSettings render_light = get_render_light_settings();
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
	constants.light_direction = XMFLOAT4(render_light.direction.x, render_light.direction.y, render_light.direction.z, 0.0f);
	const XMFLOAT3& eye = camera_controller->get_eye();
	constants.camera_position = XMFLOAT4(eye.x, eye.y, eye.z, 1.0f);
	constants.light_position_range = XMFLOAT4(
		render_light.position.x, render_light.position.y, render_light.position.z, render_light.range);
	constants.light_color_intensity = XMFLOAT4(
		render_light.color.x, render_light.color.y, render_light.color.z, render_light.intensity);
	const float alert_pulse = system_alert_level > 0
		? 0.5f + 0.5f * std::sinf(total_time * (3.0f + system_alert_level * 2.0f))
		: 0.0f;
	const float alert_ambient_boost = system_alert_level * 0.05f + alert_pulse * system_alert_level * 0.025f;
	const float alert_exposure_boost = system_alert_level * 0.12f + alert_pulse * system_alert_level * 0.06f;
	constants.ambient_color_intensity = XMFLOAT4(
		render_light.ambient_color.x, render_light.ambient_color.y, render_light.ambient_color.z,
		render_light.ambient_intensity + alert_ambient_boost);
	constants.render_options = XMFLOAT4(
		render_light.use_point_light ? 1.0f : 0.0f,
		render_light.unlit_texture_check ? 1.0f : 0.0f,
		render_light.use_pbr_lighting ? 1.0f : 0.0f, 0.0f);
	XMStoreFloat4x4(&constants.light_view_projection, calculate_light_view_projection());
	constants.shadow_settings = XMFLOAT4(0.0015f,
		(!render_light.use_point_light && render_light.use_shadows) ? 1.0f : 0.0f, 0.0f, 0.0f);
	constants.post_process_settings = XMFLOAT4(render_light.exposure_ev + alert_exposure_boost, 0.0f, 0.0f, 0.0f);

	immediate_context->UpdateSubresource(scene_constant_buffer.Get(), 0, nullptr, &constants, 0, 0);
	immediate_context->VSSetConstantBuffers(1, 1, scene_constant_buffer.GetAddressOf());
	immediate_context->PSSetConstantBuffers(1, 1, scene_constant_buffer.GetAddressOf());
	immediate_context->PSSetShaderResources(3, 1, shadow_shader_resource_view.GetAddressOf());
	immediate_context->PSSetSamplers(3, 1, shadow_sampler_state.GetAddressOf());
}

void PacmanGameScene::draw_models(ID3D11DeviceContext* immediate_context)
{
	// 迷路、復旧済み回路、移動するアクターの順に描画する。
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

// =============================================================================
// 回路復旧ルールとワールド空間デバッグヘルパー
// =============================================================================
void PacmanGameScene::record_player_circuit(const XMFLOAT3& start, const XMFLOAT3& end)
{
	const float move_x = end.x - start.x;
	const float move_z = end.z - start.z;
	const float length_squared = move_x * move_x + move_z * move_z;
	if (length_squared < 0.0004f) return; // ほとんど移動していないフレームは線を追加しない。

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

int PacmanGameScene::get_recovery_chain_multiplier() const
{
	// 遊びやすいアーケード向けの曲線。CHAIN 3でx2になり、以後3セルごとに上がる。
	return (std::min)(1 + recovery_chain / 3, 5);
}

void PacmanGameScene::update_system_alert(float)
{
	if (circuit_cells.empty()) return;
	const float recovery_ratio = static_cast<float>(get_recovered_circuit_cell_count()) /
		static_cast<float>(circuit_cells.size());
	const int next_level = recovery_ratio >= 0.90f ? 2 : recovery_ratio >= 0.50f ? 1 : 0;
	if (next_level > system_alert_level)
	{
		system_alert_level = next_level;
		system_alert_popup_time = 1.80f;
	}
}

void PacmanGameScene::draw_player_circuit(ID3D11DeviceContext* immediate_context)
{
	const float circuit_height = stage_transform.position.y + 0.2f * stage_transform.scale.y - 0.85f;
	const float circuit_thickness = (std::max)(player->get_scale().x * 3.0f, 1.0f);
	const int chain_multiplier = get_recovery_chain_multiplier();
	// 倍率ごとに色を変え、現在のスコア状態をひと目で分かるようにする。
	// x1: 緑 -> x2: シアン -> x3: 紫 -> x4: マゼンタ -> x5: 金／白。
	XMFLOAT3 base_color{ 0.08f, 0.95f, 0.20f };
	if (chain_multiplier == 2) base_color = { 0.05f, 0.90f, 1.00f };
	else if (chain_multiplier == 3) base_color = { 0.48f, 0.20f, 1.00f };
	else if (chain_multiplier == 4) base_color = { 1.00f, 0.08f, 0.72f };
	else if (chain_multiplier >= 5) base_color = { 1.00f, 0.82f, 0.10f };
	const float chain_energy = (std::min)(recovery_chain / 15.0f, 1.0f);
	const float breathing = 0.82f + 0.18f * std::sinf(total_time * (5.0f + chain_energy * 8.0f));
	const XMFLOAT4 circuit_color{
		base_color.x * breathing,
		base_color.y * breathing,
		base_color.z * breathing,
		1.0f };
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

// =============================================================================
// HUDと戦術マップ表示
// =============================================================================
void PacmanGameScene::draw_gameplay_hud(ID3D11DeviceContext* immediate_context)
{
	// 迷路を移動中に必要な情報だけを表示するプレイヤー向けHUD。
	// ImGuiに依存しないため、後から作成したPNGパネルへ置き換えられる。
	const auto text = [this, immediate_context](const char* value, float x, float y, float size,
		float r, float g, float b)
	{
		hud_font->textout(immediate_context, value, x, y, size, size, r, g, b, 1.0f);
	};
	char value[64]{};
	text("CIRCUIT TRAX", 22.0f, 18.0f, 17.0f, 0.20f, 1.0f, 0.65f);
	std::snprintf(value, sizeof(value), "SCORE %06d", score);
	text(value, 22.0f, 45.0f, 14.0f, 1.0f, 0.84f, 0.25f);
	std::snprintf(value, sizeof(value), "HI %06d", session_high_score);
	text(value, 22.0f, 68.0f, 12.0f, 0.72f, 0.85f, 1.0f);
	if (recovery_chain > 0)
	{
		std::snprintf(value, sizeof(value), "CHAIN %02d  x%d", recovery_chain, get_recovery_chain_multiplier());
		text(value, 22.0f, 90.0f, 13.0f, 0.25f, 1.0f, 0.72f);
	}
	if (near_miss_popup_time > 0.0f)
	{
		std::snprintf(value, sizeof(value), "NEAR MISS +%d", near_miss_popup_score);
		const float rise = (near_miss_effect_duration - near_miss_popup_time) * 38.0f;
		text(value, 510.0f, 190.0f - rise, 18.0f, 1.0f, 0.82f, 0.25f);
	}
	if (system_alert_level > 0)
	{
		const bool final_circuit = system_alert_level >= 2;
		text(final_circuit ? "FINAL CIRCUIT" : "SYSTEM ALERT", 530.0f, 24.0f, 16.0f,
			final_circuit ? 1.0f : 1.0f, final_circuit ? 0.22f : 0.72f, final_circuit ? 0.72f : 0.12f);
		if (system_alert_popup_time > 0.0f)
			text(final_circuit ? "FINAL CIRCUIT ACTIVE" : "SYSTEM ALERT ACTIVE", 474.0f, 126.0f, 18.0f,
				1.0f, final_circuit ? 0.22f : 0.72f, final_circuit ? 0.72f : 0.12f);
	}

	const int total = static_cast<int>(circuit_cells.size());
	std::snprintf(value, sizeof(value), "CIRCUIT %02d / %02d", get_recovered_circuit_cell_count(), total);
	text(value, 990.0f, 22.0f, 14.0f, 0.20f, 1.0f, 0.65f);
	std::snprintf(value, sizeof(value), "LIVES %d / 3", lives);
	text(value, 1060.0f, 48.0f, 14.0f, 1.0f, 0.36f, 0.40f);
}

void PacmanGameScene::draw_attract_hud(ID3D11DeviceContext* immediate_context)
{
	const auto text = [this, immediate_context](const char* value, float x, float y, float size,
		float r, float g, float b)
	{
		hud_font->textout(immediate_context, value, x, y, size, size, r, g, b, 1.0f);
	};
	text("CIRCUIT TRAX", 32.0f, 26.0f, 28.0f, 0.20f, 1.0f, 0.65f);
	text("RESTORE THE LOST GRID", 34.0f, 64.0f, 14.0f, 0.78f, 0.90f, 1.0f);
	text("ATTRACT MODE / LIVE DEMO", 34.0f, 90.0f, 12.0f, 0.40f, 0.88f, 0.82f);
	char value[64]{};
	std::snprintf(value, sizeof(value), "HIGH SCORE %06d", session_high_score);
	text(value, 34.0f, 116.0f, 14.0f, 1.0f, 0.82f, 0.20f);
	if (std::fmod(total_time, 1.0f) < 0.72f)
		text("[ENTER] START GAME     [ESC] TERMINAL", 34.0f, 650.0f, 15.0f, 0.20f, 1.0f, 0.65f);
}

void PacmanGameScene::update_minimap_rotation(float elapsed_time)
{
	// ミニマップの画面YはワールドZの反転値を使うため、自機追従回転も逆向きのYawを使う。
	const float target_angle = rotate_minimap_with_player ? -player->get_angle().y : 0.0f;
	// atan2(sin, cos)で、-PIとPIの境界をまたぐ場合も最短方向へ回転する。
	const float delta = std::atan2(std::sinf(target_angle - minimap_rotation_angle),
		std::cosf(target_angle - minimap_rotation_angle));
	const float blend = (std::min)(minimap_rotation_follow_speed * elapsed_time, 1.0f);
	minimap_rotation_angle += delta * blend;
}

void PacmanGameScene::draw_minimap()
{
#ifdef USE_IMGUI
	if (!show_minimap || circuit_cells.empty() || attract_mode) return;

	// 復旧判定と同じセルから意図的に生成している。そのため、見た目用の
	// ステージメッシュが変わっても戦術マップの通路情報は正しく保たれる。
	float min_x = FLT_MAX, max_x = -FLT_MAX, min_z = FLT_MAX, max_z = -FLT_MAX;
	for (const CircuitCell& cell : circuit_cells)
	{
		min_x = (std::min)(min_x, cell.minimum.x);
		max_x = (std::max)(max_x, cell.maximum.x);
		min_z = (std::min)(min_z, cell.minimum.z);
		max_z = (std::max)(max_z, cell.maximum.z);
	}
	const float map_size = 210.0f;
	const float margin = 12.0f;
	const float range = (std::max)((std::max)(max_x - min_x, max_z - min_z), 0.01f);
	const ImVec2 map_origin(1280.0f - map_size - 22.0f, 720.0f - map_size - 24.0f);
	const ImVec2 map_center(map_origin.x + map_size * 0.5f, map_origin.y + map_size * 0.5f);
	const XMFLOAT3& player_position = player->get_position();
	// このステージで画面上方向はワールド+Z。マップの画面Yにはワールド-Zを使う。
	const float map_rotation = rotate_minimap_with_player ? minimap_rotation_angle : 0.0f;
	const float map_cos = std::cosf(map_rotation);
	const float map_sin = std::sinf(map_rotation);
	const auto map_point = [&](float world_x, float world_z)
	{
		if (!rotate_minimap_with_player)
		{
			return ImVec2(
				map_origin.x + margin + (world_x - min_x) / range * (map_size - margin * 2.0f),
				map_origin.y + margin + (max_z - world_z) / range * (map_size - margin * 2.0f));
		}
		const float map_scale = (map_size - margin * 2.0f) / range;
		const float dx = (world_x - player_position.x) * map_scale;
		const float dz = -(world_z - player_position.z) * map_scale;
		return ImVec2(map_center.x + dx * map_cos - dz * map_sin,
			map_center.y + dx * map_sin + dz * map_cos);
	};

	// このプロジェクトのImGuiは古い版で、前面描画リストがOverlayという名前になっている。
	ImDrawList* draw_list = ImGui::GetOverlayDrawList();
	draw_list->AddRectFilled(map_origin, ImVec2(map_origin.x + map_size, map_origin.y + map_size), IM_COL32(4, 10, 24, 220));
	draw_list->AddRect(map_origin, ImVec2(map_origin.x + map_size, map_origin.y + map_size), IM_COL32(55, 230, 190, 220), 0.0f, 0, 2.0f);
	draw_list->PushClipRect(ImVec2(map_origin.x + 2.0f, map_origin.y + 2.0f),
		ImVec2(map_origin.x + map_size - 2.0f, map_origin.y + map_size - 2.0f), true);
	for (const CircuitCell& cell : circuit_cells)
	{
		const ImVec2 top_left = map_point(cell.minimum.x, cell.minimum.z);
		const ImVec2 top_right = map_point(cell.maximum.x, cell.minimum.z);
		const ImVec2 bottom_right = map_point(cell.maximum.x, cell.maximum.z);
		const ImVec2 bottom_left = map_point(cell.minimum.x, cell.maximum.z);
		// ミニマップは安定したナビゲーション補助。回路の復旧色は3D空間だけで使い、
		// マップ上では通行可能な全ルートを同じ見やすさで表示する。
		const ImU32 color = IM_COL32(35, 105, 175, 235);
		draw_list->AddQuadFilled(top_left, top_right, bottom_right, bottom_left, color);
	}

	// 設定済みワープ経路の両端をシアンのマーカーで示す。
	if (warp_tunnel.enabled)
	{
		const ImVec2 left_gate = map_point(warp_tunnel.left_trigger_x, warp_tunnel.center_z);
		const ImVec2 right_gate = map_point(warp_tunnel.right_trigger_x, warp_tunnel.center_z);
		draw_list->AddCircle(left_gate, 4.0f, IM_COL32(40, 235, 255, 255), 8, 1.5f);
		draw_list->AddCircle(right_gate, 4.0f, IM_COL32(40, 235, 255, 255), 8, 1.5f);
	}

	const auto draw_enemy = [&](const PacmanPlayer& actor, ImU32 color)
	{
		const XMFLOAT3& position = actor.get_position();
		draw_list->AddCircleFilled(map_point(position.x, position.z), 4.5f, color, 10);
	};
	draw_enemy(*enemy, IM_COL32(255, 65, 72, 255));
	draw_enemy(*enemy_second, IM_COL32(255, 160, 35, 255));

	const ImVec2 player_point = map_point(player_position.x, player_position.z);
	const float player_angle = player->get_angle().y;
	const ImVec2 unrotated_forward(std::sinf(player_angle), -std::cosf(player_angle));
	const ImVec2 forward(
		unrotated_forward.x * map_cos - unrotated_forward.y * map_sin,
		unrotated_forward.x * map_sin + unrotated_forward.y * map_cos);
	const ImVec2 side(forward.y, -forward.x);
	const ImVec2 triangle[] = {
		ImVec2(player_point.x + forward.x * 7.0f, player_point.y + forward.y * 7.0f),
		ImVec2(player_point.x - forward.x * 5.0f + side.x * 4.5f, player_point.y - forward.y * 5.0f + side.y * 4.5f),
		ImVec2(player_point.x - forward.x * 5.0f - side.x * 4.5f, player_point.y - forward.y * 5.0f - side.y * 4.5f) };
	draw_list->AddTriangleFilled(triangle[0], triangle[1], triangle[2], IM_COL32(255, 255, 255, 255));
	draw_list->PopClipRect();
#endif
}

void PacmanGameScene::draw_hud(ID3D11DeviceContext* immediate_context)
{
	if (attract_mode) draw_attract_hud(immediate_context);
	else
	{
		draw_gameplay_hud(immediate_context);
		draw_minimap();
	}
#ifdef USE_IMGUI
	if (show_development_debug && !attract_mode)
	{

	ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(300, 160), ImGuiCond_FirstUseEver);

	ImGui::Begin("Pacman HUD");
	ImGui::Text("MOVE SPEED: %.1f", player->get_move_speed());
	ImGui::Text("LIVES: %d / 3", lives);
	ImGui::Text("CIRCUIT: %d / %d", get_recovered_circuit_cell_count(), static_cast<int>(circuit_cells.size()));
	ImGui::Text("SCORE: %d   HI: %d", score, session_high_score);
	ImGui::Text("CHAIN: %d  x%d  (%.1fs)", recovery_chain, get_recovery_chain_multiplier(), recovery_chain_time_remaining);
	ImGui::Text("SYSTEM ALERT: %s", system_alert_level == 2 ? "FINAL CIRCUIT" : system_alert_level == 1 ? "ACTIVE" : "NORMAL");
	ImGui::DragFloat("Near miss radius", &near_miss_radius, 0.05f, 1.0f, 5.0f);
	ImGui::DragFloat("Near miss cooldown", &near_miss_cooldown_seconds, 0.05f, 0.2f, 5.0f);
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

	// このウィンドウでは、次フレームにb1シーン定数バッファへコピーする値を編集する。
	ImGui::SetNextWindowPos(ImVec2(20, 200), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(360, 0), ImGuiCond_FirstUseEver);
	ImGui::Begin("Lighting / Texture Debug");
	ImGui::Checkbox("Use point light", &light_settings.use_point_light);
	ImGui::Checkbox("Rotate light with background", &rotate_light_with_background);
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
	ImGui::Checkbox("Show tactical minimap", &show_minimap);
	ImGui::Checkbox("Rotate minimap with player", &rotate_minimap_with_player);
	ImGui::Separator();
	const auto edit_warp_tunnel = [](const char* label, WarpTunnelSettings& tunnel)
	{
		if (!ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen)) return;
		ImGui::PushID(label);
		ImGui::Checkbox("Enabled", &tunnel.enabled);
		if (tunnel.enabled)
		{
			ImGui::DragFloat("Center Z", &tunnel.center_z, 0.05f);
			ImGui::DragFloat("Half width Z", &tunnel.half_width_z, 0.05f, 0.10f, 5.0f);
			ImGui::DragFloat("Left trigger X", &tunnel.left_trigger_x, 0.05f);
			ImGui::DragFloat("Right trigger X", &tunnel.right_trigger_x, 0.05f);
			ImGui::DragFloat("Exit at left X", &tunnel.exit_at_left_x, 0.05f, -30.0f, -0.10f);
			ImGui::DragFloat("Exit at right X", &tunnel.exit_at_right_x, 0.05f, 0.10f, 30.0f);
		}
		ImGui::PopID();
	};
	edit_warp_tunnel("Warp tunnel", warp_tunnel);
	ImGui::Checkbox("Rotate background", &editor_debug.rotate_background);
	if (editor_debug.rotate_background)
	{
		ImGui::DragFloat("Background rotation speed (deg/s)", &editor_debug.background_rotation_speed,
			0.1f, -90.0f, 90.0f, "%.2f");
	}
	ImGui::TextWrapped("Grid: XZ plane at world origin. Gizmo: X red, Y green, Z blue.");
	ImGui::End();
	} // 開発用デバッグUI

	if (paused)
	{
		// 前面描画だけで暗幕を重ねる。ライト定数や3Dの描画状態を変えずに、
		// ゲーム画面全体を暗くできる。
		ImDrawList* foreground = ImGui::GetOverlayDrawList();
		const ImVec2 screen_size = ImGui::GetIO().DisplaySize;
		foreground->AddRectFilled(ImVec2(0.0f, 0.0f), screen_size, IM_COL32(0, 0, 0, 135));

		ImGui::SetNextWindowPos(ImVec2(screen_size.x * 0.5f, screen_size.y * 0.5f),
			ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowBgAlpha(0.94f);
		ImGui::Begin("Pause Menu", nullptr,
			ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
		ImGui::TextColored(ImVec4(0.20f, 1.0f, 0.65f, 1.0f), "PAUSED");
		ImGui::Separator();
		if (ImGui::Button("Restart", ImVec2(210.0f, 0.0f)))
		{
			paused = false;
			next_scene_type = SceneType::PACMAN;
		}
		if (ImGui::Button("Return to title", ImVec2(210.0f, 0.0f)))
		{
			paused = false;
			next_scene_type = SceneType::PACMAN_ATTRACT;
		}
		ImGui::Spacing();
		ImGui::TextDisabled("Press P to resume");
		ImGui::End();
	}

	// プレイへの反応を強めるため、UIとは別に前面へ短い画面演出を重ねる。
	// ニアミスはシアン、被弾は赤いビネットで危険度を即座に伝える。
	const ImVec2 effect_screen_size = ImGui::GetIO().DisplaySize;
	ImDrawList* effect_draw_list = ImGui::GetOverlayDrawList();
	if (near_miss_popup_time > 0.0f)
	{
		const float alpha_ratio = near_miss_popup_time / near_miss_effect_duration;
		effect_draw_list->AddRect(ImVec2(effect_screen_size.x * 0.02f, effect_screen_size.y * 0.02f),
			ImVec2(effect_screen_size.x * 0.98f, effect_screen_size.y * 0.98f),
			IM_COL32(50, 255, 225, static_cast<int>(alpha_ratio * 235.0f)), 0.0f, 0, 7.0f);
	}
	if (damage_flash_time > 0.0f)
	{
		const float alpha_ratio = damage_flash_time / damage_flash_duration;
		const float edge = 70.0f + (1.0f - alpha_ratio) * 80.0f;
		const ImU32 color = IM_COL32(255, 25, 45, static_cast<int>(alpha_ratio * 165.0f));
		effect_draw_list->AddRectFilled(ImVec2(0, 0), ImVec2(effect_screen_size.x, edge), color);
		effect_draw_list->AddRectFilled(ImVec2(0, effect_screen_size.y - edge), effect_screen_size, color);
		effect_draw_list->AddRectFilled(ImVec2(0, 0), ImVec2(edge, effect_screen_size.y), color);
		effect_draw_list->AddRectFilled(ImVec2(effect_screen_size.x - edge, 0), effect_screen_size, color);
	}

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

bool PacmanGameScene::apply_warp_tunnel(PacmanPlayer& actor)
{
	const auto apply_one = [&actor](const WarpTunnelSettings& tunnel)
	{
		if (!tunnel.enabled) return false;
		const XMFLOAT3& current = actor.get_position();
		if (std::fabs(current.z - tunnel.center_z) > tunnel.half_width_z)
			return false;

		XMFLOAT3 warped = current;
		if (current.x >= tunnel.right_trigger_x)
			warped.x = tunnel.exit_at_left_x;
		else if (current.x <= tunnel.left_trigger_x)
			warped.x = tunnel.exit_at_right_x;
		else
			return false;
		actor.set_position(warped);
		return true;
	};
	return apply_one(warp_tunnel);
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

bool PacmanGameScene::award_near_miss_if_needed(const PacmanPlayer& other, float& cooldown)
{
	if (cooldown > 0.0f) return false;
	const XMFLOAT3& player_position = player->get_position();
	const XMFLOAT3& other_position = other.get_position();
	const float dx = player_position.x - other_position.x;
	const float dz = player_position.z - other_position.z;
	const float distance_squared = dx * dx + dz * dz;
	if (distance_squared > near_miss_radius * near_miss_radius) return false;

	// 内側のAABBに入っている場合はニアミスではなく接触であり、得点は与えない。
	const float player_half_size = (std::max)(player->get_scale().x, player->get_scale().z);
	const float other_half_size = (std::max)(other.get_scale().x, other.get_scale().z);
	const float hit_limit = player_half_size + other_half_size;
	if (std::fabs(dx) < hit_limit && std::fabs(dz) < hit_limit) return false;

	near_miss_popup_score = 500 * get_recovery_chain_multiplier();
	score += near_miss_popup_score;
	near_miss_popup_time = near_miss_effect_duration;
	cooldown = near_miss_cooldown_seconds;
	return true;
}

void PacmanGameScene::begin_respawn_or_game_over()
{
	--lives;
	recovery_chain = 0;
	recovery_chain_time_remaining = 0.0f;
	near_miss_popup_time = 0.0f;
	damage_flash_time = damage_flash_duration;
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
		// 目標達成時は経過時間だけでなく、残機にも報酬を与える。
		score += 5000 + lives * 1000;
	}
	session_high_score = (std::max)(session_high_score, score);
	GameSave::save_high_score(session_high_score);
	latest_game_result.score = score;
	latest_game_result.high_score = session_high_score;
	next_scene_type = SceneType::PACMAN_RESULT;
}

void PacmanGameScene::uninitialize()
{
	hud_font.reset();
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
