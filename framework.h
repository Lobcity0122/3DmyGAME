#pragma once

#include <windows.h>
#include <tchar.h>
#include <sstream>

#include "misc.h"
#include "high_resolution_timer.h"
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern ImWchar glyphRangesJapanese[];
#endif
#include <d3d11.h>
#include "sprite.h"
#include "sprite_batch.h"

#include <wrl.h>
using namespace Microsoft::WRL;

#include <memory> // std::unique_ptr を使うために必要
using namespace std;
//#include <sprite.h>

using namespace ImGui;

#include "geometric_primitive.h"
#include "static_mesh.h"
#include "skinned_mesh.h"

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define FULLSCREEN FALSE
#define APPLICATION_NAME L"X3DGP"

class framework
{
public:
	CONST HWND hwnd;

	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> immediate_context;
	ComPtr<IDXGISwapChain> swap_chain;
	ComPtr<ID3D11RenderTargetView> render_target_view;
	ComPtr<ID3D11DepthStencilView> depth_stencil_view;

	ComPtr<ID3D11SamplerState> sampler_states[3];

	// オブジェクトの描画順と画面上の前後関係の設定
	ComPtr<ID3D11DepthStencilState> depth_stencil_states[4];

	// 画像の背景の透過させるための変数
	ComPtr<ID3D11BlendState> blend_states[4];

	// 切り替え用のピクセルシェーダー配列
	ComPtr<ID3D11PixelShader> replaced_pixel_shaders[8];

	unique_ptr<sprite> sprites[8];
	unique_ptr<sprite_batch> sprite_batches[8];

	// シーン定数バッファ
	struct scene_constans
	{
		DirectX::XMFLOAT4X4 view_projection; // ビュー・プロジェクション変換行列
		DirectX::XMFLOAT4 light_direction;   // ライトの向き
		DirectX::XMFLOAT4 camera_position;   // カメラの位置
	};
	ComPtr<ID3D11Buffer> constant_buffers[8];

	unique_ptr<geometric_primitive> geometric_primitives[8];

	// ワイヤーフレーム用
	// 0:ソリッド・裏面カリング
	ComPtr<ID3D11RasterizerState> rasterizer_states[5];

	// static_mesh *型配列を要素数8で宣言
	std::unique_ptr<static_mesh> static_meshes[8];

	// sknned_mesh *型配列を要素数8で宣言
	std::unique_ptr<skinned_mesh> skinned_meshes[8];

	////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// ImGuiで各種設定
	// [ステート選択用インデックス]
	int Sampler_index = 1; // 初期値 Linear
	int Blend_index = 0;   // 初期値 Alpha Blend
	int Depth_index = 3;   // 初期値 Test:OFF/Write:OFF
	int Rasterizer_index = 0; // 初期値 裏面カリング(塗りつぶし)

	// Sprite0用パラメータ
	DirectX::XMFLOAT2 position0{ 0.0f, 0.0f };
	float sprite0_angle = 0.0f;
	DirectX::XMFLOAT4 sprite0_color{ 1.0f, 1.0f, 1.0f, 1.0f };

	// Sprite1用パラメータ
	DirectX::XMFLOAT2 position1{ 15.0f, 200.0f };
	float sprite1_angle = 0.0f;
	DirectX::XMFLOAT4 sprite1_color{ 1.0f, 1.0f, 1.0f, 1.0f };
	int animationNo = 0;
	float src_y = 0.0f;

	// [Textout用パラメータ]
	char text_buffer[128] = "DIRECTX11";
	DirectX::XMFLOAT2 text_pos{ 0.0f, 0.0f };
	DirectX::XMFLOAT2 text_size{ 32.0f, 32.0f }; // 1文字の幅と高さ
	DirectX::XMFLOAT4 text_color{ 1.0f, 1.0f, 1.0f, 1.0f };

	bool use_batch = true;    // スプライトバッチを使用 / 通常スプライト単体
	int sprite_draw_count = 1; // 描画するスプライトの個数

	// [カメラ用のパラメータ]
	// カメラの位置 (0, 0, -10)
	DirectX::XMFLOAT4 camera_position{ 0.0f,0.0f,-10.0f ,1.0f };

	// ライトの照射方向
	DirectX::XMFLOAT4 light_direction{ 0.0f,0.0f,1.0f,0.0f };

	// [幾何プリミティブ用パラメータ]
	// 位置
	/*DirectX::XMFLOAT3 cube_position{ -1.0f,0.0f,0.0f };
	DirectX::XMFLOAT3 cube_position2{ 1.0f,0.0f,0.0f };*/

	// 姿勢(Roll、Pitch、Yaw) ※ImGUIで扱いやすいよう「度数法」で保持
	DirectX::XMFLOAT3 cube_rotation{ 0.0f,0.0f,0.0f };

	// 寸法
	DirectX::XMFLOAT3 cube_scale{ 1.0f,1.0f,1.0f };

	// 色
	float cube_color[4] = { 0.5f,0.8f,0.2f,1.0f };

	// [static_mesh用パラメータ]
	DirectX::XMFLOAT3 static_mesh_position{ 0.0f,0.0f,0.0f };

	DirectX::XMFLOAT3 static_mesh_rotation{ 0.0f,0.0f,0.0f };

	DirectX::XMFLOAT3 static_mesh_scale{ 1.0f,1.0f,1.0f };

	float static_mesh_color[4] = { 1.0f,1.0f,1.0f,1.0f };

	// [skinned_mesh用パラメータ]
	DirectX::XMFLOAT3 skinned_mesh_position{ 0.0f,0.0f,0.0f };

	DirectX::XMFLOAT3 skinned_mesh_rotation{ 0.0f,0.0f,0.0f };

	DirectX::XMFLOAT3 skinned_mesh_scale{ 1.0f,1.0f,1.0f };

	float skinned_mesh_color[4] = { 1.0f,1.0f,1.0f,1.0f };

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	
	framework(HWND hwnd);
	~framework();

	framework(const framework&) = delete;
	framework& operator=(const framework&) = delete;
	framework(framework&&) noexcept = delete;
	framework& operator=(framework&&) noexcept = delete;

	int run()
	{
		MSG msg{};

		if (!initialize())
		{
			return 0;
		}

#ifdef USE_IMGUI
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::GetIO().Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", 14.0f, nullptr, glyphRangesJapanese);
		ImGui_ImplWin32_Init(hwnd);
		ImGui_ImplDX11_Init(device.Get(), immediate_context.Get());
		ImGui::StyleColorsDark();
#endif

		while (WM_QUIT != msg.message)
		{
			if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else
			{
				tictoc.tick();
				calculate_frame_stats();
				update(tictoc.time_interval());
				render(tictoc.time_interval());
			}
		}

#ifdef USE_IMGUI
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
#endif

#if 1
		//BOOL fullscreen = 0;
		BOOL fullscreen{};
		swap_chain->GetFullscreenState(&fullscreen, 0);
		if (fullscreen)
		{
			swap_chain->SetFullscreenState(FALSE, 0);
		}
#endif

		return uninitialize() ? static_cast<int>(msg.wParam) : 0;
	}

	LRESULT CALLBACK handle_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
#ifdef USE_IMGUI
		if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) { return true; }
#endif
		switch (msg)
		{
		case WM_PAINT:
		{
			PAINTSTRUCT ps{};
			BeginPaint(hwnd, &ps);

			EndPaint(hwnd, &ps);
		}
		break;

		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		case WM_CREATE:
			break;
		case WM_KEYDOWN:
			if (wparam == VK_ESCAPE)
			{
				PostMessage(hwnd, WM_CLOSE, 0, 0);
			}
			break;
		case WM_ENTERSIZEMOVE:
			tictoc.stop();
			break;
		case WM_EXITSIZEMOVE:
			tictoc.start();
			break;
		default:
			return DefWindowProc(hwnd, msg, wparam, lparam);
		}
		return 0;
	}

private:
	bool initialize();
	void update(float elapsed_time/*Elapsed seconds from last frame*/);
	void render(float elapsed_time/*Elapsed seconds from last frame*/);
	bool uninitialize();

private:
	high_resolution_timer tictoc;
	uint32_t frames_per_second{ 0 };
	float count_by_seconds{ 0.0f };
	void calculate_frame_stats()
	{
		if (++frames_per_second, (tictoc.time_stamp() - count_by_seconds) >= 1.0f)
		{
			float fps = static_cast<float>(frames_per_second);
			std::wostringstream outs;
			outs.precision(6);
			outs << L"X3DGP" << L" : FPS : " << fps << L" / " << L"Frame Time : " << 1000.0f / fps << L" (ms)";
			SetWindowTextW(hwnd, outs.str().c_str());

			frames_per_second = 0;
			count_by_seconds += 1.0f;
		}
	}
};
