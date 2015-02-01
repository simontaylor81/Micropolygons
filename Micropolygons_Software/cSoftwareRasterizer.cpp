#include "stdafx.h"
#include "cSoftwareRasterizer.h"
#include "cGrid.h"

#define USE_SSE 1

#if USE_SSE
#include <xmmintrin.h>
#include <fvec.h>
#endif

using namespace MicropolygonCommon;

namespace
{

//--------------------------------------------------------------------------------------
// A set of four edge equations.
// 16-byte aligned to allow SSE usage.
//--------------------------------------------------------------------------------------
__declspec(align(16)) class cFourEquations
{
public:
	cFourEquations() {}

	void Set(const XMVECTOR* points)
	{
		const int IndexLookup[] = {0,1,3,2};
		for (int i = 0; i < 4; i++)
		{
			const auto& p0 = points[IndexLookup[i]];
			const auto& p1 = points[IndexLookup[(i+1) % 4]];

			const float p0_x = XMVectorGetX(p0);
			const float p0_y = XMVectorGetY(p0);
			const float p1_x = XMVectorGetX(p1);
			const float p1_y = XMVectorGetY(p1);

			const float A = p1_y - p0_y;
			const float B = p0_x - p1_x;
			const float C = A * p0_x + B * p0_y;

			As = XMVectorSetByIndex(As, A, i);
			Bs = XMVectorSetByIndex(Bs, B, i);
			Cs = XMVectorSetByIndex(Cs, C, i);
		}
	}
	void Set(const XMVECTOR* points, float Scalar)
	{
		const int IndexLookup[] = {0,1,3,2};
		for (int i = 0; i < 4; i++)
		{
			const auto& p0 = points[IndexLookup[i]] * Scalar;
			const auto& p1 = points[IndexLookup[(i + 1) % 4]] * Scalar;

			const float p0_x = XMVectorGetX(p0);
			const float p0_y = XMVectorGetY(p0);
			const float p1_x = XMVectorGetX(p1);
			const float p1_y = XMVectorGetY(p1);

			const float A = p1_y - p0_y;
			const float B = p0_x - p1_x;
			const float C = A * p0_x + B * p0_y;

			As = XMVectorSetByIndex(As, A, i);
			Bs = XMVectorSetByIndex(Bs, B, i);
			Cs = XMVectorSetByIndex(Cs, C, i);
		}
	}

	// The coefficients of the equations.
	// Note we use the form Ax + By = C, so C is negated from the canonical form.
	XMVECTOR	As;
	XMVECTOR	Bs;
	XMVECTOR	Cs;
};

//--------------------------------------------------------------------------------------
// Intermediate micropolygon data structure.
//--------------------------------------------------------------------------------------
class cIntermediateQuadNoBlur
{
public:

	cFourEquations	m_EdgeEquations;
	XMUSHORTN4		m_Colour;					// Single colour (no Gouraud)
	INT				XMin, XMax, YMin, YMax;		// Conservative extents of the screen-space AABB.
};

//--------------------------------------------------------------------------------------
// Intermediate micropolygon data structure for motion blur case.
//--------------------------------------------------------------------------------------
class cIntermediateQuadMotionBlur
{
public:

	cFourEquations	m_EdgeEquations[2];			// t0 and t1
	XMUSHORTN4		m_Colour;					// Single colour (no Gouraud)
	INT				XMin, XMax, YMin, YMax;		// Conservative extents of the screen-space AABB.
};

//--------------------------------------------------------------------------------------
// Get a prototype pattern for a given multisample factor.
//--------------------------------------------------------------------------------------
float* GetPrototype(int MSFactor)
{
	switch (MSFactor)
	{
	case 1:
		{
			static float Prototype = 0.0f;
			return &Prototype;
		}
		break;

	case 2:
		{
			static float Prototype[] = { 0.0f, 0.5f, 0.25f, 0.75f };
			return Prototype;
		}
		break;

	case 4:
		{
			// Copied from [Cook 1986].
			static float Prototype[] =
			{
				0.3750f, 0.6250f, 0.1250f, 0.8125f,
				0.1875f, 0.8750f, 0.7500f, 0.5000f,
				0.9375f, 0.0000f, 0.4375f, 0.6875f,
				0.3125f, 0.5625f, 0.2500f, 0.0625f,
			};
			return Prototype;
		}
		break;

	default:
		// TODO
		_ASSERT(0);
		return NULL;
	}
}

//--------------------------------------------------------------------------------------
// Functions for testing 4 edge equations in parallel.
//--------------------------------------------------------------------------------------

#if USE_SSE

bool IsInsideFourEqns_SSE(const __m128& a, const __m128& b, const __m128& c, const __m128& x, const __m128& y)
{
	// Compute LHS of inequality.
	__m128 lhs = _mm_add_ps(
		_mm_mul_ps(a, x),
		_mm_mul_ps(b, y));

	// Compare LHS & RHS.
	__m128 Comparison = _mm_cmplt_ps(lhs, c);

	// Extract result.
	int Mask = _mm_movemask_ps(Comparison);
	return !Mask;
}

__m128 LerpSSE(FXMVECTOR X, FXMVECTOR Y, const __m128& alpha)
{
	// Result = X + alpha * (Y - X)

	return _mm_add_ps(X, _mm_mul_ps(alpha, _mm_sub_ps(Y, X)));
}

#endif

bool IsInsideFourEquations(const cFourEquations& Eqns, float X, float Y)
{
// IsInside <=> A*x + B*y > C;

#if USE_SSE

	// Load coordinates into registers.
	__m128 x = _mm_set1_ps(X);
	__m128 y = _mm_set1_ps(Y);

	return IsInsideFourEqns_SSE(Eqns.As, Eqns.Bs, Eqns.Cs, x, y);

#else
	for (int i = 0; i < 4; i++)
	{
		if (Eqns.As[i] * X + Eqns.Bs[i] * Y <= Eqns.Cs[i])
		{
			return false;
		}
	}

	return true;
#endif
}

bool IsInsideFourTimeDependentEqns(const cFourEquations& Eqns_t0, const cFourEquations& Eqns_t1, float X, float Y, float T)
{
#if USE_SSE

	// Lerp the two equation sets.
	__m128 t = _mm_set1_ps(T);

	__m128 a = LerpSSE(Eqns_t0.As, Eqns_t1.As, t);
	__m128 b = LerpSSE(Eqns_t0.Bs, Eqns_t1.Bs, t);
	__m128 c = LerpSSE(Eqns_t0.Cs, Eqns_t1.Cs, t);

	// Load coordinates into registers
	__m128 x = _mm_set1_ps(X);
	__m128 y = _mm_set1_ps(Y);

	// Compute the inequality.
	return IsInsideFourEqns_SSE(a, b, c, x, y);

#else
	for (int i = 0; i < 4; i++)
	{
		// Lerp coefficients
		float A = Lerp(Eqns_t0.As[i], Eqns_t1.As[i], T);
		float B = Lerp(Eqns_t0.Bs[i], Eqns_t1.Bs[i], T);
		float C = Lerp(Eqns_t0.Cs[i], Eqns_t1.Cs[i], T);

		// Compute inequality
		if (A * X + B * Y <= C)
		{
			return false;
		}
	}

	return true;
#endif
}

}

// Static members.
XMFLOAT3*	cSoftwareRasterizer::sm_JitterLookup = NULL;
int			cSoftwareRasterizer::sm_JitterLookupMSFactor = 0;

inline bool MatrixEqual(const XMMATRIX& a, const FXMMATRIX& b)
{
	return
		XMVector4Equal(a.r[0], b.r[0]) &&
		XMVector4Equal(a.r[1], b.r[1]) &&
		XMVector4Equal(a.r[2], b.r[2]) &&
		XMVector4Equal(a.r[3], b.r[3]);
}

//--------------------------------------------------------------------------------------
// Rasterize a grid of micropolygons.
//--------------------------------------------------------------------------------------
void cSoftwareRasterizer::RasterizeGrid(const cGrid& Grid)
{
	if (sm_JitterLookupMSFactor != m_MSFactor)
	{
		InitJitterLookup(m_MSFactor);
	}

	// Clear render target.
	ZeroMemory(m_MSBuffer, m_Width * m_Height * m_MSFactor * m_MSFactor * sizeof(*m_MSBuffer));

	// Decide between the two rasterization methods.
	const bool bMotionBlur = !MatrixEqual(Grid.GetTransform(), Grid.GetPrevTransform());
	if (!bMotionBlur)
		RasterizeGridStandard(Grid);
	else
		RasterizeGridMotionBlur(Grid);

	DownsampleBuffer();
}

//--------------------------------------------------------------------------------------
// Rasterize a normal grid that does not have motion blur.
//--------------------------------------------------------------------------------------
void cSoftwareRasterizer::RasterizeGridStandard(const cGrid& Grid)
{
	// Compute screen-space AABB and edge equations for each uPoly.
	cIntermediateQuadNoBlur* IntQuads = (cIntermediateQuadNoBlur*) _aligned_malloc(sizeof(cIntermediateQuadNoBlur) * Grid.GetNumPolysX() * Grid.GetNumPolysY(), __alignof(cIntermediateQuadNoBlur));
	INT NumIntQuads = 0;

	// Bust each uPoly in the grid.
	for (INT y = 0; y < Grid.GetNumPolysY(); y++)
	{
		for (INT x = 0; x < Grid.GetNumPolysX(); x++)
		{
			cIntermediateQuadNoBlur OutQuad;

			// Transform verts to perspective space.
			XMVECTOR PerspectivePositions[4];
			PerspectivePositions[0] = XMVector3TransformCoord(Grid.GetVert(x    , y    ).GetPos(), Grid.GetTransform());
			PerspectivePositions[1] = XMVector3TransformCoord(Grid.GetVert(x + 1, y    ).GetPos(), Grid.GetTransform());
			PerspectivePositions[2] = XMVector3TransformCoord(Grid.GetVert(x    , y + 1).GetPos(), Grid.GetTransform());
			PerspectivePositions[3] = XMVector3TransformCoord(Grid.GetVert(x + 1, y + 1).GetPos(), Grid.GetTransform());

			// Convert perspective positions to pixel space.
			XMVECTOR PixelPositions[4] =
			{
				ToMSPixelVert(PerspectivePositions[0]),
				ToMSPixelVert(PerspectivePositions[1]),
				ToMSPixelVert(PerspectivePositions[2]),
				ToMSPixelVert(PerspectivePositions[3])
			};

			// Calculate conservative pixel-space bounds.
			OutQuad.XMin = (INT) floor(XMVectorGetX(PixelPositions[0]));
			OutQuad.YMin = (INT) floor(XMVectorGetY(PixelPositions[0]));
			OutQuad.XMax = (INT) ceil(XMVectorGetX(PixelPositions[0]));
			OutQuad.YMax = (INT) ceil(XMVectorGetY(PixelPositions[0]));

			for (int i = 1; i < 4; i++)
			{
				OutQuad.XMin = Min(OutQuad.XMin, (INT) floor(XMVectorGetX(PixelPositions[i])));
				OutQuad.YMin = Min(OutQuad.YMin, (INT) floor(XMVectorGetY(PixelPositions[i])));
				OutQuad.XMax = Max(OutQuad.XMax, (INT) ceil(XMVectorGetX(PixelPositions[i])));
				OutQuad.YMax = Max(OutQuad.YMax, (INT) ceil(XMVectorGetY(PixelPositions[i])));
			}

			// Discard polys completely off screen.
			if (OutQuad.XMax < 0 || OutQuad.XMin >= (INT) m_Width * m_MSFactor ||
				OutQuad.YMax < 0 || OutQuad.YMin >= (INT) m_Height * m_MSFactor)
			{
				continue;
			}

			// Clamp min & max to screen bounds to avoid worrying about it later.
			OutQuad.XMin = Max(OutQuad.XMin, 0);
			OutQuad.YMin = Max(OutQuad.YMin, 0);
			OutQuad.XMax = Min(OutQuad.XMax, (INT) m_Width * m_MSFactor - 1);
			OutQuad.YMax = Min(OutQuad.YMax, (INT) m_Height * m_MSFactor - 1);

			// Copy colour of first vert.
			OutQuad.m_Colour = Grid.GetVert(x, y).colour;

			// Compute edge equations.
			OutQuad.m_EdgeEquations.Set(PixelPositions);

			// Add the resulting quad to the intermediate list.
			IntQuads[NumIntQuads++] = OutQuad;
		}
	}

	// Rasterize each uPoly.
	for (INT nPoly = 0; nPoly < NumIntQuads; nPoly++)
	{
		const cIntermediateQuadNoBlur& Quad = IntQuads[nPoly];

		for (INT Y = Quad.YMin; Y <= Quad.YMax; Y++)
		{
			for (INT X = Quad.XMin; X <= Quad.XMax; X++)
			{
				// Test sample location against edge equations.
				const XMFLOAT3& Jitter = GetJitter(X, Y);
				const float x = X + Jitter.x;
				const float y = Y + Jitter.y;

				if (IsInsideFourEquations(Quad.m_EdgeEquations, x, y))
				{
					m_MSBuffer[Y * m_Width * m_MSFactor + X] = Quad.m_Colour;
				}
			}
		}
	}

	_aligned_free(IntQuads);
}

//--------------------------------------------------------------------------------------
// Rasterize a grid that has large amounts of motion blur.
//--------------------------------------------------------------------------------------
void cSoftwareRasterizer::RasterizeGridMotionBlur(const cGrid& Grid)
{
	// Compute screen-space AABB and edge equations for each uPoly.
	cIntermediateQuadMotionBlur* IntQuads = (cIntermediateQuadMotionBlur*) _aligned_malloc(sizeof(cIntermediateQuadMotionBlur) *
		Grid.GetNumPolysX() * Grid.GetNumPolysY() * m_MSFactor * m_MSFactor, __alignof(cIntermediateQuadMotionBlur));
	INT NumIntQuads = 0;

	// Bust each uPoly in the grid.
	for (INT y = 0; y < Grid.GetNumPolysY(); y++)
	{
		for (INT x = 0; x < Grid.GetNumPolysX(); x++)
		{
			// Transform verts to perspective space.
			XMVECTOR PerspectivePositions[4];
			PerspectivePositions[0] = XMVector3TransformCoord(Grid.GetVert(x    , y    ).GetPos(), Grid.GetTransform());
			PerspectivePositions[1] = XMVector3TransformCoord(Grid.GetVert(x + 1, y    ).GetPos(), Grid.GetTransform());
			PerspectivePositions[2] = XMVector3TransformCoord(Grid.GetVert(x    , y + 1).GetPos(), Grid.GetTransform());
			PerspectivePositions[3] = XMVector3TransformCoord(Grid.GetVert(x + 1, y + 1).GetPos(), Grid.GetTransform());

			// Do the same for the previous positions.
			XMVECTOR PrevPerspectivePositions[4];
			PrevPerspectivePositions[0] = XMVector3TransformCoord(Grid.GetVert(x    , y    ).GetPos(), Grid.GetPrevTransform());
			PrevPerspectivePositions[1] = XMVector3TransformCoord(Grid.GetVert(x + 1, y    ).GetPos(), Grid.GetPrevTransform());
			PrevPerspectivePositions[2] = XMVector3TransformCoord(Grid.GetVert(x    , y + 1).GetPos(), Grid.GetPrevTransform());
			PrevPerspectivePositions[3] = XMVector3TransformCoord(Grid.GetVert(x + 1, y + 1).GetPos(), Grid.GetPrevTransform());

			// Convert perspective positions (current and previous) to pixel space (non multisampled).
			XMVECTOR PixelPositions[4] =
			{
				ToPixelVert(PerspectivePositions[0]),
				ToPixelVert(PerspectivePositions[1]),
				ToPixelVert(PerspectivePositions[2]),
				ToPixelVert(PerspectivePositions[3])
			};
			XMVECTOR PrevPixelPositions[4] =
			{
				ToPixelVert(PrevPerspectivePositions[0]),
				ToPixelVert(PrevPerspectivePositions[1]),
				ToPixelVert(PrevPerspectivePositions[2]),
				ToPixelVert(PrevPerspectivePositions[3])
			};

			// Compute edge equations.
			// Edge equations must be in sub-pixel space so multiply by ms-factor.
			cFourEquations EdgeEquations[2];
			EdgeEquations[0].Set(PrevPixelPositions, (float) m_MSFactor);
			EdgeEquations[1].Set(PixelPositions, (float) m_MSFactor);

			const float* Prototype = GetPrototype(m_MSFactor);

			// Process each time sub-sample interval.
			for (int py = 0; py < m_MSFactor; py++)
			{
				for (int px = 0; px < m_MSFactor; px++)
				{
					const float tMin = Prototype[py*m_MSFactor + px];
					const float tMax = tMin + 1.0f / (float) (m_MSFactor*m_MSFactor);

					// Interpolate postions.
					XMVECTOR TMinPixelPositions[4];
					XMVECTOR TMaxPixelPositions[4];
					for (int i = 0; i < 4; i++)
					{
						TMinPixelPositions[i] = Lerp(PrevPixelPositions[i], PixelPositions[i], tMin);
						TMaxPixelPositions[i] = Lerp(PrevPixelPositions[i], PixelPositions[i], tMax);
					}

					cIntermediateQuadMotionBlur OutQuad;

					// Calculate conservative pixel-space bounds.
					OutQuad.XMin = INT_MAX; OutQuad.YMin = INT_MAX;
					OutQuad.XMax = INT_MIN; OutQuad.YMax = INT_MIN;

					for (int i = 0; i < 4; i++)
					{
						OutQuad.XMin = Min(OutQuad.XMin, (int) floor(XMVectorGetX(TMinPixelPositions[i])));
						OutQuad.YMin = Min(OutQuad.YMin, (int) floor(XMVectorGetY(TMinPixelPositions[i])));
						OutQuad.XMin = Min(OutQuad.XMin, (int) floor(XMVectorGetX(TMaxPixelPositions[i])));
						OutQuad.YMin = Min(OutQuad.YMin, (int) floor(XMVectorGetY(TMaxPixelPositions[i])));

						OutQuad.XMax = Max(OutQuad.XMax, (int) ceil(XMVectorGetX(TMinPixelPositions[i])));
						OutQuad.YMax = Max(OutQuad.YMax, (int) ceil(XMVectorGetY(TMinPixelPositions[i])));
						OutQuad.XMax = Max(OutQuad.XMax, (int) ceil(XMVectorGetX(TMaxPixelPositions[i])));
						OutQuad.YMax = Max(OutQuad.YMax, (int) ceil(XMVectorGetY(TMaxPixelPositions[i])));
					}

					// Adjust so it represents the min & max sub-samples that this time sample can occupy.
					OutQuad.XMin = OutQuad.XMin * m_MSFactor + px;
					OutQuad.YMin = OutQuad.YMin * m_MSFactor + py;
					OutQuad.XMax = OutQuad.XMax * m_MSFactor + px;
					OutQuad.YMax = OutQuad.YMax * m_MSFactor + py;

					// Discard polys completely off screen.
					if (OutQuad.XMax < 0 || OutQuad.XMin >= (INT) m_Width * m_MSFactor ||
						OutQuad.YMax < 0 || OutQuad.YMin >= (INT) m_Height * m_MSFactor)
					{
						continue;
					}

					// Clamp min & max to screen bounds to avoid worrying about it later.
					OutQuad.XMin = Max(OutQuad.XMin, 0);
					OutQuad.YMin = Max(OutQuad.YMin, 0);
					OutQuad.XMax = Min(OutQuad.XMax, (INT) m_Width * m_MSFactor - 1);
					OutQuad.YMax = Min(OutQuad.YMax, (INT) m_Height * m_MSFactor - 1);

					// Copy colour of first vert.
					OutQuad.m_Colour = Grid.GetVert(x, y).colour;

					// Copy edge equations.
					for (int i = 0; i < 2; i++)
						OutQuad.m_EdgeEquations[i] = EdgeEquations[i];

					// Add the resulting quad to the intermediate list.
					IntQuads[NumIntQuads++] = OutQuad;
				}
			}
		}
	}

	// Rasterize each uPoly.
	for (INT nPoly = 0; nPoly < NumIntQuads; nPoly++)
	{
		const cIntermediateQuadMotionBlur& Quad = IntQuads[nPoly];

		// Each uPoly is only defined for 1 time period, so skip over the irrelevant ones.
		for (INT Y = Quad.YMin; Y <= Quad.YMax; Y += m_MSFactor)
		{
			for (INT X = Quad.XMin; X <= Quad.XMax; X += m_MSFactor)
			{
				// Test sample location against edge equations.
				const XMFLOAT3& Jitter = GetJitter(X, Y);
				const float x = X + Jitter.x;
				const float y = Y + Jitter.y;
				const float t = Jitter.z;

				if (IsInsideFourTimeDependentEqns(Quad.m_EdgeEquations[0], Quad.m_EdgeEquations[1], x, y, t))
				{
					m_MSBuffer[Y * m_Width * m_MSFactor + X] = Quad.m_Colour;
				}
			}
		}
	}

	_aligned_free(IntQuads);
}

//--------------------------------------------------------------------------------------
// Fast implementation of pow for values between 0 and 1.
// Taken from https://gist.github.com/Novum/1200562
//--------------------------------------------------------------------------------------
inline XMVECTOR FastPow01(XMVECTOR x, XMVECTOR y)
{
#if USE_SSE
	static const __m128 fourOne = _mm_set1_ps(1.0f);
	static const __m128 fourHalf = _mm_set1_ps(0.5f);

	__m128 a = _mm_sub_ps(fourOne, y);
	__m128 b = _mm_sub_ps(x, fourOne);
	__m128 aSq = _mm_mul_ps(a, a);
	__m128 bSq = _mm_mul_ps(b, b);
	__m128 c = _mm_mul_ps(fourHalf, bSq);
	__m128 d = _mm_sub_ps(b, c);
	__m128 dSq = _mm_mul_ps(d, d);
	__m128 e = _mm_mul_ps(aSq, dSq);
	__m128 f = _mm_mul_ps(a, d);
	__m128 g = _mm_mul_ps(fourHalf, e);
	__m128 h = _mm_add_ps(fourOne, f);
	__m128 i = _mm_add_ps(h, g);
	__m128 iRcp = _mm_rcp_ps(i);
	__m128 result = _mm_mul_ps(x, iRcp);

	return result;
#else
#error TODO
#endif
}

//--------------------------------------------------------------------------------------
// Downsample the multi-sampled render target to the backbuffer.
//--------------------------------------------------------------------------------------
void cSoftwareRasterizer::DownsampleBuffer()
{
	// Downsample the super-sampled buffer into the back buffer.
	// (In a rather inefficient way)
	for (UINT y = 0; y < m_Height; y++)
	{
		for (UINT x = 0; x < m_Width; x++)
		{
			XMVECTOR FilteredColour = FilterPixel(x, y);

			// Clamp to [0,1]
			FilteredColour = XMVectorClamp(FilteredColour, XMVectorZero(), XMVectorSplatOne());

			// Gamma correct (not alpha).
			auto GammaColour = FastPow01(FilteredColour, XMVectorReplicate(1.0f / 2.2f));
			FilteredColour = XMVectorSelect(FilteredColour, GammaColour, XMVectorSelectControl(1, 1, 1, 0));

			// Convert to BGRA32 format.
			FilteredColour *= XMVectorReplicate(255.f);
			UINT Colour =
				((BYTE)XMVectorGetZ(FilteredColour) << 24) |
				((BYTE)XMVectorGetY(FilteredColour) << 16) |
				((BYTE)XMVectorGetX(FilteredColour) << 8) |
				((BYTE)XMVectorGetW(FilteredColour) << 0);

			// Assign to backbuffer.
			m_TargetPixels[y*m_Width+x] = Colour;
		}
	}
}

//--------------------------------------------------------------------------------------
// Compute the colour for a pixel by filtering the supersampled buffer.
//--------------------------------------------------------------------------------------
XMVECTOR cSoftwareRasterizer::FilterPixel(int x, int y)
{
	const int Offset = (m_MSFilterWidth - m_MSFactor) / 2;

	int xMin = Max(x * m_MSFactor - Offset, 0);
	int yMin = Max(y * m_MSFactor - Offset, 0);
	int xMax = Min<int>((x+1) * m_MSFactor + Offset, m_Width * m_MSFactor - 1);
	int yMax = Min<int>((y+1) * m_MSFactor + Offset, m_Height * m_MSFactor - 1);

	float SampleCount = 0.0f;

	XMVECTOR AverageColour = XMVectorZero();
	for (int sy = yMin; sy < yMax; sy++)
	{
		for (int sx = xMin; sx < xMax; sx++)
		{
			const tRenderTargetFormat& Sample = m_MSBuffer[sy * m_Width * m_MSFactor + sx];
			AverageColour += XMLoadUShortN4(&Sample);
			SampleCount += 1.0f;
		}
	}

	// Average colour.
	AverageColour /= SampleCount;

	return AverageColour;
}

//--------------------------------------------------------------------------------------
// Initialise the jitter lookup table.
//--------------------------------------------------------------------------------------
void cSoftwareRasterizer::InitJitterLookup(int MSFactor)
{
	sm_JitterLookupMSFactor = MSFactor;

	// Free previous data, if present.
	delete sm_JitterLookup;

	// Allocate new table.
	const int TableSize = GetJitterLookupSize() * GetJitterLookupSize();
	sm_JitterLookup = new XMFLOAT3[TableSize];

	// Seed the random number generator.
	srand(0);

	// Compute spatial jitter values -- simple random values that are added to the sample point.
	for (int i = 0; i < TableSize; i++)
	{
		sm_JitterLookup[i] = XMFLOAT3(
			(float) rand() / (float) RAND_MAX,
			(float) rand() / (float) RAND_MAX,
			0.0f);
	}

	float* Prototype = GetPrototype(MSFactor);

	// Compute temporal jitter. Based on prototype pattern.
	for (int y = 0; y < GetJitterLookupSize(); y+=MSFactor)
	{
		for (int x = 0; x < GetJitterLookupSize(); x+=MSFactor)
		{
			for (int sy = 0; sy < MSFactor; sy++)
			{
				for (int sx = 0; sx < MSFactor; sx++)
				{
					const float t = Prototype[sy * MSFactor + sx] +
						((float) rand() / (float) RAND_MAX) / (float) (MSFactor * MSFactor);
					sm_JitterLookup[(y + sy) * GetJitterLookupSize() + x + sx].z = t;
				}
			}
		}
	}
}
