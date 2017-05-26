
#include "pch.h"
#include "D2DWindow.h"
#include "Win32Defs.h"

using namespace std;
using namespace D2D1;

#pragma comment (lib, "d2d1.lib")
#pragma comment (lib, "dwrite.lib")
#pragma comment (lib, "D3D11.lib")
#pragma comment (lib, "Dxgi.lib")

static ATOM WndClassAtom;
static const wchar_t WndClassName[] = L"D2DWindow-{175802BE-0628-45C0-BC8A-3D27C6F4F0BE}";

D2DWindow::D2DWindow (HINSTANCE hInstance, DWORD exStyle, DWORD style, const RECT& rect, HWND hWndParent, HMENU hMenuOrControlId, IDWriteFactory* dWriteFactory)
	: base(hInstance, WndClassName, exStyle, style, rect, hWndParent, hMenuOrControlId)
	, _dWriteFactory(dWriteFactory)
{
	auto hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, IID_PPV_ARGS(&_d2dFactory));
	ThrowIfFailed(hr);

	CreateD2DDeviceContext();

	float dpiX, dpiY;
	_renderTarget->GetDpi(&dpiX, &dpiY);
	_clientSizeDips.width = GetClientWidthPixels() * 96.0f / dpiX;
	_clientSizeDips.height = GetClientHeightPixels() * 96.0f / dpiY;
}

void D2DWindow::CreateD2DDeviceContext()
{
	D2D1_RENDER_TARGET_PROPERTIES props = {};
	props.type = D2D1_RENDER_TARGET_TYPE_HARDWARE;
	props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
	props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_IGNORE;
	_d2dFactory->GetDesktopDpi (&props.dpiX, &props.dpiY);
	props.usage = D2D1_RENDER_TARGET_USAGE_NONE;
	props.minLevel = D2D1_FEATURE_LEVEL_DEFAULT;

	D2D1_HWND_RENDER_TARGET_PROPERTIES hwndrtprops = { };
	hwndrtprops.hwnd = GetHWnd();
	hwndrtprops.pixelSize = { (UINT32) GetClientWidthPixels(), (UINT32) GetClientHeightPixels() };

	auto hr = _d2dFactory->CreateHwndRenderTarget(&props, &hwndrtprops, &_renderTarget);
	ThrowIfFailed(hr);
}

std::optional<LRESULT> D2DWindow::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	auto resultBaseClass = base::WindowProc(hwnd, uMsg, wParam, lParam);

	if (uMsg == WM_SIZE)
	{
		_renderTarget = nullptr;
		CreateD2DDeviceContext();

		float dpiX, dpiY;
		_renderTarget->GetDpi(&dpiX, &dpiY);
		_clientSizeDips.width = GetClientWidthPixels() * 96.0f / dpiX;
		_clientSizeDips.height = GetClientHeightPixels() * 96.0f / dpiY;

		return 0;
	}

	if (uMsg == WM_ERASEBKGND)
		return 0; // 0 means the window remains marked for erasing, so the fErase member of the PAINTSTRUCT structure will be TRUE.

	if (uMsg == WM_PAINT)
	{
		ProcessWmPaint(hwnd);
		return 0;
	}

	return std::nullopt;
}

void D2DWindow::ProcessWmPaint (HWND hwnd)
{
	if (_painting)
	{
		// We get here when we're called recursively. The only such case I've seen so far is when
		// an assertion fails in code called from this function. We don't want to restart painting
		// cause we'll end up with a stack overflow, so let's return without attempting anything "smart".
		return;
	}

	HRESULT hr;

	// Call this before calculating the update rects, to allow derived classed to invalidate stuff.
	this->OnBeforeRender();

	// -------------------------------------------------
	// draw the stuff

	// Problem: If an assertion fails in code called from this function, the C++ runtime will try to display
	// the assertion message box. It seems that Windows, while processing WM_PAINT, displays message boxes
	// only if the application has called BeginPaint; if the application has not called BeginPaint, Windows
	// will not display the message box, will make sounds when clicking on the application window, and will
	// wait for the user to press Alt before finally displaying it (go figure!)

	PAINTSTRUCT ps;
	::BeginPaint(hwnd, &ps); // this will also hide the caret, if shown.

	_painting = true;

	_renderTarget->BeginDraw();
	_renderTarget->SetTransform(IdentityMatrix());

	this->Render(_renderTarget);

	hr = _renderTarget->EndDraw(); ThrowIfFailed(hr);

	::EndPaint(hwnd, &ps); // this will show the caret in case BeginPaint above hid it.

	this->OnAfterRender();

	assert(_painting);
	_painting = false;
}

D2D1_POINT_2F D2DWindow::GetDipLocationFromPixelLocation(POINT p) const
{
	float dpiX, dpiY;
	_renderTarget->GetDpi(&dpiX, &dpiY);
	return D2D1_POINT_2F{ p.x * 96.0f / dpiX, p.y * 96.0f / dpiY };
}

D2D1_POINT_2F D2DWindow::GetDipLocationFromPixelLocation(float xPixels, float yPixels) const
{
	float dpiX, dpiY;
	_renderTarget->GetDpi(&dpiX, &dpiY);
	return D2D1_POINT_2F{ xPixels * 96.0f / dpiX, yPixels * 96.0f / dpiY };
}

POINT D2DWindow::GetPixelLocationFromDipLocation(D2D1_POINT_2F locationDips) const
{
	float dpiX, dpiY;
	_renderTarget->GetDpi(&dpiX, &dpiY);
	return POINT{ (int)roundf(locationDips.x / 96.0f * dpiX), (int)roundf(locationDips.y / 96.0f * dpiY) };
}

D2D1_SIZE_F D2DWindow::GetDipSizeFromPixelSize(SIZE sz) const
{
	float dpiX, dpiY;
	_renderTarget->GetDpi(&dpiX, &dpiY);
	return D2D1_SIZE_F{ sz.cx * 96.0f / dpiX, sz.cy * 96.0f / dpiY };
}

SIZE D2DWindow::GetPixelSizeFromDipSize(D2D1_SIZE_F sizeDips) const
{
	float dpiX, dpiY;
	_renderTarget->GetDpi(&dpiX, &dpiY);
	return SIZE{ (int)(sizeDips.width / 96.0f * dpiX), (int)(sizeDips.height / 96.0f * dpiY) };
}

ColorF GetD2DSystemColor (int sysColorIndex)
{
	DWORD brg = GetSysColor (sysColorIndex);
	DWORD rgb = ((brg & 0xff0000) >> 16) | (brg & 0xff00) | ((brg & 0xff) << 16);
	return ColorF (rgb);
}

TextLayout TextLayout::Create (IDWriteFactory* dWriteFactory, IDWriteTextFormat* format, const wchar_t* str, float maxWidth)
{
	IDWriteTextLayoutPtr tl;
	auto hr = dWriteFactory->CreateTextLayout(str, (UINT32) wcslen(str), format, (maxWidth != 0) ? maxWidth : 10000, 10000, &tl); ThrowIfFailed(hr);
	DWRITE_TEXT_METRICS metrics;
	hr = tl->GetMetrics(&metrics); ThrowIfFailed(hr);
	return TextLayout { move(tl), metrics };
}

