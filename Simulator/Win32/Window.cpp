
#include "pch.h"
#include "Window.h"
#include "Win32Defs.h"

Window::Window (HINSTANCE hInstance, const wchar_t* wndClassName, DWORD exStyle, DWORD style, const RECT& rect, HWND hWndParent, HMENU hMenuOrControlId)
{
	WNDCLASSEX wcex;
	BOOL bRes = ::GetClassInfoEx (hInstance, wndClassName, &wcex);
	if (!bRes)
	{
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
		wcex.lpszClassName = wndClassName;
		wcex.hIconSm = 0;
		auto atom = RegisterClassEx (&wcex); assert (atom != 0);
	}

	int x = rect.left;
	int y = rect.top;
	int w = rect.right - rect.left;
	int h = rect.bottom - rect.top;
	auto hwnd = ::CreateWindowEx (exStyle, wndClassName, L"", style, x, y, w, h, hWndParent, hMenuOrControlId, hInstance, this); assert (hwnd != nullptr);
	assert (hwnd == _hwnd);
}


Window::~Window()
{
	if (_hwnd != nullptr)
		::DestroyWindow(_hwnd);
}

// From http://blogs.msdn.com/b/oldnewthing/archive/2005/04/22/410773.aspx
//static
LRESULT CALLBACK Window::WindowProcStatic (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	//if (AssertFunctionRunning)
	//{
	//	// Let's try not to run application code while the assertion dialog is shown. We'll probably mess things up even more.
	//	return DefWindowProc (hwnd, uMsg, wParam, lParam);
	//}

	Window* window;
	if (uMsg == WM_NCCREATE)
	{
		LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
		window = reinterpret_cast<Window*>(lpcs->lpCreateParams);
		window->AddRef();
		window->_hwnd = hwnd;
		SetWindowLongPtr (hwnd, GWLP_USERDATA, reinterpret_cast<LPARAM>(window));
	}
	else
		window = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

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
		window->Release();
	}

	if (result)
		return result.value();

	return ::DefWindowProc(hwnd, uMsg, wParam, lParam);
}

std::optional<LRESULT> Window::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_CREATE)
	{
		_clientSize.cx = ((CREATESTRUCT*)lParam)->cx;
		_clientSize.cy = ((CREATESTRUCT*)lParam)->cy;
		return 0;
	}

	if (uMsg == WM_SIZE)
	{
		_clientSize = { LOWORD(lParam), HIWORD(lParam) };
		return 0;
	}

	return std::nullopt;
}

ULONG Window::AddRef()
{
	return InterlockedIncrement(&_refCount);
}

ULONG Window::Release()
{
	assert (_refCount > 0);
	ULONG newRefCount = InterlockedDecrement(&_refCount);
	if (newRefCount == 0)
		delete this;
	return newRefCount;
}
