#pragma once

#include "Scene.h"
#include "Player.h"
#include "CameraController.h"
#include "static_mesh.h"
#include "skinned_mesh.h"
#include <wrl.h>
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

	// static_mesh クラスに変更
	std::unique_ptr<static_mesh> carMesh;
	std::unique_ptr<skinned_mesh> characterMesh;

	struct SceneConstants
	{
		DirectX::XMFLOAT4X4 view_projection;
		DirectX::XMFLOAT4 light_direction;
		DirectX::XMFLOAT4 camera_position;
	};
	Microsoft::WRL::ComPtr<ID3D11Buffer> sceneConstantBuffer;
	DirectX::XMFLOAT4X4 characterWorld{};

	float totalTime = 0.0f;
};
