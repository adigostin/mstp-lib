
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
#include <intrin.h>
#include <iomanip>
#include <locale>
#include <memory>
#include <MsXml6.h>
#include <mutex>
#include <objbase.h>
#include <optional>
#include <queue>
#include <random>
#include <set>
#include <ShlObj.h>
#include <Shlwapi.h>
#include <signal.h>
#include <sstream>
#include <stdint.h>
#include <stdlib.h>
#include <thread>
#include <typeindex>
#include <Unknwn.h>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <wincodec.h>

_COM_SMARTPTR_TYPEDEF(IXMLDOMDocument3, __uuidof(IXMLDOMDocument3));

_COM_SMARTPTR_TYPEDEF(ID2D1Factory, __uuidof(ID2D1Factory));
_COM_SMARTPTR_TYPEDEF(ID2D1RenderTarget, __uuidof(ID2D1RenderTarget));
_COM_SMARTPTR_TYPEDEF(ID2D1Factory, __uuidof(ID2D1Factory));
_COM_SMARTPTR_TYPEDEF(ID2D1HwndRenderTarget, __uuidof(ID2D1HwndRenderTarget));
_COM_SMARTPTR_TYPEDEF(ID2D1SolidColorBrush, __uuidof(ID2D1SolidColorBrush));
_COM_SMARTPTR_TYPEDEF(ID2D1StrokeStyle, __uuidof(ID2D1StrokeStyle));

_COM_SMARTPTR_TYPEDEF(IDWriteFactory, __uuidof(IDWriteFactory));
_COM_SMARTPTR_TYPEDEF(IDWriteTextFormat, __uuidof(IDWriteTextFormat));
_COM_SMARTPTR_TYPEDEF(IDWriteTextLayout, __uuidof(IDWriteTextLayout));

_COM_SMARTPTR_TYPEDEF(IFileDialog, __uuidof(IFileDialog));
_COM_SMARTPTR_TYPEDEF(IFileOpenDialog, __uuidof(IFileOpenDialog));
_COM_SMARTPTR_TYPEDEF(IFileSaveDialog, __uuidof(IFileSaveDialog));
_COM_SMARTPTR_TYPEDEF(IShellItem, __uuidof(IShellItem));