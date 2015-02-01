// Definitions for the D3D10 micropolygon rasterizer.

#pragma once

#include "iRasterizer.h"

class cD3D10Rasterizer : public MicropolygonCommon::iRasterizer
{
public:

	// Initialisation and cleanup.
	BOOL Init(ID3D10Device* pd3dDevice);
	void Release();

	// Rasterize a set of micropolygons using D3D10.
	virtual void RasterizeGrid(const MicropolygonCommon::cGrid& Grid);

private:

	void InitVertexBuffer(const MicropolygonCommon::cGrid& Grid);

	ID3D10Device*			D3DDevice;
	ID3D10Effect*			Effect;
	ID3D10EffectTechnique*	Technique;
	ID3D10InputLayout*		VertexLayout;
	ID3D10Buffer*			VertexBuffer;

	INT NumVerts;
};
