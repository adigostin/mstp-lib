
#pragma once

#define NOMINMAX
#define _USE_MATH_DEFINES
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define _HAS_EXCEPTIONS 0

#include "targetver.h"

#define _WIN7_PLATFORM_UPDATE // needed by wincodec.h

// From https://msdn.microsoft.com/en-us/library/974tc9t1.aspx
#include <crtdbg.h>
#include <malloc.h>

#ifdef _DEBUG
#define new new(_CLIENT_BLOCK, __FILE__, __LINE__)
#endif

#include <Windows.h>
#include <windowsx.h>
#undef DrawText

#include <algorithm>
#include <array>
#include <codecvt>
#include <comdef.h>
#include <Commctrl.h>
#include <cstdint>
#include <d2d1_1.h>
#include <d3d11_1.h>
#include <dwrite_1.h>
#include <functional>
#include <intrin.h>
#include <iomanip>
#include <locale>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <random>
#include <set>
#include <sstream>
#include <stdint.h>
#include <stdlib.h>
#include <thread>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <variant>

#include <MsXml6.h>
#include <ShlObj.h>
#include <Shlwapi.h>
#include <Unknwn.h>
#include <VersionHelpers.h>
#include <wincodec.h>
