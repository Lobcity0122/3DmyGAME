#include "framework.h"
#include "RacingGameScene.h"
#include <chrono>
#include <cmath>
#include <sstream>

using namespace Microsoft::WRL;

bool framework::initialize()
{
	// 初期化の順序: デバイス/スワップチェーン -> カラー/深度ターゲット -> 共通ステート -> 最初のシーン
	DXGI_SWAP_CHAIN_DESC swap_chain_desc{};
	swap_chain_desc.BufferCount = 1;
	swap_chain_desc.BufferDesc.Width = SCREEN_WIDTH;
	swap_chain_desc.BufferDesc.Height = SCREEN_HEIGHT;
	swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swap_chain_desc.BufferDesc.RefreshRate.Numerator = 60;
	swap_chain_desc.BufferDesc.RefreshRate.Denominator = 1;
	swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain_desc.OutputWindow = hwnd;
	swap_chain_desc.SampleDesc.Count = 1;
	swap_chain_desc.Windowed = !FULLSCREEN;

	UINT flags = 0;
#ifdef _DEBUG
	flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
	D3D_FEATURE_LEVEL feature_level{};
	const D3D_FEATURE_LEVEL feature_levels[] = { D3D_FEATURE_LEVEL_11_0 };
	HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
		feature_levels, ARRAYSIZE(feature_levels), D3D11_SDK_VERSION, &swap_chain_desc,
		swap_chain.GetAddressOf(), device.GetAddressOf(), &feature_level, immediate_context.GetAddressOf());
	if (FAILED(hr)) return false;

	// バックバッファは可視カラーを格納する。別のテクスチャはピクセルごとの深度を格納する。
	ComPtr<ID3D11Texture2D> back_buffer;
	if (FAILED(swap_chain->GetBuffer(0, IID_PPV_ARGS(back_buffer.GetAddressOf()))) ||
		FAILED(device->CreateRenderTargetView(back_buffer.Get(), nullptr, render_target_view.GetAddressOf()))) return false;

	// 深度バッファは、ピクセルごとの深度を格納する
	D3D11_TEXTURE2D_DESC depth_desc{};
	depth_desc.Width = SCREEN_WIDTH;
	depth_desc.Height = SCREEN_HEIGHT;
	depth_desc.MipLevels = 1;
	depth_desc.ArraySize = 1;
	depth_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depth_desc.SampleDesc.Count = 1;
	depth_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	ComPtr<ID3D11Texture2D> depth_texture;
	if (FAILED(device->CreateTexture2D(&depth_desc, nullptr, depth_texture.GetAddressOf())) ||
		FAILED(device->CreateDepthStencilView(depth_texture.Get(), nullptr, depth_stencil_view.GetAddressOf()))) return false;

	// これらはすべての Scene::render() 呼び出しの前に使用されるデフォルトのステート。
	D3D11_SAMPLER_DESC sampler_desc{};
	sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampler_desc.AddressU = sampler_desc.AddressV = sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
	if (FAILED(device->CreateSamplerState(&sampler_desc, linear_sampler.GetAddressOf()))) return false;

	// 深度ステンシルステートは、ピクセルの深度テストを制御する
	D3D11_DEPTH_STENCIL_DESC depth_state_desc{};
	depth_state_desc.DepthEnable = TRUE;
	depth_state_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depth_state_desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	if (FAILED(device->CreateDepthStencilState(&depth_state_desc, depth_enabled_state.GetAddressOf()))) return false;

	// ブレンドステートは、ピクセルのアルファブレンドを制御する
	D3D11_BLEND_DESC blend_desc{};
	blend_desc.RenderTarget[0].BlendEnable = FALSE;
	blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	if (FAILED(device->CreateBlendState(&blend_desc, opaque_blend_state.GetAddressOf()))) return false;

	// ラスタライザーステートは、ポリゴンの塗りつぶし方法とカリング方法を制御する
	D3D11_RASTERIZER_DESC rasterizer_desc{};
	rasterizer_desc.FillMode = D3D11_FILL_SOLID;
	rasterizer_desc.CullMode = D3D11_CULL_BACK;
	rasterizer_desc.DepthClipEnable = TRUE;
	if (FAILED(device->CreateRasterizerState(&rasterizer_desc, rasterizer_state.GetAddressOf()))) return false;

	// ビューポートは、レンダリング対象の矩形領域を定義する
	D3D11_VIEWPORT viewport{};
	viewport.Width = static_cast<float>(SCREEN_WIDTH);
	viewport.Height = static_cast<float>(SCREEN_HEIGHT);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	immediate_context->RSSetViewports(1, &viewport);

	change_scene(SceneType::RACING);
	return current_scene != nullptr;
}

// アプリケーションループのエントリーポイント、フレームごとに更新と描画を繰り返す
int framework::run()
{
	if (!initialize()) return 0;
#ifdef USE_IMGUI
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::GetIO().Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", 14.0f, nullptr, glyphRangesJapanese);
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX11_Init(device.Get(), immediate_context.Get());
	ImGui::StyleColorsDark();
#endif

	MSG msg{};
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			// フレームごとの処理: 時間計測 -> 更新 -> 描画 -> FPS制限
			const auto frame_start = std::chrono::steady_clock::now();
			tictoc.tick();
			calculate_frame_stats();
			update(tictoc.time_interval());
			render(tictoc.time_interval());

			const float elapsed_seconds = std::chrono::duration<float>(
				std::chrono::steady_clock::now() - frame_start).count();
			const float target_seconds = 1.0f / target_fps;
			if (elapsed_seconds < target_seconds)
			{
				// Sleep() はミリ秒単位で待機するため、秒単位の差をミリ秒に変換して切り上げる
				const DWORD sleep_milliseconds = static_cast<DWORD>(std::ceil((target_seconds - elapsed_seconds) * 1000.0f));
				Sleep(sleep_milliseconds);
			}
		}
	}
#ifdef USE_IMGUI
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
#endif
	return uninitialize() ? static_cast<int>(msg.wParam) : 0;
}

void framework::update(float elapsed_time)
{
#ifdef USE_IMGUI
	// シーンの render() 内でウィジェットが送信される場合があるため、先に ImGui のフレームを開始します。
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
#endif

	if (current_scene && current_scene->get_type() != requested_scene_type)
		change_scene(requested_scene_type);
	if (current_scene) current_scene->update(elapsed_time);
}

void framework::render(float elapsed_time)
{
	// 毎フレームの実行順序: 画面クリア -> 共通ステート設定 -> シーン描画コマンド -> ImGui処理 -> 画面反映(Present)
	const float clear_color[] = { 0.0f, 0.0f, 1.0f, 1.0f }; // 0.08f, 0.10f, 0.14f, 1.0f
	immediate_context->ClearRenderTargetView(render_target_view.Get(), clear_color);
	immediate_context->ClearDepthStencilView(depth_stencil_view.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
	immediate_context->OMSetRenderTargets(1, render_target_view.GetAddressOf(), depth_stencil_view.Get());
	immediate_context->OMSetDepthStencilState(depth_enabled_state.Get(), 0);
	immediate_context->OMSetBlendState(opaque_blend_state.Get(), nullptr, 0xFFFFFFFF);
	immediate_context->RSSetState(rasterizer_state.Get());
	immediate_context->PSSetSamplers(0, 1, linear_sampler.GetAddressOf());
	if (current_scene) current_scene->render(immediate_context.Get(), elapsed_time);
#ifdef USE_IMGUI
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
#endif
	swap_chain->Present(0, 0);
}

bool framework::uninitialize()
{
	if (current_scene) current_scene->uninitialize();
	current_scene.reset();
	return true;
}

// シーンの切り替えは、現在のシーンを uninitialize() して破棄し、新しいシーンを生成して initialize() する。
void framework::change_scene(SceneType new_scene_type)
{
	if (current_scene) current_scene->uninitialize();
	current_scene.reset();
	if (new_scene_type == SceneType::RACING)
		current_scene = std::make_unique<RacingGameScene>();
	if (current_scene && current_scene->initialize(device.Get()))
		requested_scene_type = new_scene_type;
	else
		current_scene.reset();
}

// フレームごとの統計情報を計算し、ウィンドウタイトルに FPS を表示する
void framework::calculate_frame_stats()
{
	if (++frames_per_second && (tictoc.time_stamp() - count_by_seconds) >= 1.0f)
	{
		std::wostringstream title;
		title << L"X3DGP : FPS " << frames_per_second;
		SetWindowTextW(hwnd, title.str().c_str());
		frames_per_second = 0;
		count_by_seconds = tictoc.time_stamp();
	}
}

// ウィンドウメッセージの処理。WM_DESTROY で PostQuitMessage() を呼び出す。
LRESULT CALLBACK framework::handle_message(HWND, UINT msg, WPARAM wparam, LPARAM lparam)
{
#ifdef USE_IMGUI
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) return true;
#endif
	switch (msg)
	{
	case WM_DESTROY: PostQuitMessage(0); return 0;
	case WM_KEYDOWN:
		if (wparam == VK_ESCAPE) PostMessage(hwnd, WM_CLOSE, 0, 0);
		return 0;
	default: return DefWindowProc(hwnd, msg, wparam, lparam);
	}
}
