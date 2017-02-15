
#include "pch.h"
#include "SimulatorInterfaces.h"
#include "ZoomableWindow.h"
#include "Ribbon/RibbonIds.h"

using namespace std;
using namespace D2D1;

class EditArea : public ZoomableWindow, public IEditArea
{
	typedef ZoomableWindow base;

	ULONG _refCount = 1;
	IUIFramework* const _rf;
	ComPtr<ISelection> const _selection;
	ComPtr<IProject> const _project;

public:
	EditArea(IProject* project, IProjectWindow* pw, ISelection* selection, IUIFramework* rf, const RECT& rect, ID3D11DeviceContext1* deviceContext, IDWriteFactory* dWriteFactory, IWICImagingFactory2* wicFactory)
		: base(0, WS_CHILD | WS_VISIBLE, rect, pw->GetHWnd(), 55, deviceContext, dWriteFactory, wicFactory)
		, _project(project), _rf(rf), _selection(selection)
	{ }

	virtual ~EditArea()
	{
		assert (_refCount == 0);
	}

	virtual void Render(ID2D1DeviceContext* dc) const override final
	{
		dc->Clear (ColorF(ColorF::Indigo));

		D2D1_MATRIX_3X2_F oldtr;
		dc->GetTransform(&oldtr);
		dc->SetTransform (base::GetZoomTransform());

		for (auto& b : _project->GetBridges())
			b->Render(dc);

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
		else if (dynamic_cast<PhysicalBridge*>(_selection->GetObjects()[0]) != nullptr)
			viewId = cmdContextMenuBridge;
		else if (dynamic_cast<PhysicalPort*>(_selection->GetObjects()[0]) != nullptr)
			viewId = cmdContextMenuPort;
		else
			throw NotImplementedException();

		ComPtr<IUIContextualUI> ui;
		auto hr = _rf->GetView(viewId, IID_PPV_ARGS(&ui)); ThrowIfFailed(hr);
		hr = ui->ShowAtLocation(pt.x, pt.y); ThrowIfFailed(hr);
		return 0;
	}

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override final { throw NotImplementedException(); }

	virtual ULONG STDMETHODCALLTYPE AddRef() override final
	{
		return InterlockedIncrement(&_refCount);
	}

	virtual ULONG STDMETHODCALLTYPE Release() override final
	{
		auto newRefCount = InterlockedDecrement(&_refCount);
		if (newRefCount == 0)
			delete this;
		return newRefCount;
	}
};

extern const EditAreaFactory editAreaFactory = [](auto... params) { return ComPtr<IEditArea>(new EditArea(params...), false); };
