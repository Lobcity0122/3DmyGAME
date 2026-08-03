#pragma once
#include <d3d11.h>
#include <directxmath.h>
#include <string>
#include <wrl.h>
using namespace Microsoft::WRL;

using namespace std;

class sprite
{
	// 頂点フォーマット
	struct vertex
	{
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT4 color;
		DirectX::XMFLOAT2 texcoord;
	};

public:
	
	// コンストラクタ・デストラクタ
	sprite(ID3D11Device* device, const wchar_t* filename);
	~sprite();

	// メンバ関数
	void render(ID3D11DeviceContext* immediate_context,
		float dx, float dy, float dw, float dh,
		float r, float g, float b, float a,
		float angle/*degree*/);

	// 回転処理
	// 点(x, y)が点(cx, cy)を中心に角(angle)で回転した時の座標を計算する関数オブジェクト（ラムダ式）
	void rotate(float& x, float& y, float cx, float cy, float cos, float sin)
		{
			x -= cx;
			y -= cy;

			float tx{ x }, ty{ y };
			x = cos * tx + -sin * ty;
			y = sin * tx + cos * ty;

			x += cx;
			y += cy;
		};

	// オーバーロード(画面上の描画位置とサイズ指定のみでテクスチャ全体を描画)
	void render(ID3D11DeviceContext* immediate_context,
		float dx, float dy, float dw, float dh);

	// オーバーロード
	void render(ID3D11DeviceContext* immediate_context,
		float dx, float dy, float dw, float dh,
		float r, float g, float b, float a,
		float angle/*degree*/,
		float sx, float sy, float sw, float sh);

	// 画像ファイルを使用し任意の文字列を画面に出力する(textout)を追加する
	// フォント画像ファイルはアスキーコード順に16×16の文字が配置された画像ファイル
	void textout(ID3D11DeviceContext* immediate_context, std::string s,
		float x, float y, float w, float h, float r, float g, float b, float a
	);

private:
	// メンバ変数
	ComPtr<ID3D11VertexShader> vertex_shader;
	ComPtr<ID3D11PixelShader> pixel_shader;
	ComPtr<ID3D11InputLayout> input_layout;
	ComPtr<ID3D11Buffer> vertex_buffer;
	ComPtr<ID3D11ShaderResourceView> shader_resource_view;
	D3D11_TEXTURE2D_DESC texture2d_desc;
};