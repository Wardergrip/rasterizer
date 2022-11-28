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

	private:
		enum class RenderMode
		{
			Default, Depth, END
		};

		SDL_Window* m_pWindow{};

		SDL_Surface* m_pFrontBuffer{ nullptr };
		SDL_Surface* m_pBackBuffer{ nullptr };
		uint32_t* m_pBackBufferPixels{};

		float* m_pDepthBufferPixels{};

		Camera m_Camera{};

		int m_Width{};
		int m_Height{};

		Texture* m_pTexture;
		Mesh* m_pMesh;
		RenderMode m_RenderMode{ RenderMode::Default };

		bool m_F6Held{ false };

		//Function that transforms the vertices from the mesh from World space to Screen space
		void VertexTransformationFunction(const std::vector<Vertex>& vertices_in, std::vector<Vertex>& vertices_out) const; //W1 Version
		void VertexTransformationFunction(const std::vector<Mesh>& meshes_in, std::vector<Mesh>& meshes_out) const;
		void VertexTranformationFunction(Mesh& mesh);

		// SDL_FillRect(m_pBackBuffer, NULL, SDL_MapRGB(m_pBackBuffer->format, 100, 100, 100 ));
		inline void ClearBackground(){ SDL_FillRect(m_pBackBuffer, NULL, SDL_MapRGB(m_pBackBuffer->format, 100, 100, 100)); } const
	
		// std::fill_n(m_pDepthBufferPixels, (m_Width * m_Height), FLT_MAX);
		inline void ResetDepthBuffer() { std::fill_n(m_pDepthBufferPixels, (m_Width * m_Height), FLT_MAX); }

		void RenderMeshTriangle(const Mesh& mesh, const std::vector<Vector2>& vertices_raster, const std::vector<Vertex>& vertices_ndc, int currentVertexIdx, bool swapVertices);
	};
}
