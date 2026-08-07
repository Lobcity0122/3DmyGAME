#pragma once
#include <windows.h>
#include <d3d11.h>
#include <wrl.h>
#include <memory>
#include "high_resolution_timer.h"
#include "Scene.h"
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern ImWchar glyphRangesJapanese[];
#endif
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define FULLSCREEN FALSE
#define APPLICATION_NAME L"X3DGP"
class framework
{
public:
	const HWND hwnd;
	framework(HWND window) : hwnd(window) {}
	~framework() = default;
	framework(const framework&) = delete;
	framework& operator=(const framework&) = delete;
	int run();
	LRESULT CALLBACK handle_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
private:
	bool initialize();
	void update(float elapsed_time);
	void render(float elapsed_time);
	bool uninitialize();
	void change_scene(SceneType new_scene_type);
	void calculate_frame_stats();
	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediate_context;
	Microsoft::WRL::ComPtr<IDXGISwapChain> swap_chain;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_target_view;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depth_stencil_view;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> linear_sampler;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depth_enabled_state;
	Microsoft::WRL::ComPtr<ID3D11BlendState> opaque_blend_state;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer_state;
	std::unique_ptr<Scene> current_scene;
	SceneType requested_scene_type = SceneType::RACING;
	high_resolution_timer tictoc;
	uint32_t frames_per_second = 0;
	float count_by_seconds = 0.0f;
};
