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
	// アプリケーションループのエントリーポイント。このクラスはDirect3Dを所有し、ゲームオブジェクトは所有しない
	const HWND hwnd;
	framework(HWND window) : hwnd(window) {}
	~framework() = default;
	framework(const framework&) = delete;
	framework& operator=(const framework&) = delete;

	int run();
	LRESULT CALLBACK handle_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

private:
	// 初期化、フレームごとの更新・描画、終了処理は意図的に分離
	bool initialize();
	void update(float elapsed_time);
	void render(float elapsed_time);
	bool create_bloom_resources();
	void render_bloom();
	bool uninitialize();
	void change_scene(SceneType new_scene_type);
	void calculate_frame_stats();

	// すべてのシーンで共有されるDirect3Dオブジェクト
	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediate_context;
	Microsoft::WRL::ComPtr<IDXGISwapChain> swap_chain;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_target_view;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depth_stencil_view;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> linear_sampler;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depth_enabled_state;
	Microsoft::WRL::ComPtr<ID3D11BlendState> opaque_blend_state;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer_state;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> scene_color_texture;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> scene_color_render_target_view;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> scene_color_shader_resource_view;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> bloom_vertex_shader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> bloom_pixel_shader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> bloom_input_layout;
	Microsoft::WRL::ComPtr<ID3D11Buffer> bloom_vertex_buffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> bloom_constant_buffer;
	bool bloom_enabled = true;
	float bloom_threshold = 0.65f;
	float bloom_intensity = 0.75f;

	// シーンがゲームルールとモデルを所有する。frameworkはこのインターフェースを呼び出すだけ
	std::unique_ptr<Scene> current_scene;
	SceneType requested_scene_type = SceneType::PACMAN;
	high_resolution_timer tictoc;
	// Change this value to change the application's maximum frame rate.
	static constexpr float target_fps = 144.0f;
	uint32_t frames_per_second = 0;
	float count_by_seconds = 0.0f;
};
