#pragma once

#include "Maths.h"

//--------------------------------------------------------------------------------------
// Basic axis-aligned bounding box class.
//--------------------------------------------------------------------------------------
class cAABB
{
public:

	cAABB()
		: m_bValid(false)
	{}

	cAABB& operator+=(const XMFLOAT3& other)
	{
		if (!m_bValid)
		{
			m_Min = m_Max = other;
			m_bValid = true;
		}
		else
		{
			m_Min.x = Min(m_Min.x, other.x);
			m_Min.y = Min(m_Min.y, other.y);
			m_Min.z = Min(m_Min.z, other.z);
			m_Max.x = Max(m_Max.x, other.x);
			m_Max.y = Max(m_Max.y, other.y);
			m_Max.z = Max(m_Max.z, other.z);
		}

		return *this;
	}

	XMVECTOR GetDiagonal() const
	{
		return XMLoadFloat3(&m_Max) - XMLoadFloat3(&m_Min);
	}
	XMVECTOR GetCentre() const
	{
		return 0.5f * (XMLoadFloat3(&m_Min) + XMLoadFloat3(&m_Max));
	}

	XMFLOAT3	m_Min;
	XMFLOAT3	m_Max;
	bool		m_bValid;
};
