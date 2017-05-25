
#pragma once
#include "EventManager.h"
#include "Window.h"

class D2DWindow abstract : public Window
{
	using base = Window;
	D2D1_SIZE_F _clientSizeDips;
	bool _painting = false;
	bool _forceFullPresentation;
	IDWriteFactoryPtr _dWriteFactory;
	ID2D1FactoryPtr _d2dFactory;
	ID2D1HwndRenderTargetPtr _renderTarget;

public:
	D2DWindow (HINSTANCE hInstance, DWORD exStyle, DWORD style, const RECT& rect, HWND hWndParent, HMENU hMenuOrControlId, IDWriteFactory* dWriteFactory);

	D2D1_SIZE_F GetClientSizeDips() const { return _clientSizeDips; }
	float GetClientWidthDips() const { return _clientSizeDips.width; }
	float GetClientHeightDips() const { return _clientSizeDips.height; }
	D2D1_POINT_2F GetDipLocationFromPixelLocation(POINT locationPixels) const;
	D2D1_POINT_2F GetDipLocationFromPixelLocation(float xPixels, float yPixels) const;
	POINT GetPixelLocationFromDipLocation(D2D1_POINT_2F locationDips) const;
	D2D1_SIZE_F GetDipSizeFromPixelSize(SIZE sizePixels) const;
	SIZE GetPixelSizeFromDipSize(D2D1_SIZE_F sizeDips) const;

	ID2D1HwndRenderTarget* GetRenderTarget() const { return _renderTarget; }
	IDWriteFactory* GetDWriteFactory() const { return _dWriteFactory; }

protected:
	virtual std::optional<LRESULT> WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;
	virtual void OnBeforeRender() { }
	virtual void Render(ID2D1RenderTarget* dc) const = 0;
	virtual void OnAfterRender() { }
private:

	void CreateD2DDeviceContext();
	//void ReleaseD2DDeviceContext();
	void ProcessWmPaint (HWND hwnd);
};

