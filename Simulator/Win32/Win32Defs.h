#pragma once
#include <assert.h>
#include "EventManager.h"

struct IDockablePanel;
struct IDockContainer;

class com_exception : public std::exception
{
public:
	HRESULT const _hr;
	com_exception(HRESULT hr) : _hr(hr) { }
};

class win32_exception : public std::exception
{
public:
	DWORD const _lastError;
	win32_exception(DWORD lastError) : _lastError(lastError) { }
};

inline void ThrowIfFailed(HRESULT hr)
{
	if (FAILED(hr))
		throw com_exception(hr);
}

inline void ThrowWin32IfFailed(BOOL bRes)
{
	if (!bRes)
		throw win32_exception(GetLastError());
}

class not_implemented_exception : public std::exception
{
public:
	not_implemented_exception()
		: std::exception("Not implemented.")
	{ }
};

struct GdiObjectDeleter
{
	void operator() (HGDIOBJ object) { ::DeleteObject(object); }
};
typedef std::unique_ptr<std::remove_pointer<HFONT>::type, GdiObjectDeleter> HFONT_unique_ptr;

struct TimerQueueTimerDeleter
{
	void operator() (HANDLE handle) { ::DeleteTimerQueueTimer (nullptr, handle, INVALID_HANDLE_VALUE); }
};
typedef std::unique_ptr<std::remove_pointer<HANDLE>::type, TimerQueueTimerDeleter> TimerQueueTimer_unique_ptr;

// ============================================================================

MIDL_INTERFACE("C5D357E8-4A20-43D8-9C40-0CE4DC7C637C") IWin32Window : public IUnknown
{
	virtual HWND GetHWnd() const = 0;

	bool IsVisible() const;
	RECT GetClientRectPixels() const;
	RECT GetWindowRect() const;
	SIZE GetWindowSize() const;
	SIZE GetClientSize() const;
};

// ============================================================================

enum class Side { Left, Top, Right, Bottom };

MIDL_INTERFACE("E97899CA-925F-43A7-A0C2-F8743A914BAB") IDockContainer : public IWin32Window
{
	virtual RECT GetContentRect() const = 0;
	virtual IDockablePanel* CreatePanel (const char* panelUniqueName, Side side, const wchar_t* title) = 0;
	virtual IDockablePanel* GetPanel (const char* panelUniqueName) const = 0;
	virtual void ResizePanel (IDockablePanel* panel, SIZE size) = 0;
};
_COM_SMARTPTR_TYPEDEF(IDockContainer, __uuidof(IDockContainer));
using DockContainerFactory = IDockContainerPtr(*const)(HINSTANCE hInstance, HWND hWndParent, const RECT& rect);
extern const DockContainerFactory dockContainerFactory;

// ============================================================================

MIDL_INTERFACE("EE540D38-79DC-479B-9619-D253EB9BA812") IDockablePanel : public IWin32Window
{
	struct VisibleChangedEvent : public Event<VisibleChangedEvent, void(IDockablePanel* panel, bool visible)> {};
	struct SplitterDragging : public Event<SplitterDragging, void(IDockablePanel* panel, SIZE proposedSize)> {};

	virtual const std::string& GetUniqueName() const = 0;
	virtual Side GetSide() const = 0;
	virtual POINT GetContentLocation() const = 0;
	virtual SIZE GetContentSize() const = 0;
	virtual VisibleChangedEvent::Subscriber GetVisibleChangedEvent() = 0;
	virtual SplitterDragging::Subscriber GetSplitterDraggingEvent() = 0;
	virtual SIZE GetPanelSizeFromContentSize (SIZE contentSize) const = 0;

	RECT GetContentRect() const
	{
		auto l = GetContentLocation();
		auto s = GetContentSize();
		return RECT { l.x, l.y, l.x + s.cx, l.y + s.cy };
	}

	SIZE GetWindowSize() const
	{
		RECT wr;
		BOOL bRes = ::GetWindowRect(this->GetHWnd(), &wr);
		if (!bRes)
			throw win32_exception(GetLastError());
		return { wr.right - wr.left, wr.bottom - wr.top };
	}
};
_COM_SMARTPTR_TYPEDEF(IDockablePanel, __uuidof(IDockablePanel));
using DockablePanelFactory = IDockablePanelPtr(*const)(HINSTANCE hInstance, const char* panelUniqueName, HWND hWndParent, const RECT& rect, Side side, const wchar_t* title);
extern const DockablePanelFactory dockablePanelFactory;

// ============================================================================

struct IZoomable abstract
{
	virtual D2D1_POINT_2F GetWLocationFromDLocation (D2D1_POINT_2F dLocation) const = 0;
	virtual D2D1_POINT_2F GetDLocationFromWLocation (D2D1_POINT_2F wLocation) const = 0;
	virtual float GetDLengthFromWLength (float wLength) const = 0;
};

