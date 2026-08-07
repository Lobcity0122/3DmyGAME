#pragma once

#include "Scene.h"
#include "Player.h"
#include "CameraController.h"
#include "skinned_mesh.h"
#include <memory>

// レーシングゲームシーン
class RacingGameScene : public Scene
{
public:
	RacingGameScene();
	virtual ~RacingGameScene() override;

	virtual bool initialize(ID3D11Device* device) override;
	virtual void update(float elapsedTime) override;
	virtual void render(ID3D11DeviceContext* immediate_context, float elapsedTime) override;
	virtual void uninitialize() override;

	virtual SceneType get_type() const override { return SceneType::RACING; }

private:
	std::unique_ptr<Player> player;
	std::unique_ptr<CameraController> cameraController;
	std::unique_ptr<skinned_mesh> carMesh;

	float totalTime = 0.0f;
};