// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

// Disable STL exceptions.
#define _HAS_EXCEPTIONS 0

// Exclude rarely-used stuff from Windows headers
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#endif

#include <windows.h>

// D3D10
#include <d3dx10.h>

// Assert stuff
#include <crtdbg.h>
