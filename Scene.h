#pragma once

#include <d3d11.h>

// framework が管理するシーンの種類。シーン自身は get_next_scene() で
// 次に遷移したい種類を返し、実際の生成・破棄は framework が行う。
enum class SceneType
{
	MENU,
	PACMAN_ATTRACT, // タイトル画面の背景で流す無操作デモ。
	PACMAN,
	PACMAN_RESULT,
	ONLYUP
};

// ゲームシーンからリザルトシーンへ渡す、1プレイ分の確定結果。
// リザルト側がPacmanGameSceneの内部実装へ依存しないよう、この共通ヘッダに置く。
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

// 現在は1つのゲームモードだけなので、シーン間の受け渡しには共有データで十分。
// モードやセーブデータが増えたら、framework所有のゲーム状態へ移行できる。
inline GameResultData latest_game_result{};
inline int session_high_score = 0;

// すべてのシーンが実装する最小限のライフサイクル。
class Scene
{
public:
	virtual ~Scene() = default;

	virtual bool initialize(ID3D11Device* device) = 0;
	virtual void update(float elapsed_time) = 0;
	virtual void render(ID3D11DeviceContext* immediate_context, float elapsed_time) = 0;
	virtual void uninitialize() = 0;

	virtual SceneType get_type() const = 0;
	// 遷移要求がなければ、現在のシーン種類を返して継続する。
	virtual SceneType get_next_scene() const { return get_type(); }
};
