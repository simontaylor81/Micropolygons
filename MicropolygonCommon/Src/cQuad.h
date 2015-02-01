#pragma once

#include "cAABB.h"

namespace MicropolygonCommon
{

//--------------------------------------------------------------------------------------
// A quad vertex
//--------------------------------------------------------------------------------------
class cQuadVertex
{
public:
	XMFLOAT3 pos;
	XMFLOAT4 colour;

	cQuadVertex() {}
	cQuadVertex(const XMFLOAT3& InPos, const XMFLOAT4& InColour)
		: pos(InPos)
		, colour(InColour)
	{}
	cQuadVertex(const XMVECTOR& InPos, const XMVECTOR& InColour)
	{
		XMStoreFloat3(&pos, InPos);
		XMStoreFloat4(&colour, InColour);
	}

	XMVECTOR GetPos() const
	{
		return XMLoadFloat3(&pos);
	}
	XMVECTOR GetColour() const
	{
		return XMLoadFloat4(&colour);
	}

	// A couple of operators to allow Lerp to work.
	friend cQuadVertex operator+(const cQuadVertex& lhs, const cQuadVertex& rhs)
	{
		return cQuadVertex(
			XMVectorAdd(lhs.GetPos(), rhs.GetPos()),
			XMVectorAdd(lhs.GetColour(), rhs.GetColour()));
	}
	friend cQuadVertex operator*(float c, const cQuadVertex& vert)
	{
		using namespace DirectX;
		return cQuadVertex(
			XMVectorScale(vert.GetPos(), c),
			XMVectorScale(vert.GetColour(), c));
	}
	friend cQuadVertex operator*(const cQuadVertex& vert, float c)
	{
		return c * vert;
	}
};

//--------------------------------------------------------------------------------------
// A very simple quad structure.
//--------------------------------------------------------------------------------------
class cQuad
{
public:

	cQuad() {}
	cQuad(const cQuadVertex& v0,
		  const cQuadVertex& v1,
		  const cQuadVertex& v2,
		  const cQuadVertex& v3)
	{
		m_Verts[0] = v0;
		m_Verts[1] = v1;
		m_Verts[2] = v2;
		m_Verts[3] = v3;
	}

	// Get the AABB of this quad.
	cAABB GetAABB() const
	{
		cAABB Result;
		for (int i = 0; i < 4; i++)
		{
			Result += m_Verts[i].pos;
		}
		return Result;
	}

	// For now, just 4 verts.
	cQuadVertex m_Verts[4];
};

}
