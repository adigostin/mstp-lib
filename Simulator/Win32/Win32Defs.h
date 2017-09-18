#pragma once
#include "EventManager.h"

struct IDockablePanel;
struct IDockContainer;

struct GdiObjectDeleter
{
	void operator() (HGDIOBJ object) { ::DeleteObject(object); }
};
typedef std::unique_ptr<std::remove_pointer<HFONT>::type, GdiObjectDeleter> HFONT_unique_ptr;

struct IWin32Window : public IUnknown
{
	virtual HWND GetHWnd() const = 0;

	bool IsVisible() const
	{
		return (GetWindowLongPtr (GetHWnd(), GWL_STYLE) & WS_VISIBLE) != 0;
	}

	RECT GetClientRectPixels() const
	{
		RECT rect;
		BOOL bRes = ::GetClientRect (GetHWnd(), &rect); assert(bRes);
		return rect;
	};

	SIZE GetClientSize() const
	{
		RECT rect = this->GetClientRectPixels();
		return SIZE { rect.right, rect.bottom };
	}

	RECT GetRect() const
	{
		auto hwnd = this->GetHWnd();
		auto parent = ::GetParent(hwnd); assert (parent != nullptr);
		RECT rect;
		BOOL bRes = ::GetWindowRect (hwnd, &rect); assert(bRes);
		MapWindowPoints (HWND_DESKTOP, parent, (LPPOINT) &rect, 2);
		return rect;
	}

private:
	static POINT GetLocation (const RECT& rect) { return { rect.left, rect.top }; }
	static SIZE GetSize (const RECT& rect) { return { rect.right - rect.left, rect.bottom - rect.top }; }
	static LONG GetWidth (const RECT& rect) { return rect.right - rect.left; }
	static LONG GetHeight (const RECT& rect) { return rect.bottom - rect.top; }

public:
	LONG GetX() const { return GetRect().left; }
	LONG GetY() const { return GetRect().top; }
	POINT GetLocation() const { return GetLocation(GetRect()); }
	LONG GetWidth() const { return GetWidth(GetRect()); }
	LONG GetHeight() const { return GetHeight(GetRect()); }
	SIZE GetSize() const { return GetSize(GetRect()); }

	void SetRect (const RECT& rect)
	{
		BOOL bRes = ::MoveWindow (GetHWnd(), rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, TRUE);
		assert(bRes);
	}

	void SetX (LONG x)
	{
		auto rect = GetRect();
		rect.right = x + rect.right - rect.left;
		rect.left = x;
		SetRect (rect);
	}

	void SetY (LONG y);
	void SetLocation (POINT pt);

	void SetWidth (LONG width)
	{
		auto rect = GetRect();
		rect.right = rect.left + width;
		SetRect(rect);
	}

	void SetHeight (LONG height);
	void SetSize (SIZE size);
};

struct IWindowWithWorkQueue : IWin32Window
{
	virtual void PostWork (std::function<void()>&& work) = 0;
};

struct IZoomable abstract
{
	virtual D2D1_POINT_2F GetWLocationFromDLocation (D2D1_POINT_2F dLocation) const = 0;
	virtual D2D1_POINT_2F GetDLocationFromWLocation (D2D1_POINT_2F wLocation) const = 0;
	virtual float GetDLengthFromWLength (float wLength) const = 0;
};

