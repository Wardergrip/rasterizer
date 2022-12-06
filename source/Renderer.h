#pragma once

#include <cstdint>
#include <vector>

#include "Camera.h"
#include "DataTypes.h"

struct SDL_Window;
struct SDL_Surface;

namespace dae
{
	class Texture;
	struct Mesh;
	struct Vertex;
	class Timer;
	class Scene;

	class Renderer final
	{
	public:
		Renderer(SDL_Window* pWindow);
		~Renderer();

		Renderer(const Renderer&) = delete;
		Renderer(Renderer&&) noexcept = delete;
		Renderer& operator=(const Renderer&) = delete;
		Renderer& operator=(Renderer&&) noexcept = delete;

		void Update(Timer* pTimer);
		void Render();

		bool SaveBufferToImage() const;

		inline void NextRenderMode()
		{
			m_RenderMode = static_cast<RenderMode>((static_cast<int>(m_RenderMode) + 1) % (static_cast<int>(RenderMode::END)));
		}
		
		inline void NextShadeMode()
		{
			m_ShadingMode = static_cast<ShadingMode>((static_cast<int>(m_ShadingMode) + 1) % (static_cast<int>(ShadingMode::END)));
		}

	private:
		enum class RenderMode
		{
			Default, Depth, END
		};
		enum class ShadingMode
		{
			ObservedArea,	// (OA)
			Diffuse,		// (incl OA)
			Specular,		// (incl OA)
			Combined,
			END
		};

		SDL_Window* m_pWindow{};

		SDL_Surface* m_pFrontBuffer{ nullptr };
		SDL_Surface* m_pBackBuffer{ nullptr };
		uint32_t* m_pBackBufferPixels{};

		float* m_pDepthBufferPixels{};

		Camera m_Camera{};

		int m_Width{};
		int m_Height{};

		Texture* m_pDiffuseTexture;
		Texture* m_pSpecularTexture;
		Texture* m_pGlossinessTexture;
		Texture* m_pNormalTexture;
		Mesh* m_pMesh;
		RenderMode m_RenderMode{ RenderMode::Default };
		ShadingMode m_ShadingMode{ ShadingMode::Combined };

		const DirectionalLight m_GlobalLight{ Vector3{ .577f,-.557f,.577f }.Normalized() , 7.f};
		const float m_SpecularShininess{ 25.0f };
		const ColorRGB m_AmbientColor{ 0.025f, 0.025f, 0.025f };

		bool m_EnableRotating{ true };
		bool m_EnableNormalMap{ true };
		// Toggle depth
		bool m_F4Held{ false };
		// Toggle rotation
		bool m_F5Held{ false };
		// Toggle normal map
		bool m_F6Held{ false };
		// Cycle shading mode
		bool m_F7Held{ false };

		//Function that transforms the vertices from the mesh from World space to Screen space
		void VertexTransformationFunction(Mesh& mesh);

		// SDL_FillRect(m_pBackBuffer, NULL, SDL_MapRGB(m_pBackBuffer->format, 100, 100, 100 ));
		inline void ClearBackground(){ SDL_FillRect(m_pBackBuffer, NULL, SDL_MapRGB(m_pBackBuffer->format, 100, 100, 100)); } const
	
		// std::fill_n(m_pDepthBufferPixels, (m_Width * m_Height), FLT_MAX);
		inline void ResetDepthBuffer() { std::fill_n(m_pDepthBufferPixels, (m_Width * m_Height), FLT_MAX); }

		void RenderMeshTriangle(const Mesh& mesh, const std::vector<Vector2>& vertices_raster, int currentVertexIdx, bool swapVertices);

		void PixelShading(const Vertex_Out& v);
	};
}
