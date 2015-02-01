//--------------------------------------------------------------------------------------
// Utility function implementations.
//--------------------------------------------------------------------------------------

#include "stdafx.h"
#include "Utility.h"
#include <assert.h>

namespace MicropolygonCommon
{

//--------------------------------------------------------------------------------------
// cTimer implementation.
//--------------------------------------------------------------------------------------

cTiming::cTiming()
{
	// Get performance counter frequency.
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);

	m_PerfCounterPeriod = 1.0 / (double) freq.QuadPart;
}

double cTiming::GetSeconds() const
{
	// Get the performance counter's current value.
	LARGE_INTEGER count;
	QueryPerformanceCounter(&count);

	return (double) count.QuadPart * m_PerfCounterPeriod;
}

}
