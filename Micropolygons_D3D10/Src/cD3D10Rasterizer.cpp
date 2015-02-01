// Implementation of the D3D10 rasterizer.

#include "stdafx.h"
#include "cD3D10Rasterizer.h"
#include "cGrid.h"
#include "Globals.h"

namespace
{

//--------------------------------------------------------------------------------------
// A set of four edge equations.
//--------------------------------------------------------------------------------------
class cFourEquations
{
public:
	cFourEquations() {}

	void Set(const D3DXVECTOR2* points)
	{
		const int IndexLookup[] = {0,1,3,2};
		for (int i = 0; i < 4; i++)
		{
			const D3DXVECTOR2& p0 = points[IndexLookup[i]];
			const D3DXVECTOR2& p1 = points[IndexLookup[(i+1) % 4]];

			As[i] = p1.y - p0.y;
			Bs[i] = p0.x - p1.x;
			Cs[i] = As[i] * p0.x + Bs[i] * p0.y;
		}
	}
	void Set(const D3DXVECTOR2* points, float Scalar)
	{
		const int IndexLookup[] = {0,1,3,2};
		for (int i = 0; i < 4; i++)
		{
			const D3DXVECTOR2& p0 = points[IndexLookup[i]] * Scalar;
			const D3DXVECTOR2& p1 = points[IndexLookup[(i+1) % 4]] * Scalar;

			As[i] = p1.y - p0.y;
			Bs[i] = p0.x - p1.x;
			Cs[i] = As[i] * p0.x + Bs[i] * p0.y;
		}
	}

	// The coefficients of the equations.
	// Note we use the form Ax + By = C, so C is negated from the canonical form.
	float	As[4];
	float	Bs[4];
	float	Cs[4];
};

//--------------------------------------------------------------------------------------
// Intermediate micropolygon data structure.
//--------------------------------------------------------------------------------------
class cIntermediateQuadNoBlur
{
public:

	cFourEquations	m_EdgeEquations;
	D3DXCOLOR		m_Colour;					// Single colour (no Gouraud)
	INT				XMin, XMax, YMin, YMax;		// Conservative extents of the screen-space AABB.
};

//--------------------------------------------------------------------------------------
// Vertex data type.
//--------------------------------------------------------------------------------------
struct cQuadVertex
{
	XMFLOAT3		Pos;
	UINT			Colour;
	cFourEquations	EdgeEquations;
};

// Convert Normalised screen space to multi-sampled pixel space.
float ToMSPixelX(float x)
{
	return (x * 0.5f + 0.5f) * (g_Width * g_MultiSampleFactor);
}
float ToMSPixelY(float y)
{
	return (-y * 0.5f + 0.5f) * (g_Height * g_MultiSampleFactor);
}
D3DXVECTOR2 ToMSPixelVert(const XMFLOAT3& In)
{
	return D3DXVECTOR2(ToMSPixelX(In.x), ToMSPixelY(In.y));
}

}

//--------------------------------------------------------------------------------------
// Rasterize a set of micropolygons using D3D10.
//--------------------------------------------------------------------------------------
void cD3D10Rasterizer::RasterizeGrid(const MicropolygonCommon::cGrid& Grid)
{
	if (!VertexBuffer)
	{
		InitVertexBuffer(Grid);
	}

	// Set the input layout
	D3DDevice->IASetInputLayout(VertexLayout);

	// Set vertex buffer
	UINT stride = sizeof(cQuadVertex);
	UINT offset = 0;
	D3DDevice->IASetVertexBuffers(0, 1, &VertexBuffer, &stride, &offset);

	// Set primitive topology
	D3DDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Render a triangle (assume single pass; asserted at initialisation).
	Technique->GetPassByIndex(0)->Apply(0);
	D3DDevice->Draw(NumVerts, 0);
}

//--------------------------------------------------------------------------------------
// Create the vertex buffer given the Grid.
// This is temporary stuff -- should be done with a geometry shader eventually.
//--------------------------------------------------------------------------------------
void cD3D10Rasterizer::InitVertexBuffer(const MicropolygonCommon::cGrid& Grid)
{
	// TEMP: Bust on the CPU.

	// Compute screen-space AABB and edge equations for each uPoly.
	cIntermediateQuadNoBlur* IntQuads = new cIntermediateQuadNoBlur[Grid.GetNumPolysX() * Grid.GetNumPolysY()];
	INT NumIntQuads = 0;

	// Bust each uPoly in the grid.
	for (INT y = 0; y < Grid.GetNumPolysY(); y++)
	{
		for (INT x = 0; x < Grid.GetNumPolysX(); x++)
		{
			cIntermediateQuadNoBlur OutQuad;

			// Transform verts to perspective space.
			XMFLOAT3 PerspectivePositions[4];
			D3DXVec3TransformCoord(&PerspectivePositions[0], &Grid.GetVert(x    , y    ).pos, &Grid.GetTransform());
			D3DXVec3TransformCoord(&PerspectivePositions[1], &Grid.GetVert(x + 1, y    ).pos, &Grid.GetTransform());
			D3DXVec3TransformCoord(&PerspectivePositions[2], &Grid.GetVert(x    , y + 1).pos, &Grid.GetTransform());
			D3DXVec3TransformCoord(&PerspectivePositions[3], &Grid.GetVert(x + 1, y + 1).pos, &Grid.GetTransform());

			// Convert perspective positions to pixel space.
			D3DXVECTOR2 PixelPositions[4] =
			{
				ToMSPixelVert(PerspectivePositions[0]),
				ToMSPixelVert(PerspectivePositions[1]),
				ToMSPixelVert(PerspectivePositions[2]),
				ToMSPixelVert(PerspectivePositions[3])
			};

			// Calculate conservative pixel-space bounds.
			OutQuad.XMin = (INT) floor(PixelPositions[0].x);
			OutQuad.YMin = (INT) floor(PixelPositions[0].y);
			OutQuad.XMax = (INT) ceil(PixelPositions[0].x);
			OutQuad.YMax = (INT) ceil(PixelPositions[0].y);

			for (int i = 1; i < 4; i++)
			{
				OutQuad.XMin = Min(OutQuad.XMin, (INT) floor(PixelPositions[i].x));
				OutQuad.YMin = Min(OutQuad.YMin, (INT) floor(PixelPositions[i].y));
				OutQuad.XMax = Max(OutQuad.XMax, (INT) ceil(PixelPositions[i].x));
				OutQuad.YMax = Max(OutQuad.YMax, (INT) ceil(PixelPositions[i].y));
			}

			// Discard polys completely off screen.
			if (OutQuad.XMax < 0 || OutQuad.XMin >= (INT) (g_Width * g_MultiSampleFactor) ||
				OutQuad.YMax < 0 || OutQuad.YMin >= (INT) (g_Height * g_MultiSampleFactor))
			{
				continue;
			}

			// Clamp min & max to screen bounds to avoid worrying about it later.
			OutQuad.XMin = Max(OutQuad.XMin, 0);
			OutQuad.YMin = Max(OutQuad.YMin, 0);
			OutQuad.XMax = Min(OutQuad.XMax, (INT) (g_Width * g_MultiSampleFactor - 1));
			OutQuad.YMax = Min(OutQuad.YMax, (INT) (g_Height * g_MultiSampleFactor - 1));

			// Copy colour of first vert.
			const D3DXCOLOR colour = Grid.GetVert(x, y).colour;
			OutQuad.m_Colour = D3DXCOLOR(colour);

			// Compute edge equations.
			OutQuad.m_EdgeEquations.Set(PixelPositions);

			// Add the resulting quad to the intermediate list.
			IntQuads[NumIntQuads++] = OutQuad;
		}
	}

	NumVerts = NumIntQuads * 6;
	//NumVerts = 3;
	cQuadVertex* vertices = new cQuadVertex[NumVerts];

	// TEMP: Rasterize each uPoly individually.
	
	for (INT nPoly = 0; nPoly < NumIntQuads; nPoly++)
	{
		const cIntermediateQuadNoBlur& Quad = IntQuads[nPoly];

		const float fXMin = (float) Quad.XMin / (float) g_Width * 2.0f - 1.0f;
		const float fXMax = (float) Quad.XMax / (float) g_Width * 2.0f - 1.0f;
		const float fYMin = -((float) Quad.YMin / (float) g_Height * 2.0f - 1.0f);
		const float fYMax = -((float) Quad.YMax / (float) g_Height * 2.0f - 1.0f);

		// Create two triangles to cover the quad's AABB.
		vertices[6 * nPoly + 0].Pos = XMFLOAT3(fXMin, fYMin, 0.5f);
		vertices[6 * nPoly + 1].Pos = XMFLOAT3(fXMax, fYMin, 0.5f);
		vertices[6 * nPoly + 2].Pos = XMFLOAT3(fXMin, fYMax, 0.5f);
		vertices[6 * nPoly + 3].Pos = XMFLOAT3(fXMin, fYMax, 0.5f);
		vertices[6 * nPoly + 4].Pos = XMFLOAT3(fXMax, fYMin, 0.5f);
		vertices[6 * nPoly + 5].Pos = XMFLOAT3(fXMax, fYMax, 0.5f);

		UINT dwR = (UINT) (Clamp(Quad.m_Colour.r, 0.0f, 1.0f) * 255.0f + 0.5f);
		UINT dwG = (UINT) (Clamp(Quad.m_Colour.g, 0.0f, 1.0f) * 255.0f + 0.5f);
		UINT dwB = (UINT) (Clamp(Quad.m_Colour.b, 0.0f, 1.0f) * 255.0f + 0.5f);
		UINT dwA = (UINT) (Clamp(Quad.m_Colour.a, 0.0f, 1.0f) * 255.0f + 0.5f);

		const UINT Colour = (dwA << 24) | (dwB << 16) | (dwG << 8) | (dwR << 0);
		vertices[6 * nPoly + 0].Colour = Colour;
		vertices[6 * nPoly + 1].Colour = Colour;
		vertices[6 * nPoly + 2].Colour = Colour;
		vertices[6 * nPoly + 3].Colour = Colour;
		vertices[6 * nPoly + 4].Colour = Colour;
		vertices[6 * nPoly + 5].Colour = Colour;

		vertices[6 * nPoly + 0].EdgeEquations = Quad.m_EdgeEquations;
		vertices[6 * nPoly + 1].EdgeEquations = Quad.m_EdgeEquations;
		vertices[6 * nPoly + 2].EdgeEquations = Quad.m_EdgeEquations;
		vertices[6 * nPoly + 3].EdgeEquations = Quad.m_EdgeEquations;
		vertices[6 * nPoly + 4].EdgeEquations = Quad.m_EdgeEquations;
		vertices[6 * nPoly + 5].EdgeEquations = Quad.m_EdgeEquations;
	}
	

	// Create vertex buffer
	//vertices[0].Pos = XMFLOAT3( 0.0f, 0.5f, 0.5f );
	//vertices[1].Pos = XMFLOAT3( 0.5f, -0.5f, 0.5f );
	//vertices[2].Pos = XMFLOAT3( -0.5f, -0.5f, 0.5f );

	D3D10_BUFFER_DESC bd;
	bd.Usage = D3D10_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(cQuadVertex) * NumVerts;
	bd.BindFlags = D3D10_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;
	bd.MiscFlags = 0;
	D3D10_SUBRESOURCE_DATA InitData;
	InitData.pSysMem = vertices;
	D3DDevice->CreateBuffer(&bd, &InitData, &VertexBuffer);

	delete[] IntQuads;
	delete[] vertices;
}

//--------------------------------------------------------------------------------------
// Initialise the resources needed by the rasterizer.
//--------------------------------------------------------------------------------------
BOOL cD3D10Rasterizer::Init(ID3D10Device *pd3dDevice)
{
	D3DDevice = pd3dDevice;

	// Compile the effect
	BOOL bSuccess = FALSE;
	while (!bSuccess)
	{
		ID3D10Blob* ErrorBlob;
		bSuccess = SUCCEEDED( D3DX10CreateEffectFromFile(L"Rasterize.fx", NULL, NULL, "fx_4_0",
			D3D10_SHADER_ENABLE_STRICTNESS, 0, D3DDevice, NULL, NULL, &Effect, &ErrorBlob, NULL) );

		if (!bSuccess)
		{
			// Display an message box with the compile errors.
			int Result = MessageBoxA(NULL, (char*) ErrorBlob->GetBufferPointer(), "Compile error. Retry?", MB_YESNO);
			if (Result == IDNO)
			{
				return FALSE;
			}
		}
	}

	// Obtain the technique
	Technique = Effect->GetTechniqueByName( "Render" );

	// For simplicity, assert a single pass.
	D3D10_TECHNIQUE_DESC techDesc;
	Technique->GetDesc(&techDesc);
	_ASSERTE(techDesc.Passes == 1);

	// Define the input layout
	D3D10_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
	};
	UINT numElements = sizeof(layout)/sizeof(layout[0]);

	// Create the input layout
	D3D10_PASS_DESC PassDesc;
	Technique->GetPassByIndex(0)->GetDesc(&PassDesc);
	if (FAILED( D3DDevice->CreateInputLayout(layout, numElements, PassDesc.pIAInputSignature, 
			PassDesc.IAInputSignatureSize, &VertexLayout) ))
	{
		return FALSE;
	}

	return TRUE;
}

//--------------------------------------------------------------------------------------
// Release resources created by the rasterizer.
//--------------------------------------------------------------------------------------
void cD3D10Rasterizer::Release()
{
	// Release everything that we creating in Init().
	if (VertexBuffer)	VertexBuffer->Release();
	if (VertexLayout)	VertexLayout->Release();
	if (Effect)			Effect->Release();
}
