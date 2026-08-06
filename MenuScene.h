#pragma once

#include "Scene.h"

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
};
