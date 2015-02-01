#pragma once

namespace MicropolygonCommon
{

// Forward decl.
class cGrid;

//--------------------------------------------------------------------------------------
// Interface for rasterizers that can be used to draw the scene.
//--------------------------------------------------------------------------------------
class iRasterizer
{
public:
	virtual void RasterizeGrid(const cGrid& Grid) = 0;
};

}
