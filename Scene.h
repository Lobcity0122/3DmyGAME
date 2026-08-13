#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <directxmath.h>

using namespace Microsoft::WRL;

// シーンタイプの列挙
enum class SceneType
{
	MENU,
	// タイトル画面の背後で、本編と同じ3Dシーンを自動操作するデモ再生。
	PACMAN_ATTRACT,
	PACMAN,
	PACMAN_RESULT,
	ONLYUP
};

// A compact record handed from the game scene to the result scene.
// Keeping it here makes the scene boundary explicit and avoids ResultScene
// depending on PacmanGameScene internals.
struct GameResultData
{
	bool cleared = false;
	int recovered_circuits = 0;
	int total_circuits = 0;
	int remaining_lives = 0;
	float elapsed_seconds = 0.0f;
	int score = 0;
	int high_score = 0;
};

// This game currently has one play mode, so a single shared hand-off record is enough.
// When multiple concurrent game modes are added, this can become framework-owned data.
inline GameResultData latest_game_result{};
// Session-only high score. File persistence can later be added without changing scenes.
inline int session_high_score = 0;

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

	// シーン自身が遷移を要求する場合に使う。通常は現在のシーンを継続する。
	virtual SceneType get_next_scene() const { return get_type(); }
};
