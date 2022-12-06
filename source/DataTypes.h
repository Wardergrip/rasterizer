#pragma once
#include "Math.h"
#include "vector"

namespace dae
{
	struct Vertex
	{
		Vector3 position{};
		ColorRGB color{ colors::White };
		Vector2 uv{}; //W3
		Vector3 normal{}; //W4
		Vector3 tangent{}; //W4
		Vector3 viewDirection{}; //W4
	};

	struct Vertex_Out
	{
		Vector4 position{};
		ColorRGB color{ colors::White };
		Vector2 uv{};
		Vector3 normal{};
		Vector3 tangent{};
		Vector3 viewDirection{};
	};

	enum class PrimitiveTopology
	{
		TriangleList,
		TriangleStrip
	};

	struct Mesh
	{
		std::vector<Vertex> vertices{};
		std::vector<uint32_t> indices{};
		PrimitiveTopology primitiveTopology{ PrimitiveTopology::TriangleList };

		std::vector<Vertex_Out> vertices_out{};
		Matrix worldMatrix{};

		inline void RotateY(float angle)
		{
			worldMatrix = Matrix::CreateRotationY(angle * TO_RADIANS) * worldMatrix;
		}

		inline void RotateX(float angle)
		{
			worldMatrix = Matrix::CreateRotationX(angle * TO_RADIANS) * worldMatrix;
		}

		inline void RotateZ(float angle)
		{
			worldMatrix = Matrix::CreateRotationZ(angle * TO_RADIANS) * worldMatrix;
		}

		inline void Translate(float x, float y, float z)
		{
			worldMatrix = Matrix::CreateTranslation(x, y, z) * worldMatrix;
		}

		inline void Translate(const Vector3& v)
		{
			worldMatrix = Matrix::CreateTranslation(v) * worldMatrix;
		}
	};

	struct DirectionalLight
	{
		Vector3 direction{};
		float intensity{};
	};
}