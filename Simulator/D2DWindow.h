
#pragma once
#include "EventManager.h"
#include "Window.h"

class D2DWindow abstract : public Window
{
	using base = Window;
	D2D1_SIZE_F _clientSizeDips;
	bool _painting = false;
	bool _forceFullPresentation;
	ID3D11Device1Ptr _d3dDevice;
	ID3D11DeviceContext1Ptr _d3dDeviceContext;
	IDWriteFactoryPtr _dWriteFactory;
	IDXGIDevice2Ptr _dxgiDevice;
	IDXGIAdapterPtr _dxgiAdapter;
	IDXGIFactory2Ptr _dxgiFactory;
	IDXGISwapChain1Ptr _swapChain;
	ID2D1Factory1Ptr _d2dFactory;
	ID2D1DeviceContextPtr _d2dDeviceContext;

public:
	D2DWindow (HINSTANCE hInstance, DWORD exStyle, DWORD style, const RECT& rect, HWND hWndParent, HMENU hMenuOrControlId, ID3D11DeviceContext1* deviceContext, IDWriteFactory* dWriteFactory);

	D2D1_SIZE_F GetClientSizeDips() const { return _clientSizeDips; }
	float GetClientWidthDips() const { return _clientSizeDips.width; }
	float GetClientHeightDips() const { return _clientSizeDips.height; }
	D2D1_POINT_2F GetDipLocationFromPixelLocation(POINT locationPixels) const;
	POINT GetPixelLocationFromDipLocation(D2D1_POINT_2F locationDips) const;
	D2D1_SIZE_F GetDipSizeFromPixelSize(SIZE sizePixels) const;
	SIZE GetPixelSizeFromDipSize(D2D1_SIZE_F sizeDips) const;

	ID2D1DeviceContext* GetDeviceContext() const { return _d2dDeviceContext; }
	IDWriteFactory* GetDWriteFactory() const { return _dWriteFactory; }

protected:
	EventManager _em;
	virtual std::optional<LRESULT> WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;
	virtual void OnBeforeRender() { }
	virtual void Render(ID2D1DeviceContext* dc) const = 0;
	virtual void OnAfterRender() { }
private:

	void CreateD2DDeviceContext();
	//void ReleaseD2DDeviceContext();
	void ProcessWmPaint (HWND hwnd);
};

