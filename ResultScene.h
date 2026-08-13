#pragma once

#include "Scene.h"
#include "sprite.h"
#include <memory>

// Temporary result presentation. Its data and scene transition remain valid
// when this text-based prototype is later replaced with authored UI sprites.
class ResultScene final : public Scene
{
public:
	bool initialize(ID3D11Device* device) override;
	void update(float elapsed_time) override;
	void render(ID3D11DeviceContext* immediate_context, float elapsed_time) override;
	void uninitialize() override;
	SceneType get_type() const override { return SceneType::PACMAN_RESULT; }
	SceneType get_next_scene() const override { return next_scene_type; }

private:
	std::unique_ptr<sprite> background;
	std::unique_ptr<sprite> font;
	SceneType next_scene_type = SceneType::PACMAN_RESULT;
	bool previous_enter_pressed = false;
	float blink_time = 0.0f;
};
