
#pragma once

#define NOMINMAX
#define _USE_MATH_DEFINES
#define WIN32_LEAN_AND_MEAN   // Exclude rarely-used stuff from Windows headers
#define _WIN7_PLATFORM_UPDATE // Needed by wincodec.h
#define _HAS_EXCEPTIONS 0 // Visual C++ headers look at this
#undef __EXCEPTIONS // WIL and Intellisense look at this

#include <WinSDKVer.h>
#define _WIN32_WINNT _WIN32_WINNT_WIN7
#include <SDKDDKVer.h>

// C/C++
#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <intrin.h>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <set>
#include <span>
#include <string_view>
#include <unordered_map>
#include <variant>

// Windows
#define _COM_NO_STANDARD_GUIDS_
#include <comdef.h>
#include <CommCtrl.h>
#include <d2d1_1helper.h>
#include <d3d11_1.h>
#include <dwrite.h>
#include <propvarutil.h>
#include <ShlObj_core.h>
#include <Shlwapi.h>
#include <ShObjIdl_core.h>
#include <Unknwn.h>
#include <VersionHelpers.h>
#include <wincodec.h>
#include <Windows.h>
#include <windowsx.h>
#include <xmllite.h>

// WIL
#include <wil/com.h>
