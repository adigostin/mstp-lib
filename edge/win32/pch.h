
#pragma once

#define NOMINMAX
#define _USE_MATH_DEFINES
#define WIN32_LEAN_AND_MEAN   // Exclude rarely-used stuff from Windows headers
#define _WIN7_PLATFORM_UPDATE // Needed by wincodec.h

#include "targetver.h"

// From https://msdn.microsoft.com/en-us/library/974tc9t1.aspx
#include <crtdbg.h>
#include <malloc.h>

#ifdef _DEBUG
#define new new(_CLIENT_BLOCK, __FILE__, __LINE__)
#endif

// C/C++
#include <array>
#include <cstdint>
#include <cmath>
#include <exception>
#include <functional>
#include <intrin.h>
#include <memory>
#include <optional>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <variant>

#include <comdef.h>
#include <CommCtrl.h>
#include <d2d1_1helper.h>
#include <d3d11_1.h>
#include <dwrite.h>
#include <Shlwapi.h>
#include <Unknwn.h>
#include <VersionHelpers.h>
#include <wincodec.h>
#include <Windows.h>
#include <windowsx.h>

#include "..\assert.h"
#include "..\span.hpp"
