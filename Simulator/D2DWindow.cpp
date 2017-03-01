
#include "pch.h"
#include "D2DWindow.h"

using namespace std;
using namespace D2D1;

#pragma comment (lib, "d2d1.lib")
#pragma comment (lib, "dwrite.lib")
#pragma comment (lib, "D3D11.lib")
#pragma comment (lib, "Dxgi.lib")

static ATOM WndClassAtom;
static const wchar_t WndClassName[] = L"D2DWindow-{175802BE-0628-45C0-BC8A-3D27C6F4F0BE}";

D2DWindow::D2DWindow (DWORD exStyle, DWORD style, const RECT& rect, HWND hWndParent, DWORD controlId, ID3D11DeviceContext1* deviceContext, IDWriteFactory* dWriteFactory)
	: _d3dDeviceContext(deviceContext)
	, _dWriteFactory(dWriteFactory)
{
	HINSTANCE hInstance;
	BOOL bRes = GetModuleHandleEx (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR) WndClassName, &hInstance);
	if (!bRes)
		throw win32_exception(GetLastError());

	ComPtr<ID3D11Device> device;
	deviceContext->GetDevice(&device);
	auto hr = device->QueryInterface(IID_PPV_ARGS(&_d3dDevice)); ThrowIfFailed(hr);

	hr = device->QueryInterface(IID_PPV_ARGS(&_dxgiDevice)); ThrowIfFailed(hr);

	hr = _dxgiDevice->GetAdapter(&_dxgiAdapter); ThrowIfFailed(hr);

	hr = _dxgiAdapter->GetParent(IID_PPV_ARGS(&_dxgiFactory)); ThrowIfFailed(hr);

	hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, IID_PPV_ARGS(&_d2dFactory)); ThrowIfFailed(hr);

	if (WndClassAtom == 0)
	{
		WNDCLASSEX wcex;
		wcex.cbSize = sizeof (wcex);
		wcex.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = &WindowProcStatic;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = hInstance;
		wcex.hIcon = nullptr;
		wcex.hCursor = LoadCursor (nullptr, IDC_ARROW);
		wcex.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
		wcex.lpszMenuName = nullptr;
		wcex.lpszClassName = WndClassName;
		wcex.hIconSm = 0;
		WndClassAtom = RegisterClassEx (&wcex);
		if (WndClassAtom == 0)
			throw win32_exception(GetLastError());
	}

	auto hwnd = ::CreateWindowEx (exStyle, WndClassName, L"", style, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, hWndParent, (HMENU) controlId, hInstance, this);
	if (hwnd == nullptr)
		throw win32_exception(GetLastError());
	assert (hwnd == _hwnd);
}

D2DWindow::~D2DWindow()
{
	if (_hwnd != nullptr)
		::DestroyWindow(_hwnd);
}

void D2DWindow::CreateD2DDeviceContext()
{
	assert(_d2dDeviceContext == nullptr);

	ComPtr<IDXGISurface2> dxgiSurface;
	auto hr = _swapChain->GetBuffer(0, IID_PPV_ARGS(&dxgiSurface)); ThrowIfFailed(hr);

	D2D1_RENDER_TARGET_PROPERTIES props = {};
	props.type = D2D1_RENDER_TARGET_TYPE_HARDWARE;
	props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
	props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_IGNORE;
	_d2dFactory->GetDesktopDpi (&props.dpiX, &props.dpiY);
	props.usage = D2D1_RENDER_TARGET_USAGE_NONE;
	props.minLevel = D2D1_FEATURE_LEVEL_DEFAULT;
	ComPtr<ID2D1RenderTarget> rt;
	hr = _d2dFactory->CreateDxgiSurfaceRenderTarget(dxgiSurface, &props, &rt); ThrowIfFailed(hr);
	hr = rt->QueryInterface(&_d2dDeviceContext); ThrowIfFailed(hr);
}

// From http://blogs.msdn.com/b/oldnewthing/archive/2005/04/22/410773.aspx
//static
LRESULT CALLBACK D2DWindow::WindowProcStatic (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	//if (AssertFunctionRunning)
	//{
	//	// Let's try not to run application code while the assertion dialog is shown. We'll probably mess things up even more.
	//	return DefWindowProc (hwnd, uMsg, wParam, lParam);
	//}

	D2DWindow* window;
	if (uMsg == WM_NCCREATE)
	{
		LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
		window = reinterpret_cast<D2DWindow*>(lpcs->lpCreateParams);
		window->_hwnd = hwnd;
		SetWindowLongPtr (hwnd, GWLP_USERDATA, reinterpret_cast<LPARAM>(window));
	}
	else
		window = reinterpret_cast<D2DWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

	if (window == nullptr)
	{
		// this must be one of those messages sent before WM_NCCREATE or after WM_NCDESTROY.
		return DefWindowProc (hwnd, uMsg, wParam, lParam);
	}

	auto result = window->WindowProc (hwnd, uMsg, wParam, lParam);

	if (uMsg == WM_NCDESTROY)
	{
		window->_hwnd = nullptr;
		SetWindowLongPtr (hwnd, GWLP_USERDATA, 0);
	}

	if (result)
		return result.value();

	return ::DefWindowProc(hwnd, uMsg, wParam, lParam);
}

std::optional<LRESULT> D2DWindow::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_CREATE)
	{
		_clientSize.cx = ((CREATESTRUCT*)lParam)->cx;
		_clientSize.cy = ((CREATESTRUCT*)lParam)->cy;

		DXGI_SWAP_CHAIN_DESC1 desc;
		desc.Width = std::max((LONG) 8, _clientSize.cx);
		desc.Height = std::max((LONG)8, _clientSize.cy);
		desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		desc.Stereo = FALSE;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.BufferCount = 1;
		desc.Scaling = DXGI_SCALING_STRETCH;
		desc.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;// DXGI_SWAP_EFFECT_DISCARD;// DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
		desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
		desc.Flags = 0;
		auto hr = _dxgiFactory->CreateSwapChainForHwnd(_d3dDevice, _hwnd, &desc, nullptr, nullptr, &_swapChain); ThrowIfFailed(hr);
		_forceFullPresentation = true;

		CreateD2DDeviceContext();

		float dpiX, dpiY;
		_d2dDeviceContext->GetDpi(&dpiX, &dpiY);
		_clientSizeDips.width = _clientSize.cx * 96.0f / dpiX;
		_clientSizeDips.height = _clientSize.cy * 96.0f / dpiY;

		return 0;
	}
	
	if (uMsg == WM_SIZE)
	{
		_clientSize = { LOWORD(lParam), HIWORD(lParam) };

		_d2dDeviceContext = nullptr;
		auto hr = _swapChain->ResizeBuffers (0, std::max ((LONG)8, _clientSize.cx), std::max((LONG)8, _clientSize.cy), DXGI_FORMAT_UNKNOWN, 0); ThrowIfFailed(hr);
		CreateD2DDeviceContext();

		float dpiX, dpiY;
		_d2dDeviceContext->GetDpi(&dpiX, &dpiY);
		_clientSizeDips.width = _clientSize.cx * 96.0f / dpiX;
		_clientSizeDips.height = _clientSize.cy * 96.0f / dpiY;

		return 0;
	}

	if (uMsg == WM_ERASEBKGND)
		return 0; // 0 means the window remains marked for erasing, so the fErase member of the PAINTSTRUCT structure will be TRUE.

	if (uMsg == WM_PAINT)
	{
		ProcessWmPaint();
		return 0;
	}

	return std::nullopt;
}

void D2DWindow::ProcessWmPaint()
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
	::BeginPaint(_hwnd, &ps); // this will also hide the caret, if shown.

	_painting = true;

	ComPtr<ID3D11Texture2D> backBuffer;
	hr = _swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)); ThrowIfFailed(hr);

	_d2dDeviceContext->BeginDraw();
	_d2dDeviceContext->SetTransform(IdentityMatrix());
	
	this->Render(_d2dDeviceContext);

	hr = _d2dDeviceContext->EndDraw(); ThrowIfFailed(hr);

	DXGI_PRESENT_PARAMETERS pp = {};
	hr = _swapChain->Present1(0, 0, &pp); ThrowIfFailed(hr);

	::EndPaint(_hwnd, &ps); // this will show the caret in case BeginPaint above hid it.
	
	this->OnAfterRender();

	assert(_painting);
	_painting = false;
}

D2D1_POINT_2F D2DWindow::GetDipLocationFromPixelLocation(POINT p) const
{
	float dpiX, dpiY;
	_d2dDeviceContext->GetDpi(&dpiX, &dpiY);
	return D2D1_POINT_2F{ p.x * 96.0f / dpiX, p.y * 96.0f / dpiY };
}

POINT D2DWindow::GetPixelLocationFromDipLocation(D2D1_POINT_2F locationDips) const
{
	float dpiX, dpiY;
	_d2dDeviceContext->GetDpi(&dpiX, &dpiY);
	return POINT{ (int)(locationDips.x / 96.0f * dpiX), (int)(locationDips.y / 96.0f * dpiY) };
}

D2D1_SIZE_F D2DWindow::GetDipSizeFromPixelSize(SIZE sz) const
{
	float dpiX, dpiY;
	_d2dDeviceContext->GetDpi(&dpiX, &dpiY);
	return D2D1_SIZE_F{ sz.cx * 96.0f / dpiX, sz.cy * 96.0f / dpiY };
}

SIZE D2DWindow::GetPixelSizeFromDipSize(D2D1_SIZE_F sizeDips) const
{
	float dpiX, dpiY;
	_d2dDeviceContext->GetDpi(&dpiX, &dpiY);
	return SIZE{ (int)(sizeDips.width / 96.0f * dpiX), (int)(sizeDips.height / 96.0f * dpiY) };
}
