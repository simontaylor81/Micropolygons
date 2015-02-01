#pragma once

#include <vector>
#include "cQuad.h"

namespace MicropolygonCommon
{

//--------------------------------------------------------------------------------------
// A scene that can be rendered. For now, just a bunch of quads.
//--------------------------------------------------------------------------------------
class cScene
{
public:
	std::vector<cQuad>	m_Quads;

	// Transforms for the current and previous frames.
	XMFLOAT4X4	m_Transform;
	XMFLOAT4X4	m_PrevTransform;
};

}
