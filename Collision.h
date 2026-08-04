#pragma once

#include <DirectXMath.h>
#include "skinned_mesh.h" // skinned_meshを使うためインクルード

namespace Collision
{
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
}