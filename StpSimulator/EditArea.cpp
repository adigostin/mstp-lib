
#include "pch.h"
#include "SimulatorInterfaces.h"
#include "ZoomableWindow.h"

using namespace std;
using namespace D2D1;

class EditArea : public ZoomableWindow, public IEditArea
{
	typedef ZoomableWindow base;

public:
	EditArea(IProject* project, IProjectWindow* pw, const RECT& rect, ID3D11DeviceContext1* deviceContext, IDWriteFactory* dWriteFactory, IWICImagingFactory2* wicFactory)
		: base(0, WS_CHILD | WS_VISIBLE, rect, pw->GetHWnd(), 55, deviceContext, dWriteFactory, wicFactory)
	{
	}

	virtual void Render(ID2D1DeviceContext* dc) const override final
	{
		dc->Clear (ColorF(ColorF::Indigo));

		D2D1_MATRIX_3X2_F oldtr;
		dc->GetTransform(&oldtr);
		dc->SetTransform (base::GetZoomTransform());

		ComPtr<ID2D1SolidColorBrush> brush;
		dc->CreateSolidColorBrush (ColorF(ColorF::Red), &brush);
		dc->DrawRectangle ({ 0, 0, 800, 600 }, brush, 4.0f);

		dc->SetTransform(oldtr);
	}

	virtual HWND GetHWnd() const override final { return base::GetHWnd(); }
};

extern EditAreaFactory* const editAreaFactory = [](auto... params) { return unique_ptr<IEditArea>(new EditArea(params...)); };
