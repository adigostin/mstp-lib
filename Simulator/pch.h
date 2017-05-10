
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

_COM_SMARTPTR_TYPEDEF(ID3D11Device, __uuidof(ID3D11Device));
_COM_SMARTPTR_TYPEDEF(ID3D11Device1, __uuidof(ID3D11Device1));
_COM_SMARTPTR_TYPEDEF(ID3D11DeviceContext, __uuidof(ID3D11DeviceContext));
_COM_SMARTPTR_TYPEDEF(ID3D11DeviceContext1, __uuidof(ID3D11DeviceContext1));
_COM_SMARTPTR_TYPEDEF(ID3D11Texture2D, __uuidof(ID3D11Texture2D));

_COM_SMARTPTR_TYPEDEF(IDXGIFactory2, __uuidof(IDXGIFactory2));
_COM_SMARTPTR_TYPEDEF(IDXGIDevice2, __uuidof(IDXGIDevice2));
_COM_SMARTPTR_TYPEDEF(IDXGIAdapter, __uuidof(IDXGIAdapter));
_COM_SMARTPTR_TYPEDEF(IDXGISurface2, __uuidof(IDXGISurface2));
_COM_SMARTPTR_TYPEDEF(IDXGISwapChain1, __uuidof(IDXGISwapChain1));

_COM_SMARTPTR_TYPEDEF(ID2D1Factory, __uuidof(ID2D1Factory));
_COM_SMARTPTR_TYPEDEF(ID2D1RenderTarget, __uuidof(ID2D1RenderTarget));
_COM_SMARTPTR_TYPEDEF(ID2D1Factory1, __uuidof(ID2D1Factory1));
_COM_SMARTPTR_TYPEDEF(ID2D1DeviceContext, __uuidof(ID2D1DeviceContext));
_COM_SMARTPTR_TYPEDEF(ID2D1SolidColorBrush, __uuidof(ID2D1SolidColorBrush));
_COM_SMARTPTR_TYPEDEF(ID2D1StrokeStyle, __uuidof(ID2D1StrokeStyle));

_COM_SMARTPTR_TYPEDEF(IDWriteFactory, __uuidof(IDWriteFactory));
_COM_SMARTPTR_TYPEDEF(IDWriteTextFormat, __uuidof(IDWriteTextFormat));
_COM_SMARTPTR_TYPEDEF(IDWriteTextLayout, __uuidof(IDWriteTextLayout));
