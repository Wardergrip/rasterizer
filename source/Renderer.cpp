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

	//Create Buffers
	m_pFrontBuffer = SDL_GetWindowSurface(pWindow);
	m_pBackBuffer = SDL_CreateRGBSurface(0, m_Width, m_Height, 32, 0, 0, 0, 0);
	m_pBackBufferPixels = (uint32_t*)m_pBackBuffer->pixels;

	m_pDepthBufferPixels = new float[m_Width * m_Height];
	ResetDepthBuffer();

	//Initialize Camera
	m_Camera.Initialize(45.f, { .0f,.0f,.0f }, static_cast<float>(m_Width) / m_Height);

	//Initialize Texture
	m_pDiffuseTexture = Texture::LoadFromFile("Resources/vehicle_diffuse.png");
	m_pSpecularTexture = Texture::LoadFromFile("Resources/vehicle_specular.png");
	m_pGlossinessTexture = Texture::LoadFromFile("Resources/vehicle_gloss.png");
	m_pNormalTexture = Texture::LoadFromFile("Resources/vehicle_normal.png");

	//Initialize Mesh
	m_pMesh = new Mesh();
	Utils::ParseOBJ("Resources/vehicle.obj", m_pMesh->vertices, m_pMesh->indices);
	m_pMesh->Translate(0.f, 0.f, 50.f);
}

Renderer::~Renderer()
{
	delete[] m_pDepthBufferPixels;
	delete m_pDiffuseTexture;
	m_pDiffuseTexture = nullptr;
	delete m_pSpecularTexture;
	m_pSpecularTexture = nullptr;
	delete m_pGlossinessTexture;
	m_pGlossinessTexture = nullptr;
	delete m_pNormalTexture;
	m_pNormalTexture = nullptr;
	delete m_pMesh;
	m_pMesh = nullptr;
}

void Renderer::Update(Timer* pTimer)
{
	m_Camera.Update(pTimer);

	constexpr const float rotationSpeed{ 30.f };
	if (m_EnableRotating) m_pMesh->RotateY(rotationSpeed * pTimer->GetElapsed());

	const uint8_t* pKeyboardState = SDL_GetKeyboardState(nullptr);

	if (pKeyboardState[SDL_SCANCODE_F4])
	{
		if (!m_F4Held)
		{
			NextRenderMode();
			std::cout << "[RENDERMODE] ";
			switch (m_RenderMode)
			{
			case dae::Renderer::RenderMode::Default:
				std::cout << "Default\n";
				break;
			case dae::Renderer::RenderMode::Depth:
				std::cout << "Depth\n";
				break;
			}
		}
		m_F4Held = true;
	}
	else m_F4Held = false;
	if (pKeyboardState[SDL_SCANCODE_F5])
	{
		if (!m_F5Held)
		{
			m_EnableRotating = !m_EnableRotating;
			std::cout << "[ROT] ";
			std::cout << (m_EnableRotating ? "Rotation enabled\n" : "Rotation disabled\n");
		}
		m_F5Held = true;
	}
	else m_F5Held = false;
	if (pKeyboardState[SDL_SCANCODE_F6])
	{
		if (!m_F6Held)
		{
			m_EnableNormalMap = !m_EnableNormalMap;
			std::cout << "[NOR] ";
			std::cout << (m_EnableNormalMap ? "NormalMap enabled\n" : "NormalMap disabled\n");
		}
		m_F6Held = true;
	}
	else m_F6Held = false;
	if (pKeyboardState[SDL_SCANCODE_F7])
	{
		if (!m_F7Held)
		{
			NextShadeMode();
			std::cout << "[SHADINGMODE] ";
			switch (m_ShadingMode)
			{
			case dae::Renderer::ShadingMode::ObservedArea:
				std::cout << "ObservedArea\n";
				break;
			case dae::Renderer::ShadingMode::Diffuse:
				std::cout << "Diffuse\n";
				break;
			case dae::Renderer::ShadingMode::Specular:
				std::cout << "Specular\n";
				break;
			case dae::Renderer::ShadingMode::Combined:
				std::cout << "Combined\n";
				break;
			}
		}
		m_F7Held = true;
	}
	else m_F7Held = false;
}

void Renderer::Render()
{
	//@START
	//Lock BackBuffer
	SDL_LockSurface(m_pBackBuffer);

	// Define Triangles - Vertices in WORLD space
	std::vector<Mesh> meshes_world;
	// can be optimised
	meshes_world.push_back(*m_pMesh);
	//{
	//	Mesh
	//	{
	//		{
	//			Vertex{{-3,3,-2},colors::White,{0,0}},
	//			Vertex{{0,3,-2},colors::White,{.5,0}},
	//			Vertex{{3,3,-2},colors::White,{1,0}},
	//
	//			Vertex{{-3,0,-2},colors::White,{0,0.5}},
	//			Vertex{{0,0,-2},colors::White,{.5,.5}},
	//			Vertex{{3,0,-2},colors::White,{1,.5}},
	//
	//			Vertex{{-3,-3,-2},colors::White,{0,1}},
	//			Vertex{{0,-3,-2},colors::White,{.5,1}},
	//			Vertex{{3,-3,-2},colors::White,{1,1}}
	//		},
	//		{
	//			3,0,4,1,5,2,
	//			2,6,
	//			6,3,7,4,8,5
	//		},
	//		PrimitiveTopology::TriangleStrip
	//		/*{
	//			3,0,1,	1,4,3,	4,1,2,
	//			2,5,4,	6,3,4,	4,7,6,
	//			7,4,5,	5,8,7
	//		},
	//		PrimitiveTopology::TriangleList*/
	//		
	//	}
	//};

	// For each mesh
	for (auto& mesh : meshes_world)
	{
		// World space --> NDC Space
		VertexTransformationFunction(mesh);

		std::vector<Vector2> vertices_raster;
		for (const Vertex_Out& ndcVertex : mesh.vertices_out)
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
					RenderMeshTriangle(mesh, vertices_raster, currStartVertIdx, false);
				}
				break;
			case PrimitiveTopology::TriangleStrip:
				// For each triangle
				for (int currStartVertIdx{0}; currStartVertIdx < mesh.indices.size() - 2; ++currStartVertIdx)
				{
					RenderMeshTriangle(mesh, vertices_raster, currStartVertIdx, currStartVertIdx % 2);
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

void dae::Renderer::VertexTransformationFunction(Mesh& mesh)
{
	Matrix worldViewProjectionMatrix{ mesh.worldMatrix * m_Camera.viewMatrix * m_Camera.projectionMatrix };
	mesh.vertices_out.clear();
	mesh.vertices_out.reserve(mesh.vertices.size());

	for (const Vertex& v : mesh.vertices)
	{
		Vertex_Out vertex_out{ Vector4{}, v.color, v.uv, v.normal, v.tangent };

		vertex_out.position = worldViewProjectionMatrix.TransformPoint({ v.position, 1.0f });
		vertex_out.viewDirection = Vector3{ vertex_out.position.x, vertex_out.position.y, vertex_out.position.z }.Normalized();

		vertex_out.normal = mesh.worldMatrix.TransformVector(v.normal);
		vertex_out.tangent = mesh.worldMatrix.TransformVector(v.tangent);


		const float invVw{1/vertex_out.position.w};
		vertex_out.position.x *= invVw;
		vertex_out.position.y *= invVw;
		vertex_out.position.z *= invVw;

		// emplace back because we made vOut just to store in this vector
		mesh.vertices_out.emplace_back(vertex_out);
	}
}

void dae::Renderer::RenderMeshTriangle(const Mesh& mesh, const std::vector<Vector2>& vertices_raster, int currStartVertIdx, bool swapVertices)
{
	const size_t vertIdx0{mesh.indices[currStartVertIdx + (2 * swapVertices)] };
	const size_t vertIdx1{ mesh.indices[currStartVertIdx + 1] };
	const size_t vertIdx2{ mesh.indices[currStartVertIdx + (!swapVertices * 2)] };

	// If a triangle has the same vertex twice, it means it has no surface and can't be rendered.
	if (vertIdx0 == vertIdx1 || vertIdx1 == vertIdx2 || vertIdx2 == vertIdx0)
	{
		return;
	}
	if (m_Camera.ShouldVertexBeClipped(mesh.vertices_out[vertIdx0].position) || m_Camera.ShouldVertexBeClipped(mesh.vertices_out[vertIdx1].position) || m_Camera.ShouldVertexBeClipped(mesh.vertices_out[vertIdx2].position))
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

				const float depth0{ mesh.vertices_out[vertIdx0].position.z };
				const float depth1{ mesh.vertices_out[vertIdx1].position.z };
				const float depth2{ mesh.vertices_out[vertIdx2].position.z };
				const float interpolatedDepth{1.f / (weight0 * (1.f / depth0) + weight1 * (1.f / depth1) + weight2 * (1.f / depth2))};
				if (m_pDepthBufferPixels[pixelIdx] < interpolatedDepth || interpolatedDepth < 0.f || interpolatedDepth > 1.f) continue;

				m_pDepthBufferPixels[pixelIdx] = interpolatedDepth;

				Vertex_Out pixel{};
				pixel.position = { currentPixel.x,currentPixel.y, interpolatedDepth,interpolatedDepth };
				pixel.uv = interpolatedDepth * ((weight0 * mesh.vertices[vertIdx0].uv) / depth0 + (weight1 * mesh.vertices[vertIdx1].uv) / depth1 + (weight2 * mesh.vertices[vertIdx2].uv) / depth2);
				pixel.normal = Vector3{ interpolatedDepth * (weight0 * mesh.vertices_out[vertIdx0].normal / mesh.vertices_out[vertIdx0].position.w + weight1 * mesh.vertices_out[vertIdx1].normal / mesh.vertices_out[vertIdx1].position.w + weight2 * mesh.vertices_out[vertIdx2].normal / mesh.vertices_out[vertIdx2].position.w)}.Normalized();
				pixel.tangent = Vector3{ interpolatedDepth * (weight0 * mesh.vertices_out[vertIdx0].tangent / mesh.vertices_out[vertIdx0].position.w + weight1 * mesh.vertices_out[vertIdx1].tangent / mesh.vertices_out[vertIdx1].position.w + weight2 * mesh.vertices_out[vertIdx2].tangent / mesh.vertices_out[vertIdx2].position.w)}.Normalized();
				pixel.viewDirection = Vector3{interpolatedDepth * (weight0 * mesh.vertices_out[vertIdx0].viewDirection / mesh.vertices_out[vertIdx0].position.w +weight1 * mesh.vertices_out[vertIdx1].viewDirection / mesh.vertices_out[vertIdx1].position.w +weight2 * mesh.vertices_out[vertIdx2].viewDirection / mesh.vertices_out[vertIdx2].position.w)}.Normalized();

				PixelShading(pixel);
			}
		}
	}
}

void dae::Renderer::PixelShading(const Vertex_Out& v)
{
	Vector3 normal{v.normal};

	ColorRGB finalColor{};

	if (m_EnableNormalMap)
	{
		const Vector3 binormal = Vector3::Cross(v.normal, v.tangent);
		const Matrix tangentSpaceAxis = Matrix{ v.tangent,binormal,v.normal,Vector3::Zero };

		const ColorRGB normalSampleVecCol{ (2 * m_pNormalTexture->Sample(v.uv)) - ColorRGB{1,1,1} };
		const Vector3 normalSampleVec{ normalSampleVecCol.r,normalSampleVecCol.g,normalSampleVecCol.b };
		normal = tangentSpaceAxis.TransformVector(normalSampleVec);
	}

	switch (m_RenderMode)
	{
	case dae::Renderer::RenderMode::Default:
	{
		const float observedArea{ Vector3::DotClamp(normal.Normalized(), -m_GlobalLight.direction)};
		finalColor = m_pDiffuseTexture->Sample(v.uv);
		const ColorRGB lambert{ BRDF::Lambert(1.0f, m_pDiffuseTexture->Sample(v.uv)) };
		const float specularVal{ m_SpecularShininess * m_pGlossinessTexture->Sample(v.uv).r };
		const ColorRGB specular{ m_pSpecularTexture->Sample(v.uv) * BRDF::Phong(1.0f, specularVal, -m_GlobalLight.direction, v.viewDirection, normal) };

		// += since finalColor is already a sample of the diffuse texture
		switch (m_ShadingMode)
		{
		case dae::Renderer::ShadingMode::ObservedArea:
			finalColor = ColorRGB{ observedArea, observedArea, observedArea };
			break;
		case dae::Renderer::ShadingMode::Diffuse:
			finalColor += m_GlobalLight.intensity * observedArea * lambert;
			break;
		case dae::Renderer::ShadingMode::Specular:
			finalColor += specular * observedArea;
			break;
		case dae::Renderer::ShadingMode::Combined:
			finalColor += m_GlobalLight.intensity * observedArea * lambert + specular;
			break;
		}

	}
	break;
	case dae::Renderer::RenderMode::Depth:
	{
		const float depthCol{ Remap(v.position.w,0.985f,1.f) };
		finalColor = { depthCol,depthCol,depthCol };
	}
	break;
	case dae::Renderer::RenderMode::END:
		std::cout << "Something went terribly wrong, Rendermode is END\n";
		break;
	}

	//Update Color in Buffer
	finalColor.MaxToOne();

	const int px{ static_cast<int>(v.position.x) };
	const int py{ static_cast<int>(v.position.y) };

	m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
		static_cast<uint8_t>(finalColor.r * 255),
		static_cast<uint8_t>(finalColor.g * 255),
		static_cast<uint8_t>(finalColor.b * 255));
}

bool Renderer::SaveBufferToImage() const
{
	return SDL_SaveBMP(m_pBackBuffer, "Rasterizer_ColorBuffer.bmp");
}
