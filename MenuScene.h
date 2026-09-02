#pragma once

#include "Scene.h"
#include "sprite.h"
#include <memory>

// CIRCUIT TRAX専用のタイトル画面。複数ゲームを選択するランチャーではない。
class MenuScene : public Scene
{
public:
	bool initialize(ID3D11Device* device) override;
	void update(float elapsed_time) override;
	void render(ID3D11DeviceContext* immediate_context, float elapsed_time) override;
	void uninitialize() override;

	SceneType get_type() const override { return SceneType::MENU; }
	SceneType get_next_scene() const override { return next_scene_type; }

private:
	SceneType next_scene_type = SceneType::MENU;
	std::unique_ptr<sprite> background;
	std::unique_ptr<sprite> font;
	bool previous_enter_pressed = false;
	float idle_seconds = 0.0f;
	float title_pulse_time = 0.0f;
};
