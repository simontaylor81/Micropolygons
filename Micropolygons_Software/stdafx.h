// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

// Disable STL exceptions.
#define _HAS_EXCEPTIONS 0

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#endif

#include <windows.h>

#pragma warning(push)
#pragma warning(disable:4838)
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
using namespace DirectX;
using namespace DirectX::PackedVector;
#pragma warning(pop)

#include <crtdbg.h>

#include <malloc.h>
#include <limits.h>
#include <stdlib.h>
#include <stdint.h>

#pragma warning(disable:4201) // nonstandard extension used: nameless struct/union
