#pragma once

const float DefaultMicropolygonSize = 128.0f;

namespace MicropolygonCommon
{

//--------------------------------------------------------------------------------------
// Class for rendering scenes.
//--------------------------------------------------------------------------------------
class cSceneRenderer
{
public:

	// Default constructor
	cSceneRenderer()
		: m_Scene(NULL)
		, m_MicropolygonSize(DefaultMicropolygonSize)
	{}

	// Constructor with a given scene.
	cSceneRenderer(class cScene* Scene)
		: m_Scene(Scene)
		, m_MicropolygonSize(DefaultMicropolygonSize)
	{}

	// Render the scene with the given rasterizer.
	void Render(class iRasterizer* Rasterizer, int ScreenWidth, int ScreenHeight);

	// Micropolygon size accessors.
	float GetMicropolygonSize() const { return m_MicropolygonSize; }
	void SetMicropolygonSize(float NewSize)
	{
		m_MicropolygonSize = NewSize;
	}

private:
	class cScene* m_Scene;

	// Approximate size of each micropolygon in pixels.
	float m_MicropolygonSize;
};

}
