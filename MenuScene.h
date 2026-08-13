#pragma once

#include "Scene.h"
#include "sprite.h"
#include <memory>

class MenuScene : public Scene
{
public:
	MenuScene();
	virtual ~MenuScene() override;

	virtual bool initialize(ID3D11Device* device) override;
	virtual void update(float elapsed_time) override;
	virtual void render(ID3D11DeviceContext* immediate_context, float elapsed_time) override;
	virtual void uninitialize() override;

	virtual SceneType get_type() const override { return SceneType::MENU; }
	virtual SceneType get_next_scene() const override { return next_scene_type; }

private:
	SceneType next_scene_type = SceneType::MENU;
	std::unique_ptr<sprite> background;
	std::unique_ptr<sprite> font;
	int selected_game = 0;
	bool previous_left_pressed = false;
	bool previous_right_pressed = false;
	bool previous_enter_pressed = false;
	float locked_message_timer = 0.0f;
};
