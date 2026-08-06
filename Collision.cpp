#include "Collision.h"
#include <DirectXCollision.h>
#include <cfloat>

namespace Collision
{
	// 三角形に対するレイキャスト判定関数
    bool RayCastTriangle(
        const DirectX::XMFLOAT3& start,
        const DirectX::XMFLOAT3& end,
        const DirectX::XMFLOAT3& vertexA,
        const DirectX::XMFLOAT3& vertexB,
        const DirectX::XMFLOAT3& vertexC,
        DirectX::XMFLOAT3& hitPosition,
        DirectX::XMFLOAT3& hitNormal)
    {
        using namespace DirectX;

        XMVECTOR Start = XMLoadFloat3(&start);
        XMVECTOR End = XMLoadFloat3(&end);
        XMVECTOR Vec = XMVectorSubtract(End, Start);
        XMVECTOR Direction = XMVector3Normalize(Vec);
        XMVECTOR Length = XMVector3Length(Vec);
        float distance = XMVectorGetX(Length);

        XMVECTOR A = XMLoadFloat3(&vertexA);
        XMVECTOR B = XMLoadFloat3(&vertexB);
        XMVECTOR C = XMLoadFloat3(&vertexC);

        float hitDist = 0.0f;
        // DirectXCollision の交差判定
        if (TriangleTests::Intersects(Start, Direction, A, B, C, hitDist))
        {
            // 移動レイの範囲内（distance以内）で衝突しているか
            if (hitDist <= distance)
            {
                // 衝突位置の算出
                XMVECTOR HitPos = XMVectorAdd(Start, XMVectorScale(Direction, hitDist));
                XMStoreFloat3(&hitPosition, HitPos);

                // 三角形の法線ベクトルの算出
                XMVECTOR AB = XMVectorSubtract(B, A);
                XMVECTOR BC = XMVectorSubtract(C, B);
                XMVECTOR N = XMVector3Normalize(XMVector3Cross(AB, BC));
                XMStoreFloat3(&hitNormal, N);

                return true;
            }
        }
        return false;
    }

	// skinned_mesh に対するレイキャスト判定関数
    bool RayCastSkinnedMesh(
        const DirectX::XMFLOAT3& start,
        const DirectX::XMFLOAT3& end,
        const DirectX::XMFLOAT4X4& worldMatrix,
        const skinned_mesh* model,
        DirectX::XMFLOAT3& hitPosition,
        DirectX::XMFLOAT3& hitNormal)
    {
        if (!model) return false;

        bool isHit = false;
        float closestDistance = FLT_MAX;
        DirectX::XMFLOAT3 tempHitPos, tempHitNormal;
        DirectX::XMVECTOR S = DirectX::XMLoadFloat3(&start);

        // モデル全体のワールド行列
        DirectX::XMMATRIX W = DirectX::XMLoadFloat4x4(&worldMatrix);

        // 1. skinned_mesh が持つすべてのメッシュ（パーツ）をループ
        for (const auto& mesh : model->GetMeshes())
        {
            // 2. ワールド行列のみを使用（default_global_transformを無視）
            DirectX::XMMATRIX finalTransform = W;

            // 3. メッシュ内のすべてのポリゴン（三角形）をループ
            for (size_t i = 0; i < mesh.cpu_indices.size(); i += 3)
            {
                DirectX::XMVECTOR p0 = DirectX::XMLoadFloat3(&mesh.cpu_vertices[mesh.cpu_indices[i]].position);
                DirectX::XMVECTOR p1 = DirectX::XMLoadFloat3(&mesh.cpu_vertices[mesh.cpu_indices[i + 1]].position);
                DirectX::XMVECTOR p2 = DirectX::XMLoadFloat3(&mesh.cpu_vertices[mesh.cpu_indices[i + 2]].position);

                // ワールド空間の座標に変換
                p0 = DirectX::XMVector3TransformCoord(p0, finalTransform);
                p1 = DirectX::XMVector3TransformCoord(p1, finalTransform);
                p2 = DirectX::XMVector3TransformCoord(p2, finalTransform);

                DirectX::XMFLOAT3 v0, v1, v2;
                DirectX::XMStoreFloat3(&v0, p0);
                DirectX::XMStoreFloat3(&v1, p1);
                DirectX::XMStoreFloat3(&v2, p2);

                // 三角形とのレイキャスト判定
                if (RayCastTriangle(start, end, v0, v1, v2, tempHitPos, tempHitNormal))
                {
                    // 交点までの距離を測り、一番近いものを記録
                    DirectX::XMVECTOR P = DirectX::XMLoadFloat3(&tempHitPos);
                    float dist = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(P, S)));


					// 一番近い交点を更新
                    if (dist < closestDistance)
                    {
                        closestDistance = dist;
                        hitPosition = tempHitPos;
                        hitNormal = tempHitNormal;
                        isHit = true;
                    }
                }
            }
        }
        return isHit;
    }
}