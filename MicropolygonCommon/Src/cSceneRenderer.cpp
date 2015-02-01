
#include "stdafx.h"
#include "cSceneRenderer.h"
#include "cScene.h"
#include "iRasterizer.h"
#include "cGrid.h"

using namespace std;

namespace MicropolygonCommon
{

//--------------------------------------------------------------------------------------
// Render the scene with the given rasterizer.
//--------------------------------------------------------------------------------------
void cSceneRenderer::Render(iRasterizer* Rasterizer, int ScreenWidth, int ScreenHeight)
{
	// Process each quad individually.
	for (vector<cQuad>::const_iterator it = m_Scene->m_Quads.begin();
		it != m_Scene->m_Quads.end(); ++it)
	{
		// Calc screen-space size of the quad.
		cAABB aabb = it->GetAABB();
		const float Width = XMVectorGetX(aabb.GetDiagonal());
		const float Height = XMVectorGetY(aabb.GetDiagonal());

		const float PixelWidth = Width * 0.5f * ScreenWidth;
		const float PixelHeight = Height * 0.5f * ScreenHeight;

		const int NumPolysX = (int) ceil(PixelWidth / m_MicropolygonSize);
		const int NumPolysY = (int) ceil(PixelHeight / m_MicropolygonSize);

		if (NumPolysX * NumPolysY > 0)
		{
			// Create a new uPoly grid for this quad.
			cGrid Grid(NumPolysX, NumPolysY, m_Scene->m_Transform, m_Scene->m_PrevTransform);

			// Dice the quad into micropolygons.
			// TODO: Forward differencing is probably more efficient than lerping.
			for (int y = 0; y <= NumPolysY; y++)
			{
				// Start and end points of this row.
				const float yAlpha = (float) y / (float) NumPolysY;
				const cQuadVertex RowStart = Lerp(it->m_Verts[0], it->m_Verts[2], yAlpha);
				const cQuadVertex RowEnd   = Lerp(it->m_Verts[1], it->m_Verts[3], yAlpha);

				for (int x = 0; x <= NumPolysX; x++)
				{
					const float xAlpha = (float) x / (float) NumPolysX;
					const cQuadVertex Vert = Lerp(RowStart, RowEnd, xAlpha);

					// Add vert to the grid.
					Grid.SetVert(x, y, Vert);
				}
			}

			Rasterizer->RasterizeGrid(Grid);
		}
	}
}

}