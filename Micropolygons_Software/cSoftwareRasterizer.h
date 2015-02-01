#pragma once

#include "iRasterizer.h"
#include "Utility.h"

class cSoftwareRasterizer : public MicropolygonCommon::iRasterizer
{
public:

	// Constructor
	cSoftwareRasterizer(UINT Width, UINT Height, INT MSFactor, float FilterWidth, DWORD* TargetPixels)
		: m_Width(Width)
		, m_Height(Height)
		, m_MSFactor(MSFactor)
		, m_MSFilterWidth((INT) (FilterWidth * MSFactor))
		, m_TargetPixels(TargetPixels)
	{
		// Allocate super-sampled render target.
		m_MSBuffer = MicropolygonCommon::AlignedAlloc<tRenderTargetFormat>(Width * Height * MSFactor * MSFactor);
	}

	~cSoftwareRasterizer()
	{
		MicropolygonCommon::AlignedFree(m_MSBuffer);
	}

	// Rasterize a set of micropolygons using the CPU.
	virtual void RasterizeGrid(const MicropolygonCommon::cGrid& Grid);

private:

	void RasterizeGridStandard(const MicropolygonCommon::cGrid& Grid);
	void RasterizeGridMotionBlur(const MicropolygonCommon::cGrid& Grid);

	void DownsampleBuffer();

	// Convert Normalised screen space to multi-sampled pixel space.
	float ToMSPixelX(float x) const
	{
		return (x * 0.5f + 0.5f) * (m_Width * m_MSFactor);
	}
	float ToMSPixelY(float y) const
	{
		return (-y * 0.5f + 0.5f) * (m_Height * m_MSFactor);
	}
	XMVECTOR ToMSPixelVert(FXMVECTOR In)
	{
		const XMVECTOR Scale = XMVectorReplicate(0.5f * m_MSFactor) * XMVectorSet((float)m_Width, -(float)m_Height, 0.0f, 0.0f);
		const XMVECTOR Bias = XMVectorReplicate(0.5f * m_MSFactor) * XMVectorSet((float)m_Width, (float)m_Height, 0.0f, 0.0f);
		return In * Scale + Bias;
	}

	// Convert Normalised screen space to NON-multi-sampled pixel space.
	float ToPixelX(float x) const
	{
		return (x * 0.5f + 0.5f) * m_Width;
	}
	float ToPixelY(float y) const
	{
		return (-y * 0.5f + 0.5f) * m_Height;
	}
	XMVECTOR ToPixelVert(FXMVECTOR In)
	{
		const XMVECTOR Scale = XMVectorReplicate(0.5f) * XMVectorSet((float)m_Width, -(float)m_Height, 0.0f, 0.0f);
		const XMVECTOR Bias = XMVectorReplicate(0.5f) * XMVectorSet((float)m_Width, (float)m_Height, 0.0f, 0.0f);
		return In * Scale + Bias;
	}

	// Compute the colour for a pixel by filtering the supersampled buffer.
	XMVECTOR FilterPixel(int x, int y);

	UINT	m_Width;
	UINT	m_Height;
	INT		m_MSFactor;
	INT		m_MSFilterWidth;
	DWORD*	m_TargetPixels;

	// The super-sampled buffer.
	typedef XMUSHORTN4 tRenderTargetFormat;
	tRenderTargetFormat*	m_MSBuffer;

	// Jitter lookup buffer to ensure sampling locations are coherent temporaly.
	enum { JitterLookupSizePixels = 32 };
	static XMVECTOR*	sm_JitterLookup;
	static int			sm_JitterLookupMSFactor;

	static void InitJitterLookup(int MSFactor);
	static int GetJitterLookupSize() { return JitterLookupSizePixels * sm_JitterLookupMSFactor; }

	static const XMVECTOR GetJitter(XMVECTOR xy)
	{
		// Arbitrary scale & bias to apply to randomise the coordinates.
		static const XMVECTORF32 Scale = { 10.32854f, 1.2029f, 0.0f, 0.0f };
		static const XMVECTORF32 Bias = { 12.94703f, 4.3744f, 0.0f, 0.0f };

		auto x = XMVectorSplatX(xy);
		auto y = XMVectorSplatY(xy);

		// Mush the x & y together and apply scale & bias.
		auto permuted = (x + y) * Scale * Bias;

		// Take fractional portion.
		return permuted - _mm_cvtepi32_ps(_mm_cvttps_epi32(permuted));
	}

	static const XMVECTOR& GetJitterWithT(int x, int y)
	{
		x = x % GetJitterLookupSize();
		y = y % GetJitterLookupSize();
		return sm_JitterLookup[y * GetJitterLookupSize() + x];
	}
};
