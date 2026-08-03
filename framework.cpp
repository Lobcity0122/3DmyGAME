#include "framework.h"
#include "shader.h"

using namespace DirectX;

framework::framework(HWND hwnd) : hwnd(hwnd)
{
}

bool framework::initialize()
{
	// デバイス・デバイスコンテキスト・スワップチェーンの作成
	HRESULT hr{ S_OK }; // エラーチェック　{}を使う方が処理が早い

	UINT create_device_flags{ 0 };
#ifdef _DEBUG
	create_device_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif 

	// L11の機能を持った設定をしていく
	D3D_FEATURE_LEVEL feature_levels{ D3D_FEATURE_LEVEL_11_0 }; 

	// 画面の初期設定をする
	DXGI_SWAP_CHAIN_DESC swap_chain_desc{};
	swap_chain_desc.BufferCount = 1;
	swap_chain_desc.BufferDesc.Width = SCREEN_WIDTH;   // 幅
	swap_chain_desc.BufferDesc.Height = SCREEN_HEIGHT; // 高さ
	swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 色指定のフォーマットの型
	swap_chain_desc.BufferDesc.RefreshRate.Numerator = 60; // フレームレート60FPS
	swap_chain_desc.BufferDesc.RefreshRate.Denominator = 1;
	swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain_desc.OutputWindow = hwnd;
	swap_chain_desc.SampleDesc.Count = 1;
	swap_chain_desc.SampleDesc.Quality = 0;
	swap_chain_desc.Windowed = !FULLSCREEN; // ！でスクリーンモード
	hr = D3D11CreateDeviceAndSwapChain(
		NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, create_device_flags,
		&feature_levels, 1, D3D11_SDK_VERSION, &swap_chain_desc,
		swap_chain.GetAddressOf(), device.GetAddressOf(), NULL, immediate_context.GetAddressOf()); // NULLから全て戻り値
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr)); // ここまでのエラーチェックの文

	// サンプラーステートオブジェクトを生成
	// どのようにテクスチャの色をサンプルするかの設定など
	D3D11_SAMPLER_DESC sampler_desc;
	sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.MipLODBias = 0;
	sampler_desc.MaxAnisotropy = 16;
	sampler_desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	sampler_desc.BorderColor[0] = 0;
	sampler_desc.BorderColor[1] = 0;
	sampler_desc.BorderColor[2] = 0;
	sampler_desc.BorderColor[3] = 0;
	sampler_desc.MinLOD = 0;
	sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
	hr = device->CreateSamplerState(&sampler_desc, sampler_states[0].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	hr = device->CreateSamplerState(&sampler_desc, sampler_states[1].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	sampler_desc.Filter = D3D11_FILTER_ANISOTROPIC;
	hr = device->CreateSamplerState(&sampler_desc, sampler_states[2].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	// レンダーターゲットビューの作成
	ComPtr<ID3D11Texture2D> back_buffer{};
	hr = swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<LPVOID*>(back_buffer.GetAddressOf()));
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	hr = device->CreateRenderTargetView(back_buffer.Get(), NULL, render_target_view.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	// 深度ステンシルビュー用のテクスチャの作成
	ComPtr<ID3D11Texture2D> depth_stencil_buffer{};
	D3D11_TEXTURE2D_DESC texture2d_desc{};
	texture2d_desc.Width = SCREEN_WIDTH;
	texture2d_desc.Height = SCREEN_HEIGHT;
	texture2d_desc.MipLevels = 1;
	texture2d_desc.ArraySize = 1;
	texture2d_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	texture2d_desc.SampleDesc.Count = 1;
	texture2d_desc.SampleDesc.Quality = 0;
	texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
	texture2d_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	texture2d_desc.CPUAccessFlags = 0;
	texture2d_desc.MiscFlags = 0;
	hr = device->CreateTexture2D(&texture2d_desc, NULL, depth_stencil_buffer.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{};
	depth_stencil_view_desc.Format = texture2d_desc.Format;
	depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depth_stencil_view_desc.Texture2D.MipSlice = 0;
	hr = device->CreateDepthStencilView(depth_stencil_buffer.Get(), &depth_stencil_view_desc, depth_stencil_view.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr)); 

	// ブレンディングステートオブジェクトを作成する 
	// Result = (Source * SrcBlend})BlendOp(Destination * DestBlend)に当てはめる
	
	// ブレンド・無効
	D3D11_BLEND_DESC blend_desc{};
	blend_desc.AlphaToCoverageEnable = FALSE; // アンチエイリアス
	blend_desc.IndependentBlendEnable = FALSE;
	blend_desc.RenderTarget[0].BlendEnable = TRUE;

	// 役割「描く色 (src) に掛けるもの（係数）」を指定
	blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA; // src.a
	// 役割「背景の色 (dst) に掛けるもの（係数）」を指定
	blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA; // 1-src.a
	// 役割「計算記号（演算子）」を指定
	blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD; // どのように合成するかの演算子を決める

	blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE; // 1
	blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO; // 0 
	blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD; // +

	// ALLですべての成分を書き込む
	blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	hr = device->CreateBlendState(&blend_desc, blend_states[0].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	// 加算
	blend_desc.AlphaToCoverageEnable = FALSE;
	blend_desc.IndependentBlendEnable = FALSE;
	blend_desc.RenderTarget[0].BlendEnable = TRUE;
	blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
	blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = device->CreateBlendState(&blend_desc, blend_states[1].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	// 減算
	blend_desc.AlphaToCoverageEnable = FALSE;
	blend_desc.IndependentBlendEnable = FALSE;
	blend_desc.RenderTarget[0].BlendEnable = TRUE;
	blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
	blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_REV_SUBTRACT;
	blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = device->CreateBlendState(&blend_desc, blend_states[2].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	// 乗算
	blend_desc.AlphaToCoverageEnable = FALSE;
	blend_desc.IndependentBlendEnable = FALSE;
	blend_desc.RenderTarget[0].BlendEnable = TRUE;
	blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_DEST_COLOR;
	blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
	blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = device->CreateBlendState(&blend_desc, blend_states[3].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	// ラスタライザステートオブジェクトを生成
	D3D11_RASTERIZER_DESC rasterizer_desc{};
	rasterizer_desc.FillMode = D3D11_FILL_SOLID;
	rasterizer_desc.CullMode = D3D11_CULL_BACK;
	rasterizer_desc.FrontCounterClockwise = TRUE;
	rasterizer_desc.DepthBias = 0;
	rasterizer_desc.DepthBiasClamp = 0;
	rasterizer_desc.SlopeScaledDepthBias = 0;
	rasterizer_desc.DepthClipEnable = TRUE;
	rasterizer_desc.ScissorEnable = FALSE;
	rasterizer_desc.MultisampleEnable = FALSE;
	rasterizer_desc.AntialiasedLineEnable = FALSE;
	hr = device->CreateRasterizerState(&rasterizer_desc, rasterizer_states[0].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	// 1:ワイヤーフレーム描画（表面のみ描画）
	rasterizer_desc.FillMode = D3D11_FILL_WIREFRAME;
	rasterizer_desc.CullMode = D3D11_CULL_BACK;
	rasterizer_desc.AntialiasedLineEnable = TRUE;
	hr = device->CreateRasterizerState(&rasterizer_desc, rasterizer_states[1].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	// 3:ワイヤーフレーム描画（裏面は描画しない）
	rasterizer_desc.FillMode = D3D11_FILL_WIREFRAME;
	rasterizer_desc.CullMode = D3D11_CULL_NONE;
	rasterizer_desc.AntialiasedLineEnable = TRUE;
	hr = device->CreateRasterizerState(&rasterizer_desc, rasterizer_states[2].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	// 4:ワイヤーフレーム描画（裏面も描画）
	rasterizer_desc.FillMode = D3D11_FILL_SOLID;
	rasterizer_desc.CullMode = D3D11_CULL_NONE;
	rasterizer_desc.AntialiasedLineEnable = TRUE;
	hr = device->CreateRasterizerState(&rasterizer_desc, rasterizer_states[3].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	// 5:通常の描画
	rasterizer_desc.FillMode = D3D11_FILL_SOLID;
	rasterizer_desc.CullMode = D3D11_CULL_BACK;
	rasterizer_desc.FrontCounterClockwise = FALSE;
	hr = device->CreateRasterizerState(&rasterizer_desc, rasterizer_states[4].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	// ビューポートの設定
	D3D11_VIEWPORT viewport{};
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = static_cast<float>(SCREEN_WIDTH);
	viewport.Height = static_cast<float>(SCREEN_HEIGHT);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	immediate_context->RSSetViewports(1, &viewport);

	// 0:深度テストON 深度ライトON
	D3D11_DEPTH_STENCIL_DESC depth_stencil_desc{}; // 初期化
	depth_stencil_desc.DepthEnable = TRUE;                           // 深度テスト ON
	depth_stencil_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;  // 深度ライト ON
	depth_stencil_desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	hr = device->CreateDepthStencilState(&depth_stencil_desc, depth_stencil_states[0].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	// 1:深度テストON 深度ライトOFF
	depth_stencil_desc.DepthEnable = TRUE;                           // 深度テスト ON
	depth_stencil_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; // 深度ライト OFF
	depth_stencil_desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	hr = device->CreateDepthStencilState(&depth_stencil_desc, depth_stencil_states[1].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	// 2:深度テストOFF 深度ライトON
	depth_stencil_desc.DepthEnable = FALSE;                          // 深度テスト OFF
	depth_stencil_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;  // 深度ライト ON
	depth_stencil_desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	hr = device->CreateDepthStencilState(&depth_stencil_desc, depth_stencil_states[2].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	// 3:深度テストOFF 深度ライトOFF
	depth_stencil_desc.DepthEnable = FALSE;                          // 深度テスト OFF
	depth_stencil_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; // 深度ライト OFF
	depth_stencil_desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	hr = device->CreateDepthStencilState(&depth_stencil_desc, depth_stencil_states[3].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	// シーン定数バッファオブジェクトの生成
	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = sizeof(scene_constans);
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D10_BIND_CONSTANT_BUFFER;
	buffer_desc.CPUAccessFlags = 0;
	buffer_desc.StructureByteStride = 0;
	hr = device->CreateBuffer(&buffer_desc, nullptr, constant_buffers[0].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	// 画像
	sprites[0] = make_unique<sprite>(device.Get(), L".\\resources\\cyberpunk.jpg");
	sprites[1] = make_unique<sprite>(device.Get(), L".\\resources\\player-sprites.png");
	sprite_batches[0] = make_unique<sprite_batch>(device.Get(), L".\\resources\\player-sprites.png", 2048);
	sprites[2] = make_unique<sprite>(device.Get(), L".\\resources\\fonts\\font2.png");

	// 切り替え用のピクセルシェーダーを作成
	create_ps_from_cso(device.Get(), "effect_ps.cso", replaced_pixel_shaders[0].GetAddressOf());

	// 幾何プリミティブ
	//geometric_primitives[0] = make_unique<geometric_primitive>(device.Get());
    //geometric_primitives[1] = make_unique<geometric_primitive>(device.Get());
	geometric_primitives[0] = make_unique<cube>(device.Get());

	// static_meshオブジェクトを生成する
	// static_meshes[0] = make_unique<static_mesh>(device.Get(), L".\\resources\\Rock\\Rock.obj"); // cube、torus、\\Cup\\cup.obj、\\Bison\\Bison.obj、\\Mr.Incredible\\Mr.Incredible.obj
	// \\Mr.Incredible\\Mr.Incredible.obj

	// skinned_meshオブジェクトを生成する
	skinned_meshes[0] = make_unique<skinned_mesh>(device.Get(), ".\\resources\\cube.003.1.fbx",true); // \\cube.000.fbx

	return true;
}

void framework::update(float elapsed_time/*Elapsed seconds from last frame*/)
{
#ifdef USE_IMGUI
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
#endif

#ifdef USE_IMGUI
	ImGui::Begin("ImGUI");

	// 1. パイプライン・ステート設定（コンボボックス化）
	if (ImGui::CollapsingHeader("Global States", ImGuiTreeNodeFlags_DefaultOpen))
	{
		// テクスチャフィルタ
		const char* sampler_names[] = { "Point", "Linear", "Anisotropic" };
		ImGui::Combo("Texture Filter", &Sampler_index, sampler_names, IM_ARRAYSIZE(sampler_names));
		// ブレンドモード
		const char* blend_names[] = { "None / Opaque", "Alpha Blend", "Additive", "Multiplicative" };
		ImGui::Combo("BLEND Mode", &Blend_index, blend_names, IM_ARRAYSIZE(blend_names));
		// 深度ステンシルステート
		// 0: Test_ON/Write_ON, 1: Test_ON/Write_OFF, 2: Test_OFF/Write_ON, 3: Test_OFF/Write_OFF
		const char* depth_names[] = { "Test:ON / Write:ON", "Test:ON / Write:OFF", "Test:OFF / Write:ON", "Test:OFF / Write:OFF" };
		if (ImGui::Combo("Depth Stencil", &Depth_index, depth_names, IM_ARRAYSIZE(depth_names))){}
		// ラスタライザステートの切り替え
		const char* rasterizer_names[] = { "Fill","Wireframe","Wireframe(No culling)","4" };
		ImGui::Combo("Rasterizer_states", &Rasterizer_index, rasterizer_names, IM_ARRAYSIZE(rasterizer_names));
		ImGui::Separator();
	}

	// 2. スプライトオブジェクトの操作（カラーピッカーと回転の追加）
	if (ImGui::CollapsingHeader("Sprite 0 (Cyberpunk)", 0))
	{
		ImGui::DragFloat2("Sp0: Position", &position0.x, 1.0f);
		ImGui::SliderFloat("Sp0: Angle", &sprite0_angle, 0.0f, 360.0f);
		// ColorEdit4 を使うと、Slider4つ並べるより遥かに楽にRGBAを変更できる
		ImGui::ColorEdit4("Sp0: Color", &sprite0_color.x);
		ImGui::Separator();
	}

	if (ImGui::CollapsingHeader("Sprite 1 (Character)", 0))
	{
		ImGui::DragFloat2("Sp1: Position", &position1.x, 1.0f);
		ImGui::SliderFloat("Sp1: Angle", &sprite1_angle, 0.0f, 360.0f);
		ImGui::ColorEdit4("Sp1: Color", &sprite1_color.x);

		ImGui::Text("Source Rectangle (Texels)");
		ImGui::SliderInt("Animation No", &animationNo, 0, 13);
		ImGui::SliderFloat("Y (sy)", &src_y, 0.0f, 960.0f);
		ImGui::Separator();
	}

	// 3. 文字描画（textout）の調整
	if (ImGui::CollapsingHeader("Textout Control", 0))
	{
		// リアルタイムに文字を打ち替えるバッファ
		ImGui::InputText("String", text_buffer, IM_ARRAYSIZE(text_buffer));
		ImGui::DragFloat2("Text: Position", &text_pos.x, 1.0f);
		ImGui::DragFloat2("Text: Char Size", &text_size.x, 0.5f, 1.0f, 128.0f);
		ImGui::ColorEdit4("Text: Color", &text_color.x);
	}

	// 4. パフォーマンステスト（通常描画とバッチ描画の切り替え）
	if (ImGui::CollapsingHeader("Performance Test", ImGuiTreeNodeFlags_DefaultOpen))
	{
		// チェックボックスで通常スプライトとバッチ描画を切り替える
		ImGui::Checkbox("Use Sprite Batch", &use_batch);
		// バッチの最大数を調整
		ImGui::InputInt("Draw Count", &sprite_draw_count, 1, 1092);
		if (sprite_draw_count < 1)    sprite_draw_count = 1;
		if (sprite_draw_count > 1092) sprite_draw_count = 1092;
		// FPS表示
		ImGui::Text("%.3f ms/frame (%.1f FPS)",
			1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate
		);
		ImGui::Separator();
	}

	// 5. カメラ
	if (ImGui::CollapsingHeader("Camera", 0))
	{
		// カメラ位置の調整
		ImGui::SliderFloat3("Camera Position", &camera_position.x, -10.0f, 10.0f);
		// ライト照射方向の調整（-1.0f ～ 1.0f の範囲）
		ImGui::SliderFloat3("Light Direction", &light_direction.x, -1.0f, 1.0f);
	}

	// 6. 幾何プリミティブ編集
	if (ImGui::CollapsingHeader("Geometric primitive", 0))
	{
		// 位置の調整
		ImGui::DragFloat3("Position", &cube_rotation.x, 0.1f);
		// 姿勢の調整（-180度～180度）0.5fはドラッグのスピード
		ImGui::DragFloat3("Rotation", &cube_rotation.x, 0.5f, -180.0f, 180.0f);
		// 寸法の調整（0.1倍～5.0倍）
		ImGui::SliderFloat3("Scale", &cube_scale.x, 0.1f, 5.0f);
		// 色の調整（カラーピッカー）
		ImGui::ColorEdit4("Material Color", cube_color);
	}

	// 7. static_meshオブジェクト編集
	if (ImGui::CollapsingHeader("Static_mesh", 0))
	{
		// 位置の調整
		ImGui::DragFloat3("mesh_Pos", &static_mesh_position.x, 0.1f);
		// 姿勢の調整（-180度～180度）0.5fはドラッグのスピード
		ImGui::DragFloat3("mesh_Rota", &static_mesh_rotation.x, 0.5f, -180.0f, 180.0f);
		// 寸法の調整（0.1倍～5.0倍）
		ImGui::SliderFloat3("mesh_Scale", &static_mesh_scale.x, 0.1f, 5.0f);
		// 色の調整（カラーピッカー）
		ImGui::ColorEdit4("mesh Color", static_mesh_color);
	}

	// 8. skinned_meshオブジェクト編集
	if (ImGui::CollapsingHeader("Skinned_mesh", ImGuiTreeNodeFlags_DefaultOpen))
	{
		// 位置の調整
		ImGui::DragFloat3("skinned_Pos", &skinned_mesh_position.x, 0.1f);
		// 姿勢の調整（-180度～180度）0.5fはドラッグのスピード
		ImGui::DragFloat3("skinned_Rota", &skinned_mesh_rotation.x, 0.5f, -360.0f, 360.0f);
		// 寸法の調整（0.1倍～5.0倍）
		ImGui::SliderFloat3("skinned_Scale", &skinned_mesh_scale.x, 0.1f, 5.0f);
		// 色の調整（カラーピッカー）
		ImGui::ColorEdit4("skinned Color", skinned_mesh_color);
	}
	
	ImGui::End();
#endif
}

void framework::render(float elapsed_time/*Elapsed seconds from last frame*/)
{
	// これより上には書かない
	HRESULT hr{ S_OK };

	FLOAT color[]{ 0.2f,0.2f,0.2f,1.0f }; // 画面をクリアする
	immediate_context->ClearRenderTargetView(render_target_view.Get(), color);
	immediate_context->ClearDepthStencilView(depth_stencil_view.Get(),
		D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	immediate_context->OMSetRenderTargets(1, render_target_view.GetAddressOf(), depth_stencil_view.Get());

	// framework クラスの render メンバ関数でサンプラーステートオブジェクトをバインドする
	// ImGuiで選択された番号(sampler_index)のサンプラーを、スロット0にバインドする
	immediate_context->PSSetSamplers(0, 1, sampler_states[Sampler_index].GetAddressOf());

	// ここからゲームの背景の描画(つまり2Dの描画)を行っている
	// なので2Dの深度の設定を行う
	// spriteオブジェクト描画の直前で深度ステンシルステートオブジェクトを設定する(初期値 Test:OFF/Write:OFF)
	immediate_context->OMSetDepthStencilState(depth_stencil_states[Depth_index].Get(), 0);

	// spriteオブジェクト描画の直前でブレンディングステートオブジェクトを設定する
	immediate_context->OMSetBlendState(blend_states[Blend_index].Get(), nullptr, 0xFFFFFFFF);

	// ラスタライザステートの切り替え(2D)
	immediate_context->RSSetState(rasterizer_states[5].Get());

	// renderメンバ関数でのspriteオブジェクトの描画方法を変更する
	/*sprites[0]->render(immediate_context.Get(), 
		position0.x, position0.y, 1280, 720,
		sprite0_color.x, sprite0_color.y, 
		sprite0_color.z, sprite0_color.w, 
		sprite0_angle
	);*/
	/*sprites[1]->render(immediate_context.Get(), 
		position1.x, position1.y, 240, 240,
		sprite1_color.x, sprite1_color.y, 
		sprite1_color.z, sprite1_color.w, 
		sprite1_angle,
		140.0f * animationNo, src_y, 140.0f, 240.0f
	);*/
	
	float x{ 0 };
	float y{ 0 };

	if (!use_batch)
	{
		// 個数分ドローコールが発生して重い
		/*for (int i = 0; i < sprite_draw_count; ++i)
		{
			sprites[1]->render(immediate_context.Get(),
				x, static_cast<float>(static_cast<int>(y) % 720), 64, 64,
				1, 1, 1, 1, 0, 140 * 0, 240 * 0, 140, 240);

			x += 32;
			if (x > 1280 - 64)
			{
				x = 0;
				y += 24;
			}
		}*/
	}
	else
	{
		// すべての頂点をまとめて1回で描くので軽量
		/*sprite_batches[0]->begin(immediate_context.Get(), replaced_pixel_shaders[0].Get());

		for (int i = 0; i < sprite_draw_count; ++i)
		{
			sprite_batches[0]->render(immediate_context.Get(),
				x, static_cast<float>(static_cast<int>(y) % 720), 64, 64,
				1, 1, 1, 1, 0, 140 * 0, 240 * 0, 140, 240);

			x += 32;
			if (x > 1280 - 64)
			{
				x = 0;
				y += 24;
			}
		}

		sprite_batches[0]->end(immediate_context.Get());*/
	}

	sprites[2]->textout(immediate_context.Get(), 
		text_buffer, text_pos.x, text_pos.y,
		text_size.x, text_size.y,
		text_color.x, text_color.y, 
		text_color.z, text_color.w
	);

	// ここからモデルの描画を行う(つまり3Dの描画)
	// なので3Dの深度の設定を行う
	D3D11_VIEWPORT viewport;
	UINT num_viewports{ 1 };
	immediate_context->RSGetViewports(&num_viewports, &viewport);

	float aspect_ratio{ viewport.Width / viewport.Height };
	XMMATRIX P{ XMMatrixPerspectiveFovLH(XMConvertToRadians(30), aspect_ratio, 0.1f, 100.0f) };

	// ImGuiの変数からカメラ位置を設定する
	XMVECTOR eye{ XMVectorSet(camera_position.x, camera_position.y, camera_position.z, 1.0f) };
	XMVECTOR focus{ XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f) }; // 注視点
	XMVECTOR up{ XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f) };    // 上方向ベクトル
	XMMATRIX V{ XMMatrixLookAtLH(eye, focus, up) };

	// 定数バッファにビュー・プロジェクション行列とライトの向きを転送
	scene_constans data{};
	XMStoreFloat4x4(&data.view_projection, V * P); // ビュー・プロジェクション行列の合成

	// ImGuiの変数からライト方向を設定する
	data.light_direction = light_direction;
	data.camera_position = camera_position;
	immediate_context->UpdateSubresource(constant_buffers[0].Get(), 0, 0, &data, 0, 0);
	immediate_context->VSSetConstantBuffers(1, 1, constant_buffers[0].GetAddressOf());
	immediate_context->PSSetConstantBuffers(1, 1, constant_buffers[0].GetAddressOf());

	// 座標系変換行列の配列を定義する
	const DirectX::XMFLOAT4X4 coordinate_system_transforms[]
	{
		{
		    -1,0,0,0,0,
		     1,0,0,0,0,
		     1,0,0,0,0,
		     1
		}, // 左手座標系（DirectXのデフォルト）

		{
			 1,0,0,0,0,
			 1,0,0,0,0,
			 1,0,0,0,0,
			 1
		}, // 右手座標系（OpenGLのデフォルト）

		{
			-1,0,0,0,0,
			 0,-1,0,0,1,
			 0,0,0,0,0,
			 1
		}, // 左手座標系（DirectXのデフォルト）＋Y軸反転

		{
			 1,0,0,0,0,
			 0,1,0,0,1,
			 0,0,0,0,0,
			 1
		}, // 右手座標系（OpenGLのデフォルト）＋Y軸反転
	};

	// To change the units from centimeters to meters, set 'scale_factor' to 0.01.
	const float scale_factor = 1.0f;
	DirectX::XMMATRIX C{
	    DirectX::XMLoadFloat4x4(&coordinate_system_transforms[2])
		* DirectX::XMMatrixScaling(scale_factor, scale_factor, scale_factor) 
	};

	// 拡大縮小（S）・回転（R）・平行移動（T）行列を計算する
	//XMMATRIX S{ XMMatrixScaling(cube_scale.x, cube_scale.y, cube_scale.z) };
	XMMATRIX S3{ XMMatrixScaling(static_mesh_scale.x, static_mesh_scale.y, static_mesh_scale.z) };
	XMMATRIX S4{ XMMatrixScaling(skinned_mesh_scale.x, skinned_mesh_scale.y, skinned_mesh_scale.z) };

	// 回転は度数法からラジアンに変換して行列に渡す(ImGuiを使うため)
	// XMMATRIX R{ XMMatrixRotationRollPitchYaw(0.0f, 0.0f, 0.0f) };
	//XMMATRIX R{ XMMatrixRotationRollPitchYaw(
	//       XMConvertToRadians(cube_rotation.x), // Roll
	//       XMConvertToRadians(cube_rotation.y), // Pitch
	//       XMConvertToRadians(cube_rotation.z)  // Yaw
	//) };
	XMMATRIX R3{ XMMatrixRotationRollPitchYaw(
	       XMConvertToRadians(static_mesh_rotation.x), // Roll
	       XMConvertToRadians(static_mesh_rotation.y), // Pitch
	       XMConvertToRadians(static_mesh_rotation.z)  // Yaw
	) };
	XMMATRIX R4{ XMMatrixRotationRollPitchYaw(
	       XMConvertToRadians(skinned_mesh_rotation.x), // Roll
	       XMConvertToRadians(skinned_mesh_rotation.y), // Pitch
	       XMConvertToRadians(skinned_mesh_rotation.z)  // Yaw
	) };

	//XMMATRIX T{ XMMatrixTranslation(cube_position.x, cube_position.y, cube_position.z) };
	//XMMATRIX T2{ XMMatrixTranslation(cube_position2.x, cube_position2.y, cube_position2.z) };
	XMMATRIX T3{ XMMatrixTranslation(static_mesh_position.x, static_mesh_position.y, static_mesh_position.z) };
	XMMATRIX T4{ XMMatrixTranslation(skinned_mesh_position.x, skinned_mesh_position.y, skinned_mesh_position.z) };

	// 上記３行列を合成しワールド変換行列を作成する
	/*DirectX::XMFLOAT4X4 world;
	DirectX::XMStoreFloat4x4(&world, S * R * T);
	DirectX::XMFLOAT4X4 world2;
	DirectX::XMStoreFloat4x4(&world2, S * R * T2);*/
	DirectX::XMFLOAT4X4 world3;
	DirectX::XMStoreFloat4x4(&world3, S3 * R3 * T3);
	DirectX::XMFLOAT4X4 world4;
	DirectX::XMStoreFloat4x4(&world4, C* S4* R4* T4);

	// geometric_primitive クラスの render メンバ関数を呼び出す
    // ※深度テスト：オン、深度ライト：オンの深度ステンシルステートをバインドしておく
	// 深度テストON / 深度ライトON のステート（0番）をバインドする
	immediate_context->OMSetDepthStencilState(depth_stencil_states[0].Get(), 0);

	// 3Dオブジェクト描画の直前でブレンディングステートオブジェクトを設定
	immediate_context->OMSetBlendState(blend_states[0].Get(), nullptr, 0xFFFFFFFF);

	// // ラスタライザステートの切り替え(3D)
	immediate_context->RSSetState(rasterizer_states[Rasterizer_index].Get());
	
	// 1つ目の正立方体：ソリッド描画（左側）
	/*geometric_primitives[0]->render(
		immediate_context.Get(), 
		world, 
		{ cube_color[0], cube_color[1], cube_color[2], cube_color[3] }
	);*/

	// 2つ目の正立方体：ワイヤーフレーム描画（右側）
	/*immediate_context->RSSetState(rasterizer_states[1].Get());

	geometric_primitives[1]->render(
		immediate_context.Get(),
		world2,
		{ cube_color[0], cube_color[1], cube_color[2], cube_color[3] }
	);*/

	//-----------------------------------------------------
	// スタティックメッシュ用(境界ボックスの可視化)
	//-----------------------------------------------------
	
	// ① 各クラスに作ったゲッターから最小・最大座標を取得
	//DirectX::XMFLOAT3 min_v, max_v;
	//static_meshes[0]->get_bounding_box(min_v, max_v);

	//// ② XMVECTORに変換し、境界ボックスの「サイズ」と「中心座標」を計算
	//DirectX::XMVECTOR min_vec = DirectX::XMLoadFloat3(&min_v);
	//DirectX::XMVECTOR max_vec = DirectX::XMLoadFloat3(&max_v);

	//// Center = (min + max) * 0.5
	//DirectX::XMVECTOR center_vec = DirectX::XMVectorScale(DirectX::XMVectorAdd(min_vec, max_vec), 0.5f);
	//// Size = max - min
	//DirectX::XMVECTOR size_vec = DirectX::XMVectorSubtract(max_vec, min_vec);

	//DirectX::XMFLOAT3 center, size;
	//DirectX::XMStoreFloat3(&center, center_vec);
	//DirectX::XMStoreFloat3(&size, size_vec);

	//// ③ 境界ボックス専用のワールド行列を作成
	//// 【順序】ボックス自体の拡大縮小 → ボックス自体の平行移動 → オブジェクト本来のワールド行列
	//DirectX::XMMATRIX box_scale = DirectX::XMMatrixScaling(size.x, size.y, size.z);
	//DirectX::XMMATRIX box_translation = DirectX::XMMatrixTranslation(center.x, center.y, center.z);
	//DirectX::XMMATRIX object_world = DirectX::XMLoadFloat4x4(&world3);

	//DirectX::XMFLOAT4X4 box_world_matrix;
	//DirectX::XMStoreFloat4x4(&box_world_matrix, box_scale* box_translation* object_world);

	//// ④ ラスタライザステートを「ワイヤフレーム（線画）」に切り替える
	//immediate_context->RSSetState(rasterizer_states[1].Get());

	//// ⑤ geometric_primitive の正六面体(CUBE)を使って、計算した行列で箱を描画
	//geometric_primitives[0]->render(immediate_context.Get(), box_world_matrix, DirectX::XMFLOAT4(0.0f, 1.0f, 0.0f, 0.5f));

	//// ⑥ 描画が終わったら、ラスタライザステートを「ソリッド（通常の塗りつぶし）」に戻す
	//immediate_context->RSSetState(rasterizer_states[0].Get());

	//// static_meshクラスのrenderメンバ関数を呼び出す	
	////static_meshes[0]->render(immediate_context.Get(), world3, { static_mesh_color[0], static_mesh_color[1], static_mesh_color[2], static_mesh_color[3] });

	//// （第4引数に、用意したモノクロ化やモザイクなどのシェーダーオブジェクトを渡す）
	//static_meshes[0]->render(
	//	immediate_context.Get(),
	//	world3,
	//	{ static_mesh_color[0], static_mesh_color[1], static_mesh_color[2], static_mesh_color[3] }
	//	//replaced_pixel_shaders[0].Get() // 第4引数に差し替え用シェーダーを渡す
	//);

	//-----------------------------------------------------
	// スキンメッシュ用(境界ボックスの可視化)
	//-----------------------------------------------------
	
	DirectX::XMFLOAT3 bbox_min, bbox_max;
	skinned_meshes[0]->get_bounding_box(bbox_min, bbox_max);

	DirectX::XMVECTOR bbox_min_vec = DirectX::XMLoadFloat3(&bbox_min);
	DirectX::XMVECTOR bbox_max_vec = DirectX::XMLoadFloat3(&bbox_max);

	// Center = (min + max) * 0.5
	DirectX::XMVECTOR center_bbvec = DirectX::XMVectorScale(DirectX::XMVectorAdd(bbox_min_vec, bbox_max_vec), 0.5f);
	// Size = max - min
	DirectX::XMVECTOR size_bbvec = DirectX::XMVectorSubtract(bbox_max_vec, bbox_min_vec);
	DirectX::XMFLOAT3 bbcenter, bbsize;
	DirectX::XMStoreFloat3(&bbcenter, center_bbvec);
	DirectX::XMStoreFloat3(&bbsize, size_bbvec);

	// 境界ボックス専用のワールド行列を作成
	DirectX::XMMATRIX bbox_scale = DirectX::XMMatrixScaling(bbsize.x, bbsize.y, bbsize.z);
	DirectX::XMMATRIX bbox_translation = DirectX::XMMatrixTranslation(bbcenter.x, bbcenter.y, bbcenter.z);
	DirectX::XMMATRIX bbox_object_world = DirectX::XMLoadFloat4x4(&world4);

	DirectX::XMFLOAT4X4 bbox_world_matrix;
	DirectX::XMStoreFloat4x4(&bbox_world_matrix, bbox_scale * bbox_translation * bbox_object_world);

	// ラスタライザステートを「ワイヤフレーム」に切り替え
	immediate_context->RSSetState(rasterizer_states[1].Get());

	// geometric_primitive の正六面体を使って箱を描画
	geometric_primitives[0]->render(immediate_context.Get(), bbox_world_matrix, DirectX::XMFLOAT4(0.0f, 1.0f, 0.0f, 0.5f));

	// ラスタライザステートを「ソリッド」に戻す
	immediate_context->RSSetState(rasterizer_states[0].Get());

	// skinned_meshクラスのrenderメンバ関数を呼び出す
	skinned_meshes[0]->render(
		immediate_context.Get(),
		world4,
		{ skinned_mesh_color[0], skinned_mesh_color[1], skinned_mesh_color[2], skinned_mesh_color[3] }
	);

#ifdef USE_IMGUI
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
#endif 

	UINT sync_interval{ 0 };
	swap_chain->Present(sync_interval, 0);
}

bool framework::uninitialize()
{
	/*device->Release();
	immediate_context->Release();
	swap_chain->Release();
	render_target_view->Release();
	depth_stencil_view->Release();
	for (sprite* p : sprites) delete p;
	for (sprite_batch* s : sprite_batches)delete s;*/

	return true;
}

framework::~framework()
{

}