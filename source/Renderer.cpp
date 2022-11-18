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
}

Renderer::~Renderer()
{
	delete[] m_pDepthBufferPixels;
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
	std::vector<Vertex> vertices_world
	{
		// Triangle 0
		{ { 0.0f, 2.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
		{ { 1.5f, -1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
		{ { -1.5f, -1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },

		// Triangle 1
		{ { 0.0f, 4.0f, 2.0f }, { 1.0f, 0.0f, 0.0f } },
		{ { 3.0f, -2.0f, 2.0f }, { 0.0f, 1.0f, 0.0f } },
		{ { -3.0f, -2.0f, 2.0f }, { 0.0f, 0.0f, 1.0f } }
	};

	std::vector<Vertex> vertices_ndc{};
	// World space --> NDC Space
	VertexTransformationFunction(vertices_world, vertices_ndc);

	std::vector<Vector2> verticesRaster;
	for (const Vertex& ndcVertex : vertices_ndc)
	{
		// Formula from slides
		// NDC --> Screenspace
		verticesRaster.push_back({ (ndcVertex.position.x + 1) / 2.0f * m_Width, (1.0f - ndcVertex.position.y) / 2.0f * m_Height });
	}

	// Depth buffer
	ResetDepthBuffer();
	ClearBackground();

	// +--------------+
	// | RENDER LOGIC |
	// +--------------+

	// For each triangle
	for (int currStartVertIdx{}; currStartVertIdx < vertices_world.size(); currStartVertIdx += 3)
	{ 
		// For each pixel
		for (int px{}; px < m_Width; ++px)
		{
			for (int py{}; py < m_Height; ++py)
			{
				const Vector2 currentPixel{ static_cast<float>(px),static_cast<float>(py) };
				const int pixelIdx{ px + py * m_Width };
				ColorRGB finalColor{};
				// Cross products go to waste, optimalisation is possible
				const bool hitTriangle{ Utils::IsInTriangle(currentPixel,verticesRaster[currStartVertIdx + 0],verticesRaster[currStartVertIdx + 1],verticesRaster[currStartVertIdx + 2]) };
				if (hitTriangle)
				{
					// weights
					float weight0, weight1, weight2;
					weight0 = Vector2::Cross((currentPixel - verticesRaster[currStartVertIdx + 1]),(verticesRaster[currStartVertIdx + 1]-verticesRaster[currStartVertIdx + 2]));
					weight1 = Vector2::Cross((currentPixel - verticesRaster[currStartVertIdx + 2]),(verticesRaster[currStartVertIdx + 2]-verticesRaster[currStartVertIdx + 0]));
					weight2 = Vector2::Cross((currentPixel - verticesRaster[currStartVertIdx + 0]),(verticesRaster[currStartVertIdx + 0]-verticesRaster[currStartVertIdx + 1]));
					// divide by total triangle area
					const float totalTriangleArea{ Vector2::Cross(verticesRaster[currStartVertIdx + 1]-verticesRaster[currStartVertIdx + 0],verticesRaster[currStartVertIdx + 2]-verticesRaster[currStartVertIdx + 0])};
					const float invTotalTriangleArea{ 1 / totalTriangleArea };
					weight0 *= invTotalTriangleArea;
					weight1 *= invTotalTriangleArea;
					weight2 *= invTotalTriangleArea;

					const Vector3 middleOfTriangle{weight0 * vertices_world[currStartVertIdx + 0].position + weight1 * vertices_world[currStartVertIdx + 1].position + weight2 * vertices_world[currStartVertIdx + 2].position};
					const float depthValue{(m_Camera.origin - middleOfTriangle).SqrMagnitude()};
					if (m_pDepthBufferPixels[pixelIdx] < depthValue) continue;

					m_pDepthBufferPixels[pixelIdx] = depthValue;
					
					finalColor = { weight0 * vertices_world[currStartVertIdx + 0].color + weight1 * vertices_world[currStartVertIdx + 1].color + weight2 * vertices_world[currStartVertIdx + 2].color};
				}
				//finalColor = {static_cast<float>(hitTriangle),static_cast<float>(hitTriangle),static_cast<float>(hitTriangle)};

				//Update Color in Buffer
				finalColor.MaxToOne();

				m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
					static_cast<uint8_t>(finalColor.r * 255),
					static_cast<uint8_t>(finalColor.g * 255),
					static_cast<uint8_t>(finalColor.b * 255));
			}
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

bool Renderer::SaveBufferToImage() const
{
	return SDL_SaveBMP(m_pBackBuffer, "Rasterizer_ColorBuffer.bmp");
}
