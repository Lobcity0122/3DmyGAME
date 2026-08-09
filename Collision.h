#pragma once

#include <DirectXMath.h>
#include "skinned_mesh.h" // skinned_meshを使うためインクルード
#include "static_mesh.h"

namespace Collision
{
	// 自機のXZ方向AABBを、collision_mesh内の各オブジェクトAABBに対して移動判定する。
	// 壁を自機の半サイズぶん膨らませ、中心の移動線分との交差として計算する方式。
	bool SweepAABB2D(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end,
		const DirectX::XMFLOAT2& player_half_extent, const DirectX::XMFLOAT4X4& world_matrix,
		const static_mesh* collision_mesh, float& hit_fraction, DirectX::XMFLOAT3& hit_normal);

	// 曲がりたい方向へ進める通路中心を、壁OBJの面から探す。
	// current_direction と直交する座標だけを補正するので、進行方向にワープしない。
	bool FindTurnSnapPosition2D(const DirectX::XMFLOAT3& position,
		const DirectX::XMFLOAT2& requested_direction, const DirectX::XMFLOAT2& player_half_extent,
		const DirectX::XMFLOAT4X4& world_matrix, const static_mesh* collision_mesh,
		float maximum_snap_distance, DirectX::XMFLOAT3& snapped_position);

    // レイと単一の三角形の交差判定
    bool RayCastTriangle(
        const DirectX::XMFLOAT3& start,
        const DirectX::XMFLOAT3& end,
        const DirectX::XMFLOAT3& vertexA,
        const DirectX::XMFLOAT3& vertexB,
        const DirectX::XMFLOAT3& vertexC,
        DirectX::XMFLOAT3& hitPosition,
        DirectX::XMFLOAT3& hitNormal);

    // レイと skinned_mesh 全体（全ポリゴン）の交差判定
    bool RayCastSkinnedMesh(
        const DirectX::XMFLOAT3& start,
        const DirectX::XMFLOAT3& end,
        const DirectX::XMFLOAT4X4& worldMatrix,
        const skinned_mesh* model,
        DirectX::XMFLOAT3& hitPosition,
        DirectX::XMFLOAT3& hitNormal);

	// レイと static_mesh 全体（全ポリゴン）の交差判定
    bool RayCastStaticMesh(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end,
        const DirectX::XMFLOAT4X4& worldMatrix, const static_mesh* model,
        DirectX::XMFLOAT3& hitPosition, DirectX::XMFLOAT3& hitNormal);
}
