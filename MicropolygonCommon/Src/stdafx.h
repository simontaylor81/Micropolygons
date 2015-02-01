// Disable STL exceptions.
#define _HAS_EXCEPTIONS 0

#pragma warning(push)
#pragma warning(disable:4838)
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
using namespace DirectX;
using namespace DirectX::PackedVector;
#pragma warning(pop)

// For QueryPerformanceCounter
#include <Windows.h>

#include <stdint.h>

#pragma warning(disable:4201) // nonstandard extension used: nameless struct/union
