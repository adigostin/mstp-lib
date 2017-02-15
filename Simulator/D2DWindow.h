
#pragma once
#include "Win32Defs.h"

struct HWndDeleter
{
	void operator() (HWND hWnd) { ::DestroyWindow(hWnd); }
};
typedef std::unique_ptr<std::remove_pointer<HWND>::type, HWndDeleter> HWND_unique_ptr;

class D2DWindow abstract : public IWin32Window
{
	HWND _hwnd;
	RECT _clientRect;
	bool _painting = false;
	bool _forceFullPresentation;
	ComPtr<ID3D11Device1> _d3dDevice;
	ComPtr<ID3D11DeviceContext1> _d3dDeviceContext;
	ComPtr<IDWriteFactory> _dWriteFactory;
	ComPtr<IWICImagingFactory2> _wicFactory;
	ComPtr<IDXGIDevice2> _dxgiDevice;
	ComPtr<IDXGIAdapter> _dxgiAdapter;
	ComPtr<IDXGIFactory2> _dxgiFactory;
	ComPtr<IDXGISwapChain1> _swapChain;
	ComPtr<ID2D1Factory1> _d2dFactory;
	ComPtr<ID2D1DeviceContext> _d2dDeviceContext;

public:
	D2DWindow (DWORD exStyle, DWORD style, const RECT& rect, HWND hWndParent, DWORD controlId,
		ID3D11DeviceContext1* deviceContext, IDWriteFactory* dWriteFactory, IWICImagingFactory2* wicFactory);
	~D2DWindow();

	virtual HWND GetHWnd() const override { return _hwnd; }
	const RECT& GetClientRect() const { return _clientRect; }
	D2D1_RECT_F GetClientRectDips() const;
	D2D1_POINT_2F GetDipLocationFromPixelLocation(POINT locationPixels) const;
	POINT GetPixelLocationFromDipLocation(D2D1_POINT_2F locationDips) const;
	D2D1_SIZE_F GetDipSizeFromPixelSize(SIZE sizePixels) const;
	SIZE GetPixelSizeFromDipSize(D2D1_SIZE_F sizeDips) const;

protected:
	virtual std::optional<LRESULT> WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual void OnBeforeRender() { }
	virtual void Render(ID2D1DeviceContext* dc) const = 0;
	virtual void OnAfterRender() { }

private:
	static LRESULT CALLBACK WindowProcStatic (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	void CreateD2DDeviceContext();
	//void ReleaseD2DDeviceContext();
	void ProcessWmPaint();
};

