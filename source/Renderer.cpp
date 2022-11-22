//External includes
#include "SDL.h"
#include "SDL_surface.h"

//Project includes
#include "Renderer.h"
#include "Math.h"
#include "Matrix.h"
#include "Texture.h"
#include "Utils.h"

#include <iostream>

//#define USE_VERTEX_COLORS

using namespace dae;

Renderer::Renderer(SDL_Window* pWindow) :
	m_pWindow(pWindow)
{
	//Initialize
	SDL_GetWindowSize(pWindow, &m_Width, &m_Height);

	m_AspectRatio = static_cast<float>(m_Width) / static_cast<float>(m_Height);

	//Create Buffers
	m_pFrontBuffer = SDL_GetWindowSurface(pWindow);
	m_pBackBuffer = SDL_CreateRGBSurface(0, m_Width, m_Height, 32, 0, 0, 0, 0);
	m_pBackBufferPixels = (uint32_t*)m_pBackBuffer->pixels;

	m_pDepthBufferPixels = new float[m_Width * m_Height];
	ResetDepthBuffer();

	//Initialize Camera
	m_Camera.Initialize(60.f, { .0f,.0f,-10.f });

	//Initialize Texture
	m_pTexture = Texture::LoadFromFile("Resources/uv_grid_2.png");
}

Renderer::~Renderer()
{
	delete[] m_pDepthBufferPixels;
	delete m_pTexture;
	m_pTexture = nullptr;
}

void Renderer::Update(Timer* pTimer)
{
	m_Camera.Update(pTimer);
}

void Renderer::Render()
{
	//@START
	//Lock BackBuffer
	SDL_LockSurface(m_pBackBuffer);

	// Define Triangles - Vertices in WORLD space
	std::vector<Mesh> meshes_world
	{
		Mesh
		{
			{
				Vertex{{-3,3,-2},colors::White,{0,0}},
				Vertex{{0,3,-2},colors::White,{.5,0}},
				Vertex{{3,3,-2},colors::White,{1,0}},

				Vertex{{-3,0,-2},colors::White,{0,0.5}},
				Vertex{{0,0,-2},colors::White,{.5,.5}},
				Vertex{{3,0,-2},colors::White,{1,.5}},

				Vertex{{-3,-3,-2},colors::White,{0,1}},
				Vertex{{0,-3,-2},colors::White,{.5,1}},
				Vertex{{3,-3,-2},colors::White,{1,1}}
			},
			{
				3,0,4,1,5,2,
				2,6,
				6,3,7,4,8,5
			},
			PrimitiveTopology::TriangleStrip
			/*{
				3,0,1,	1,4,3,	4,1,2,
				2,5,4,	6,3,4,	4,7,6,
				7,4,5,	5,8,7
			},
			PrimitiveTopology::TriangleList*/
			
		}
	};

	// For each mesh
	for (const auto& mesh : meshes_world)
	{
		std::vector<Vertex> vertices_ndc{};
		// World space --> NDC Space
		VertexTransformationFunction(mesh.vertices, vertices_ndc);

		std::vector<Vector2> vertices_raster;
		for (const Vertex& ndcVertex : vertices_ndc)
		{
			// Formula from slides
			// NDC --> Screenspace
			vertices_raster.push_back({ (ndcVertex.position.x + 1) / 2.0f * m_Width, (1.0f - ndcVertex.position.y) / 2.0f * m_Height });
		}

		// Depth buffer
		ResetDepthBuffer();
		ClearBackground();

		// +--------------+
		// | RENDER LOGIC |
		// +--------------+
			switch (mesh.primitiveTopology)
			{
			case PrimitiveTopology::TriangleList:
				// For each triangle
				for (int currStartVertIdx{0}; currStartVertIdx < mesh.indices.size(); currStartVertIdx += 3)
				{
					RenderMeshTriangle(mesh, vertices_raster, vertices_ndc, currStartVertIdx, false);
				}
				break;
			case PrimitiveTopology::TriangleStrip:
				// For each triangle
				for (int currStartVertIdx{0}; currStartVertIdx < mesh.indices.size() - 2; ++currStartVertIdx)
				{
					RenderMeshTriangle(mesh, vertices_raster, vertices_ndc, currStartVertIdx, currStartVertIdx % 2);
				}
				break;
			default:
				std::cout << "PrimitiveTopology not implemented yet\n";
				break;
			}
	}

	//@END
	//Update SDL Surface
	SDL_UnlockSurface(m_pBackBuffer);
	SDL_BlitSurface(m_pBackBuffer, 0, m_pFrontBuffer, 0);
	SDL_UpdateWindowSurface(m_pWindow);
}

void Renderer::VertexTransformationFunction(const std::vector<Vertex>& vertices_in, std::vector<Vertex>& vertices_out) const
{
	vertices_out.reserve(vertices_in.size());

	// World vertices --transform to--> NDC

	// No const&, making a copy already
	for (Vertex vertex : vertices_in)
	{
		vertex.position = m_Camera.invViewMatrix.TransformPoint(vertex.position);

		vertex.position.x = (vertex.position.x / (m_AspectRatio * m_Camera.fov)) / vertex.position.z;
		vertex.position.y = (vertex.position.y / m_Camera.fov) / vertex.position.z;

		// Move instead of copy --> we made a copy already
		vertices_out.emplace_back(vertex);
	}
}
void Renderer::VertexTransformationFunction(const std::vector<Mesh>& meshes_in, std::vector<Mesh>& meshes_out) const
{
	meshes_out.reserve(meshes_in.size());
	
	// World vertices --transform to--> NDC

	for (const Mesh& mesh : meshes_in)
	{
		std::vector<Vertex> vertices_out;
		VertexTransformationFunction(mesh.vertices, vertices_out);
		meshes_out.emplace_back(
		Mesh{
				vertices_out,
				mesh.indices,
				mesh.primitiveTopology
			}
		);
	}
}

void dae::Renderer::RenderMeshTriangle(const Mesh& mesh, const std::vector<Vector2>& vertices_raster, const std::vector<Vertex>& vertices_ndc, int currStartVertIdx, bool swapVertices)
{
	const size_t vertIdx0{mesh.indices[currStartVertIdx + (2 * swapVertices)] };
	const size_t vertIdx1{ mesh.indices[currStartVertIdx + 1] };
	const size_t vertIdx2{ mesh.indices[currStartVertIdx + (!swapVertices * 2)] };

	// If a triangle has the same vertex twice, it means it has no surface and can't be rendered.
	if (vertIdx0 == vertIdx1 || vertIdx1 == vertIdx2 || vertIdx2 == vertIdx0)
	{
		return;
	}

	const Vector2 vert0{ vertices_raster[vertIdx0] };
	const Vector2 vert1{ vertices_raster[vertIdx1] };
	const Vector2 vert2{ vertices_raster[vertIdx2] };

	// Boundingbox (bb)
	Vector2 bbTopLeft{ Vector2::Min(vert0,Vector2::Min(vert1,vert2))};
	Vector2 bbBotRight{ Vector2::Max(vert0,Vector2::Max(vert1,vert2)) };

	const float margin{ 1.f };
	// Add margin to prevent seethrough lines between quads
	{
		const Vector2 marginVect{ margin,margin };
		bbTopLeft -= marginVect;
		bbBotRight += marginVect;
	}

	// Make sure the boundingbox is on the screen
	bbTopLeft.x = Clamp(bbTopLeft.x, 0.f, static_cast<float>(m_Width));
	bbTopLeft.y = Clamp(bbTopLeft.y, 0.f, static_cast<float>(m_Height));
	bbBotRight.x = Clamp(bbBotRight.x, 0.f, static_cast<float>(m_Width));
	bbBotRight.y = Clamp(bbBotRight.y, 0.f, static_cast<float>(m_Height));

	const int startX{ static_cast<int>(bbTopLeft.x) };
	const int endX{ static_cast<int>(bbBotRight.x) };
	const int startY{ static_cast<int>(bbTopLeft.y) };
	const int endY{ static_cast<int>(bbBotRight.y) };

	// For each pixel
	for (int px{ startX }; px < endX; ++px)
	{
		for (int py{ startY }; py < endY; ++py)
		{
			const Vector2 currentPixel{ static_cast<float>(px),static_cast<float>(py) };
			const int pixelIdx{ px + py * m_Width };
			// Cross products for weights go to waste, optimalisation is possible
			const bool hitTriangle{ Utils::IsInTriangle(currentPixel,vert0,vert1,vert2) };
			if (hitTriangle)
			{
				ColorRGB finalColor{};
				// weights
				float weight0, weight1, weight2;
				weight0 = Vector2::Cross((currentPixel - vert1), (vert1 - vert2));
				weight1 = Vector2::Cross((currentPixel - vert2), (vert2 - vert0));
				weight2 = Vector2::Cross((currentPixel - vert0), (vert0 - vert1));
				// divide by total triangle area
				const float totalTriangleArea{ Vector2::Cross(vert1 - vert0,vert2 - vert0) };
				const float invTotalTriangleArea{ 1 / totalTriangleArea };
				weight0 *= invTotalTriangleArea;
				weight1 *= invTotalTriangleArea;
				weight2 *= invTotalTriangleArea;

				const float depth0{ vertices_ndc[vertIdx0].position.z };
				const float depth1{ vertices_ndc[vertIdx1].position.z };
				const float depth2{ vertices_ndc[vertIdx2].position.z };
				const float interpolatedDepth{1.f / (weight0 * (1.f / depth0) + weight1 * (1.f / depth1) + weight2 * (1.f / depth2))};
				if (m_pDepthBufferPixels[pixelIdx] < interpolatedDepth) continue;

				m_pDepthBufferPixels[pixelIdx] = interpolatedDepth;

#ifdef USE_VERTEX_COLORS
				finalColor = { weight0 * mesh.vertices[vertIdx0].color + weight1 * mesh.vertices[vertIdx1].color + weight2 * mesh.vertices[vertIdx2].color };
#else 
				finalColor = m_pTexture->Sample(interpolatedDepth * ((weight0 * mesh.vertices[vertIdx0].uv) / depth0 + (weight1 * mesh.vertices[vertIdx1].uv) / depth1 + (weight2 * mesh.vertices[vertIdx2].uv) / depth2));
#endif
				//Update Color in Buffer
				finalColor.MaxToOne();

				m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
					static_cast<uint8_t>(finalColor.r * 255),
					static_cast<uint8_t>(finalColor.g * 255),
					static_cast<uint8_t>(finalColor.b * 255));
			}
		}
	}
}

bool Renderer::SaveBufferToImage() const
{
	return SDL_SaveBMP(m_pBackBuffer, "Rasterizer_ColorBuffer.bmp");
}
