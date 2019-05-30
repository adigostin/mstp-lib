
#include "win32_lib.h"

namespace edge
{
	bool win32_window_i::IsVisible() const
	{
		return (GetWindowLongPtr (hwnd(), GWL_STYLE) & WS_VISIBLE) != 0;
	}

	RECT win32_window_i::client_rect_pixels() const
	{
		RECT rect;
		BOOL bRes = ::GetClientRect (hwnd(), &rect); assert(bRes);
		return rect;
	};

	SIZE win32_window_i::client_size_pixels() const
	{
		RECT rect = this->client_rect_pixels();
		return SIZE { rect.right, rect.bottom };
	}

	RECT win32_window_i::GetRect() const
	{
		auto hwnd = this->hwnd();
		auto parent = ::GetParent(hwnd); assert (parent != nullptr);
		RECT rect;
		BOOL bRes = ::GetWindowRect (hwnd, &rect); assert(bRes);
		MapWindowPoints (HWND_DESKTOP, parent, (LPPOINT) &rect, 2);
		return rect;
	}

	POINT win32_window_i::GetLocation() const
	{
		auto rect = GetRect();
		return { rect.left, rect.top };
	}

	LONG win32_window_i::GetWidth() const
	{
		RECT rect;
		::GetWindowRect (hwnd(), &rect);
		return rect.right - rect.left;
	}

	LONG win32_window_i::GetHeight() const
	{
		RECT rect;
		::GetWindowRect (hwnd(), &rect);
		return rect.bottom - rect.top;
	}

	SIZE win32_window_i::GetSize() const
	{
		RECT rect;
		::GetWindowRect (hwnd(), &rect);
		return { rect.right - rect.left, rect.bottom - rect.top };
	}
}
