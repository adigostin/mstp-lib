
#pragma once

#define NOMINMAX
#define _USE_MATH_DEFINES

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

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
#include <assert.h>
#include <codecvt>
#include <comdef.h>
#include <Commctrl.h>
#include <cstdint>
#include <d2d1_1.h>
#include <d3d11_1.h>
#include <dwrite_1.h>
#include <functional>
#include <locale>
#include <memory>
#include <mutex>
#include <objbase.h>
#include <optional>
#include <Shlwapi.h>
#include <signal.h>
#include <sstream>
#include <stdint.h>
#include <stdlib.h>
#include <thread>
#include <typeindex>
#include <UIRibbon.h>
#include <UIRibbonPropertyHelpers.h>
#include <Unknwn.h>
#include <unordered_map>
#include <unordered_set>
#include <wincodec.h>
#include "Win32Defs.h"
