
#include "pch.h"
#include "SimulatorInterfaces.h"
#include "ZoomableWindow.h"
#include "Ribbon/RibbonIds.h"

using namespace std;
using namespace D2D1;

class EditArea : public ZoomableWindow, public IEditArea
{
	typedef ZoomableWindow base;

	IUIFramework* const _rf;
	ComPtr<ISelection> const _selection;

public:
	EditArea(IProject* project, IProjectWindow* pw, ISelection* selection, IUIFramework* rf, const RECT& rect, ID3D11DeviceContext1* deviceContext, IDWriteFactory* dWriteFactory, IWICImagingFactory2* wicFactory)
		: base(0, WS_CHILD | WS_VISIBLE, rect, pw->GetHWnd(), 55, deviceContext, dWriteFactory, wicFactory)
		, _rf(rf), _selection(selection)
	{ }

	virtual ~EditArea()
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

	virtual std::optional<LRESULT> WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override
	{
		if (uMsg == WM_CONTEXTMENU)
			return ProcessWmContextMenu (hwnd, POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });

		return base::WindowProc (hwnd, uMsg, wParam, lParam);
	}

	std::optional<LRESULT> ProcessWmContextMenu (HWND hwnd, POINT pt)
	{
		//D2D1_POINT_2F dipLocation = GetDipLocationFromPixelLocation(pt);
		//_elementsAtContextMenuLocation.clear();
		//GetElementsAt(_project->GetInnerRootElement(), { dipLocation.x, dipLocation.y }, _elementsAtContextMenuLocation);

		UINT32 viewId;
		if (_selection->GetObjects().empty())
			viewId = cmdContextMenuBlankArea;
		else if (dynamic_cast<Bridge*>(_selection->GetObjects()[0]) != nullptr)
			viewId = cmdContextMenuBridge;
		else if (dynamic_cast<Port*>(_selection->GetObjects()[0]) != nullptr)
			viewId = cmdContextMenuPort;
		else
			throw NotImplementedException();

		ComPtr<IUIContextualUI> ui;
		auto hr = _rf->GetView(viewId, IID_PPV_ARGS(&ui)); ThrowIfFailed(hr);
		hr = ui->ShowAtLocation(pt.x, pt.y); ThrowIfFailed(hr);
		return 0;
	}
};

extern const EditAreaFactory editAreaFactory = [](auto... params) { return unique_ptr<IEditArea>(new EditArea(params...)); };
