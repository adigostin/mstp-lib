
#pragma once

#define NOMINMAX
#define _USE_MATH_DEFINES
#define WIN32_LEAN_AND_MEAN   // Exclude rarely-used stuff from Windows headers
#define _WIN7_PLATFORM_UPDATE // Needed by wincodec.h
#define _HAS_EXCEPTIONS 0 // Visual C++ headers look at this
#undef __EXCEPTIONS // WIL and Intellisense look at this

#include "targetver.h"

#ifdef _DEBUG
	#include <crtdbg.h>
#else
	#define _DEBUG
	#include <crtdbg.h>
	#undef _DEBUG
#endif

// C/C++
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <intrin.h>
#include <iomanip>
#include <locale>
#include <optional>
#include <queue>
#include <random>
#include <set>
#include <span>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <variant>

// Windows
#define _COM_NO_STANDARD_GUIDS_
#include <d2d1_1.h>
#include <d3d11_1.h>
#include <dwrite_1.h>
#include <shellapi.h>
#include <ShlObj.h>
#include <Shlwapi.h>
#include <Uxtheme.h>
#include <VersionHelpers.h>
#include <wincodec.h>
#include <Windows.h>
#include <windowsx.h>
#undef DrawText

// WIL
#include <wil/com.h>
