// General maths stuff.

#pragma once

//--------------------------------------------------------------------------------------
// Basic min and max functions.
//--------------------------------------------------------------------------------------
template <class T>
inline const T& Min(const T& a, const T& b)
{
	return a < b ? a : b;
}
template <class T>
inline const T& Max(const T& a, const T& b)
{
	return a > b ? a : b;
}

//--------------------------------------------------------------------------------------
// Simple clamp function.
//--------------------------------------------------------------------------------------
template <class T>
inline const T& Clamp(const T& X, const T& min, const T& max)
{
	return Min(Max(X, min), max);
}

//--------------------------------------------------------------------------------------
// Linear interpolation template function.
//--------------------------------------------------------------------------------------
template <class T>
inline T Lerp(const T& X, const T& Y, float alpha)
{
	return (1.0f - alpha) * X + alpha * Y;
}
