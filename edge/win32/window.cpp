
#include "pch.h"
#include "window.h"

using namespace edge;

void window::register_class (HINSTANCE hInstance, const wnd_class_params& class_params)
{
	WNDCLASSEX wcex;
	BOOL bRes = ::GetClassInfoEx (hInstance, class_params.lpszClassName, &wcex);
	if (!bRes)
	{
		wcex.cbSize = sizeof(wcex);
		wcex.style = class_params.style;
		wcex.lpfnWndProc = &WindowProcStatic;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = hInstance;
		wcex.hIcon = class_params.lpIconName ? ::LoadIcon(hInstance, class_params.lpIconName) : nullptr;
		wcex.hCursor = LoadCursor (nullptr, IDC_ARROW);
		wcex.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
		wcex.lpszMenuName = class_params.lpszMenuName;
		wcex.lpszClassName = class_params.lpszClassName;
		wcex.hIconSm = class_params.lpIconSmName ? ::LoadIcon(hInstance, class_params.lpIconSmName) : nullptr;
		auto atom = RegisterClassEx (&wcex); assert (atom != 0);
	}
}

window::window (HINSTANCE hInstance, const wnd_class_params& class_params, DWORD exStyle, DWORD style, int x, int y, int width, int height, HWND hWndParent, HMENU hMenu)
{
	register_class (hInstance, class_params);

	auto hwnd = ::CreateWindowEx (exStyle, class_params.lpszClassName, L"", style, x, y, width, height, hWndParent, hMenu, hInstance, this); assert (hwnd != nullptr);
	assert (hwnd == _hwnd);
}

static const wnd_class_params child_wnd_class_params = 
{
	L"window-{0F45B203-6AE9-49B0-968A-4006176EDA40}", // lpszClassName
	CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW, // style
	nullptr, // lpszMenuName
	nullptr, // lpIconName
	nullptr, // lpIconSmName
};

window::window (HINSTANCE hInstance, DWORD exStyle, DWORD style, const RECT& rect, HWND hWndParent, int child_control_id)
	: window (hInstance, child_wnd_class_params, exStyle, style,
			  rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
			  hWndParent, (HMENU)(size_t)child_control_id)
{ }

window::~window()
{
	if (_hwnd != nullptr)
		::DestroyWindow(_hwnd);
}

// From http://blogs.msdn.com/b/oldnewthing/archive/2005/04/22/410773.aspx
//static
LRESULT CALLBACK window::WindowProcStatic (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (assert_function_running)
	{
		// Let's try not to run application code while the assertion dialog is shown. We'll probably mess things up even more.
		return DefWindowProc (hwnd, uMsg, wParam, lParam);
	}

	window* wnd;
	if (uMsg == WM_NCCREATE)
	{
		LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
		wnd = reinterpret_cast<window*>(lpcs->lpCreateParams);
		wnd->_hwnd = hwnd;
		SetWindowLongPtr (hwnd, GWLP_USERDATA, reinterpret_cast<LPARAM>(wnd));
	}
	else
		wnd = reinterpret_cast<window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

	if (wnd == nullptr)
	{
		// this must be one of those messages sent before WM_NCCREATE or after WM_NCDESTROY.
		return DefWindowProc (hwnd, uMsg, wParam, lParam);
	}

	auto result = wnd->window_proc (hwnd, uMsg, wParam, lParam);

	if (uMsg == WM_NCDESTROY)
	{
		wnd->_hwnd = nullptr;
		SetWindowLongPtr (hwnd, GWLP_USERDATA, 0);
	}

	if (result.has_value())
		return result.value();

	return ::DefWindowProc(hwnd, uMsg, wParam, lParam);
}

std::optional<LRESULT> window::window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_CREATE)
	{
		_clientSize.cx = ((CREATESTRUCT*)lParam)->cx;
		_clientSize.cy = ((CREATESTRUCT*)lParam)->cy;
		return 0;
	}

	if (uMsg == WM_DESTROY)
	{
		this->event_invoker<destroying_event>()(this);
		return 0;
	}

	if (uMsg == WM_SIZE)
	{
		_clientSize = { LOWORD(lParam), HIWORD(lParam) };
		return 0;
	}

	return std::nullopt;
}

// static
UINT window::GetModifierKeys()
{
	UINT keys = 0;

	if (GetKeyState (VK_SHIFT) < 0)
		keys |= VK_SHIFT;

	if (GetKeyState (VK_CONTROL) < 0)
		keys |= VK_CONTROL;

	if (GetKeyState (VK_MENU) < 0)
		keys |= VK_MENU;

	return keys;
}
