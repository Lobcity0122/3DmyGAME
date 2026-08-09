#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <directxmath.h>

using namespace Microsoft::WRL;

// シーンタイプの列挙
enum class SceneType
{
	MENU,
	PACMAN,
	ONLYUP
};

// シーン基底クラス
class Scene
{
public:
	virtual ~Scene() {}

	// 初期化
	virtual bool initialize(ID3D11Device* device) = 0;

	// 更新
	virtual void update(float elapsed_time) = 0;

	// 描画
	virtual void render(ID3D11DeviceContext* immediate_context, float elapsed_time) = 0;

	// 終了処理
	virtual void uninitialize() = 0;

	// シーンタイプを取得
	virtual SceneType get_type() const = 0;
};
