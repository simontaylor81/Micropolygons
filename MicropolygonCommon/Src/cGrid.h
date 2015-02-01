#pragma once

#include "cQuad.h"

namespace MicropolygonCommon
{

//--------------------------------------------------------------------------------------
// A grid of micropolygons.
//--------------------------------------------------------------------------------------
class cGrid
{
public:

	cGrid(int NumPolysX, int NumPolysY, const XMFLOAT4X4& Transform, const XMFLOAT4X4& PrevTransform)
		: m_NumPolysX(NumPolysX)
		, m_NumPolysY(NumPolysY)
		, m_Transform(Transform)
		, m_PrevTransform(PrevTransform)
		, m_Verts(new cQuadVertex[(NumPolysX + 1) * (NumPolysY + 1)])
	{}

	~cGrid()
	{
		delete[] m_Verts;
	}

	const cQuadVertex& GetVert(int x, int y) const
	{
		_ASSERTE(x >= 0 && x <= m_NumPolysX);
		_ASSERTE(y >= 0 && y <= m_NumPolysY);
		return m_Verts[y * (m_NumPolysX + 1) + x];
	}

	void SetVert(int x, int y, const cQuadVertex& Vert)
	{
		_ASSERTE(x >= 0 && x <= m_NumPolysX);
		_ASSERTE(y >= 0 && y <= m_NumPolysY);
		m_Verts[y * (m_NumPolysX + 1) + x] = Vert;
	}

	int GetNumPolysX() const { return m_NumPolysX; }
	int GetNumPolysY() const { return m_NumPolysY; }

	XMMATRIX GetTransform() const { return XMLoadFloat4x4(&m_Transform); }
	XMMATRIX GetPrevTransform() const { return XMLoadFloat4x4(&m_PrevTransform); }

private:

	int				m_NumPolysX;
	int				m_NumPolysY;
	cQuadVertex*	m_Verts;

	// Transforms for the current and previous frames.
	XMFLOAT4X4	m_Transform;
	XMFLOAT4X4	m_PrevTransform;
};

}
