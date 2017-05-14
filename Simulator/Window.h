
#pragma once

class Window : public IUnknown
{
public:
	Window (HINSTANCE hInstance, const wchar_t* wndClassName, DWORD exStyle, DWORD style, const RECT& rect, HWND hWndParent, HMENU hMenuOrControlId);
protected:
	virtual ~Window();

	virtual std::optional<LRESULT> WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

public:
	HWND GetHWnd() const { return _hwnd; }
	SIZE GetClientSizePixels() const { return _clientSize; }
	LONG GetClientWidthPixels() const { return _clientSize.cx; }
	LONG GetClientHeightPixels() const { return _clientSize.cy; }

	virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID riid, void** ppvObject) override { return E_NOTIMPL; }
	virtual ULONG STDMETHODCALLTYPE AddRef();
	virtual ULONG STDMETHODCALLTYPE Release();

private:
	ULONG _refCount = 1;
	HWND _hwnd = nullptr;
	SIZE _clientSize;

	static LRESULT CALLBACK WindowProcStatic (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

