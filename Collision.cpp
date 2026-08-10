#include "Collision.h"
#include <DirectXCollision.h>
#include <cfloat>
#include <algorithm>
#include <cmath>
#include <vector>

namespace Collision
{
	bool SweepAABB2D(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end,
		const DirectX::XMFLOAT2& player_half_extent, const DirectX::XMFLOAT4X4& world_matrix,
		const static_mesh* collision_mesh, float& hit_fraction, DirectX::XMFLOAT3& hit_normal)
	{
		if (collision_mesh == nullptr) return false;
		const DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&world_matrix);
		const float direction_x = end.x - start.x;
		const float direction_z = end.z - start.z;
		bool hit = false;
		hit_fraction = 1.0f;
		hit_normal = {};

		for (const static_mesh::bounding_box& local_box : collision_mesh->get_object_bounding_boxes())
		{
			// 回転・拡縮されたモデルにも対応するため、8頂点を変換してワールドAABBを作る。
			DirectX::XMFLOAT3 world_min{ FLT_MAX, FLT_MAX, FLT_MAX };
			DirectX::XMFLOAT3 world_max{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
			for (int x = 0; x < 2; ++x)
			{
				for (int y = 0; y < 2; ++y)
				{
					for (int z = 0; z < 2; ++z)
					{
						const DirectX::XMFLOAT3 local_point{
							x == 0 ? local_box.minimum.x : local_box.maximum.x,
							y == 0 ? local_box.minimum.y : local_box.maximum.y,
							z == 0 ? local_box.minimum.z : local_box.maximum.z };
						DirectX::XMFLOAT3 world_point{};
						DirectX::XMStoreFloat3(&world_point,
							DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&local_point), world));
						world_min.x = (std::min)(world_min.x, world_point.x);
						world_min.z = (std::min)(world_min.z, world_point.z);
						world_max.x = (std::max)(world_max.x, world_point.x);
						world_max.z = (std::max)(world_max.z, world_point.z);
					}
				}
			}

			const float minimum_x = world_min.x - player_half_extent.x;
			const float maximum_x = world_max.x + player_half_extent.x;
			const float minimum_z = world_min.z - player_half_extent.y;
			const float maximum_z = world_max.z + player_half_extent.y;
			const bool starts_inside = start.x >= minimum_x && start.x <= maximum_x &&
				start.z >= minimum_z && start.z <= maximum_z;
			if (starts_inside)
			{
				// 接触面に残った状態では、壁に平行な移動まで止めない。
				// 最も近い脱出面の法線を求め、そこへ向かう（めり込む）移動だけを衝突にする。
				const float distances[] = {
					start.x - minimum_x, maximum_x - start.x,
					start.z - minimum_z, maximum_z - start.z };
				const DirectX::XMFLOAT3 normals[] = {
					{ -1, 0, 0 }, { 1, 0, 0 }, { 0, 0, -1 }, { 0, 0, 1 } };
				int nearest_face = 0;
				for (int index = 1; index < 4; ++index)
				{
					if (distances[index] < distances[nearest_face]) nearest_face = index;
				}
				const float normal_dot_move = normals[nearest_face].x * direction_x + normals[nearest_face].z * direction_z;
				if (normal_dot_move >= 0.0f) continue;
				hit = true;
				hit_fraction = 0.0f;
				hit_normal = normals[nearest_face];
				continue;
			}
			float enter = 0.0f;
			float leave = 1.0f;
			DirectX::XMFLOAT3 normal{};
			const auto update_slab = [&enter, &leave, &normal](float origin, float direction, float minimum, float maximum,
				const DirectX::XMFLOAT3& minimum_normal, const DirectX::XMFLOAT3& maximum_normal)
			{
				if (std::fabs(direction) < 0.000001f) return origin < minimum || origin > maximum ? false : true;
				float first = (minimum - origin) / direction;
				float second = (maximum - origin) / direction;
				DirectX::XMFLOAT3 first_normal = minimum_normal;
				if (first > second)
				{
					std::swap(first, second);
					first_normal = maximum_normal;
				}
				if (first >= enter)
				{
					enter = first;
					normal = first_normal;
				}
				leave = (std::min)(leave, second);
				return enter <= leave;
			};
			if (!update_slab(start.x, direction_x, minimum_x, maximum_x, { -1, 0, 0 }, { 1, 0, 0 })) continue;
			if (!update_slab(start.z, direction_z, minimum_z, maximum_z, { 0, 0, -1 }, { 0, 0, 1 })) continue;
			if (enter >= 0.0f && enter <= 1.0f && enter < hit_fraction)
			{
				hit = true;
				hit_fraction = enter;
				hit_normal = normal;
			}
		}
		return hit;
	}

	bool FindTurnSnapPosition2D(const DirectX::XMFLOAT3& position,
		const DirectX::XMFLOAT2& requested_direction, const DirectX::XMFLOAT2& player_half_extent,
		const DirectX::XMFLOAT4X4& world_matrix, const static_mesh* collision_mesh,
		float maximum_snap_distance, DirectX::XMFLOAT3& snapped_position)
	{
		if (collision_mesh == nullptr) return false;

		// 横へ曲がる時はZ、縦へ曲がる時はXを通路中心へ合わせる。
		const bool turn_along_x = std::fabs(requested_direction.x) > 0.5f;
		std::vector<float> wall_faces;
		// { minX, maxX, minZ, maxZ }。候補中心そのものが壁の中でないことも確認する。
		std::vector<DirectX::XMFLOAT4> world_wall_bounds;
		const DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&world_matrix);
		for (const static_mesh::bounding_box& local_box : collision_mesh->get_object_bounding_boxes())
		{
			float minimum = FLT_MAX;
			float maximum = -FLT_MAX;
			float minimum_x = FLT_MAX, maximum_x = -FLT_MAX;
			float minimum_z = FLT_MAX, maximum_z = -FLT_MAX;
			for (int x = 0; x < 2; ++x)
				for (int y = 0; y < 2; ++y)
					for (int z = 0; z < 2; ++z)
					{
						const DirectX::XMFLOAT3 local_point{
							x == 0 ? local_box.minimum.x : local_box.maximum.x,
							y == 0 ? local_box.minimum.y : local_box.maximum.y,
							z == 0 ? local_box.minimum.z : local_box.maximum.z };
						DirectX::XMFLOAT3 world_point{};
						DirectX::XMStoreFloat3(&world_point,
							DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&local_point), world));
						const float value = turn_along_x ? world_point.z : world_point.x;
						minimum = (std::min)(minimum, value);
						maximum = (std::max)(maximum, value);
						minimum_x = (std::min)(minimum_x, world_point.x);
						maximum_x = (std::max)(maximum_x, world_point.x);
						minimum_z = (std::min)(minimum_z, world_point.z);
						maximum_z = (std::max)(maximum_z, world_point.z);
					}
			wall_faces.push_back(minimum);
			wall_faces.push_back(maximum);
			world_wall_bounds.push_back({ minimum_x, maximum_x, minimum_z, maximum_z });
		}
		std::sort(wall_faces.begin(), wall_faces.end());

		const float current_axis = turn_along_x ? position.z : position.x;
		float best_distance = maximum_snap_distance;
		bool found = false;
		for (size_t index = 1; index < wall_faces.size(); ++index)
		{
			// 隣り合う壁面の中点が、通路の中心候補になる。
			const float candidate = (wall_faces[index - 1] + wall_faces[index]) * 0.5f;
			const float distance = std::fabs(candidate - current_axis);
			if (distance > best_distance) continue;

			DirectX::XMFLOAT3 candidate_position = position;
			if (turn_along_x) candidate_position.z = candidate;
			else candidate_position.x = candidate;
			bool candidate_overlaps_wall = false;
			for (const DirectX::XMFLOAT4& wall : world_wall_bounds)
			{
				if (candidate_position.x > wall.x - player_half_extent.x && candidate_position.x < wall.y + player_half_extent.x &&
					candidate_position.z > wall.z - player_half_extent.y && candidate_position.z < wall.w + player_half_extent.y)
				{
					candidate_overlaps_wall = true;
					break;
				}
			}
			if (candidate_overlaps_wall) continue;
			// 中心へ合わせた後、曲がる方向へ自機半径以上進める場所だけを採用する。
			DirectX::XMFLOAT3 check_end = candidate_position;
			const float check_distance = (std::max)(0.25f,
				turn_along_x ? player_half_extent.x * 2.0f : player_half_extent.y * 2.0f);
			check_end.x += requested_direction.x * check_distance;
			check_end.z += requested_direction.y * check_distance;
			float hit_fraction = 1.0f;
			DirectX::XMFLOAT3 hit_normal{};
			if (!SweepAABB2D(candidate_position, check_end, player_half_extent,
				world_matrix, collision_mesh, hit_fraction, hit_normal))
			{
				best_distance = distance;
				snapped_position = candidate_position;
				found = true;
			}
		}
		return found;
	}

	bool IsAABBBlocked2D(const DirectX::XMFLOAT3& position,
		const DirectX::XMFLOAT2& player_half_extent, const DirectX::XMFLOAT4X4& world_matrix,
		const static_mesh* collision_mesh)
	{
		if (collision_mesh == nullptr) return false;
		const DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&world_matrix);
		for (const static_mesh::bounding_box& local_box : collision_mesh->get_object_bounding_boxes())
		{
			float minimum_x = FLT_MAX, maximum_x = -FLT_MAX;
			float minimum_z = FLT_MAX, maximum_z = -FLT_MAX;
			for (int x = 0; x < 2; ++x)
				for (int y = 0; y < 2; ++y)
					for (int z = 0; z < 2; ++z)
					{
						const DirectX::XMFLOAT3 local_point{
							x == 0 ? local_box.minimum.x : local_box.maximum.x,
							y == 0 ? local_box.minimum.y : local_box.maximum.y,
							z == 0 ? local_box.minimum.z : local_box.maximum.z };
						DirectX::XMFLOAT3 world_point{};
						DirectX::XMStoreFloat3(&world_point,
							DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&local_point), world));
						minimum_x = (std::min)(minimum_x, world_point.x);
						maximum_x = (std::max)(maximum_x, world_point.x);
						minimum_z = (std::min)(minimum_z, world_point.z);
						maximum_z = (std::max)(maximum_z, world_point.z);
					}
			if (position.x > minimum_x - player_half_extent.x && position.x < maximum_x + player_half_extent.x &&
				position.z > minimum_z - player_half_extent.y && position.z < maximum_z + player_half_extent.y)
				return true;
		}
		return false;
	}

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

	bool RayCastStaticMesh(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end,
		const DirectX::XMFLOAT4X4& worldMatrix, const static_mesh* model,
		DirectX::XMFLOAT3& hitPosition, DirectX::XMFLOAT3& hitNormal)
	{
		if (!model) return false;
		const auto& vertices = model->get_cpu_vertices();
		const auto& indices = model->get_cpu_indices();
		const DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&worldMatrix);
		const DirectX::XMVECTOR ray_start = DirectX::XMLoadFloat3(&start);
		bool hit = false;
		float closest_distance = FLT_MAX;

		for (size_t i = 0; i + 2 < indices.size(); i += 3)
		{
			DirectX::XMVECTOR p0 = DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&vertices[indices[i]].position), world);
			DirectX::XMVECTOR p1 = DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&vertices[indices[i + 1]].position), world);
			DirectX::XMVECTOR p2 = DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&vertices[indices[i + 2]].position), world);
			DirectX::XMFLOAT3 v0{}, v1{}, v2{}, temporary_position{}, temporary_normal{};
			DirectX::XMStoreFloat3(&v0, p0);
			DirectX::XMStoreFloat3(&v1, p1);
			DirectX::XMStoreFloat3(&v2, p2);
			if (RayCastTriangle(start, end, v0, v1, v2, temporary_position, temporary_normal))
			{
				const float distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(
					DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&temporary_position), ray_start)));
				if (distance < closest_distance)
				{
					closest_distance = distance;
					hitPosition = temporary_position;
					hitNormal = temporary_normal;
					hit = true;
				}
			}
		}
		return hit;
	}
}
