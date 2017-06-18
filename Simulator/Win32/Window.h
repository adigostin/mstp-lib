
#pragma once
#include "Win32Defs.h"
#include "EventManager.h"

class Window : public EventManager, public IWin32Window
{
public:
	Window (HINSTANCE hInstance, const wchar_t* wndClassName, DWORD exStyle, DWORD style, const RECT& rect, HWND hWndParent, HMENU hMenuOrControlId);
protected:
	virtual ~Window();

	virtual std::optional<LRESULT> WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

public:
	SIZE GetClientSizePixels() const { return _clientSize; }
	LONG GetClientWidthPixels() const { return _clientSize.cx; }
	LONG GetClientHeightPixels() const { return _clientSize.cy; }

	virtual HWND GetHWnd() const override { return _hwnd; }
	virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID riid, void** ppvObject) override { return E_NOINTERFACE; }
	virtual ULONG STDMETHODCALLTYPE AddRef() override ;
	virtual ULONG STDMETHODCALLTYPE Release() override ;

private:
	ULONG _refCount = 1;
	HWND _hwnd = nullptr;
	SIZE _clientSize;

	static LRESULT CALLBACK WindowProcStatic (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

