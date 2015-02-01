//--------------------------------------------------------------------------------------
// Utility.h -- General useful stuff.
//--------------------------------------------------------------------------------------

#pragma once

namespace MicropolygonCommon
{

//--------------------------------------------------------------------------------------
// Timing related functionality. Singleton.
//--------------------------------------------------------------------------------------
class cTiming
{
public:

	// Get time in second. Origin is arbitrary.
	double GetSeconds() const;

	// Get the singleton instance.
	static cTiming& Instance()
	{
		static cTiming instance;
		return instance;
	}

private:

	// Hide constructors.
	cTiming();
	cTiming(const cTiming&);

	// Length of a performance counter tick in seconds.
	double	m_PerfCounterPeriod;
};

}
