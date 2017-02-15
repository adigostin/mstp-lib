
#include "pch.h"
#include "SimulatorDefs.h"
#include "ZoomableWindow.h"
#include "Ribbon/RibbonIds.h"

using namespace std;
using namespace D2D1;

class EditArea : public ZoomableWindow, public IEditArea
{
	typedef ZoomableWindow base;

	ULONG _refCount = 1;
	IProjectWindow* const _pw;
	IUIFramework* const _rf;
	ComPtr<ISelection> const _selection;
	ComPtr<IProject> const _project;
	ComPtr<IDWriteFactory> const _dWriteFactory;
	unsigned int _selectedTreeIndex = 0;

public:
	EditArea(IProject* project, IProjectWindow* pw, ISelection* selection, IUIFramework* rf, const RECT& rect, ID3D11DeviceContext1* deviceContext, IDWriteFactory* dWriteFactory, IWICImagingFactory2* wicFactory)
		: base(0, WS_CHILD | WS_VISIBLE, rect, pw->GetHWnd(), 55, deviceContext, dWriteFactory, wicFactory)
		, _project(project), _pw(pw), _rf(rf), _selection(selection), _dWriteFactory(dWriteFactory)
	{
		_selection->GetSelectionChangedEvent().AddHandler (&OnSelectionChanged, this);
	}

	virtual ~EditArea()
	{
		assert (_refCount == 0);
		_selection->GetSelectionChangedEvent().RemoveHandler (&OnSelectionChanged, this);
	}

	static void OnSelectionChanged (void* callbackArg, ISelection* selection)
	{
		auto editArea = static_cast<EditArea*>(callbackArg);
		::InvalidateRect (editArea->GetHWnd(), nullptr, FALSE);
	}

	virtual void Render(ID2D1DeviceContext* dc) const override final
	{
		auto backGdiColor = GetSysColor(COLOR_WINDOW);
		auto backColor = D2D1_COLOR_F{ (backGdiColor & 0xff) / 255.0f, ((backGdiColor >> 8) & 0xff) / 255.0f, ((backGdiColor >> 16) & 0xff) / 255.0f, 1.0f };

		auto textGdiColor = GetSysColor(COLOR_WINDOWTEXT);
		auto textColor = D2D1_COLOR_F{ (textGdiColor & 0xff) / 255.0f, ((textGdiColor >> 8) & 0xff) / 255.0f, ((textGdiColor >> 16) & 0xff) / 255.0f, 1.0f };

		dc->Clear(backColor);

		auto clientRectDips = GetClientRectDips();

		HRESULT hr;

		if (_project->GetBridges().empty())
		{
			auto text = L"No bridges created. Right-click to create some.";
			ComPtr<IDWriteTextFormat> tf;
			hr = _dWriteFactory->CreateTextFormat (L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 16, L"en-US", &tf); ThrowIfFailed(hr);
			ComPtr<IDWriteTextLayout> tl;
			hr = _dWriteFactory->CreateTextLayout (text, wcslen(text), tf, 10000, 10000, &tl); ThrowIfFailed(hr);
			DWRITE_TEXT_METRICS metrics;
			hr = tl->GetMetrics(&metrics); ThrowIfFailed(hr);
			D2D1_POINT_2F origin = { clientRectDips.right / 2 - metrics.width / 2, clientRectDips.bottom / 2 - metrics.height / 2 };
			ComPtr<ID2D1SolidColorBrush> brush;
			dc->CreateSolidColorBrush (textColor, &brush);
			dc->DrawTextLayout (origin, tl, brush);
		}
		else
		{
			D2D1_MATRIX_3X2_F oldtr;
			dc->GetTransform(&oldtr);
			dc->SetTransform(GetZoomTransform());

			for (auto& b : _project->GetBridges())
				b->Render(dc, _selectedTreeIndex, _dWriteFactory);

			dc->SetTransform(oldtr);

			auto oldaa = dc->GetAntialiasMode();
			dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);

			for (auto o : _selection->GetObjects())
			{
				if (auto b = dynamic_cast<PhysicalBridge*>(o))
				{
					auto tl = GetDLocationFromWLocation ({ b->GetLeft() - BridgeOutlineWidth / 2, b->GetTop() - BridgeOutlineWidth / 2 });
					auto br = GetDLocationFromWLocation ({ b->GetRight() + BridgeOutlineWidth / 2, b->GetBottom() + BridgeOutlineWidth / 2 });

					ComPtr<ID2D1Factory> factory;
					dc->GetFactory(&factory);
					D2D1_STROKE_STYLE_PROPERTIES ssprops = {};
					ssprops.dashStyle = D2D1_DASH_STYLE_DASH;
					ComPtr<ID2D1StrokeStyle> ss;
					hr = factory->CreateStrokeStyle (&ssprops, nullptr, 0, &ss); ThrowIfFailed(hr);
					ComPtr<ID2D1SolidColorBrush> brush;
					hr = dc->CreateSolidColorBrush (ColorF(ColorF::Blue), &brush);
					dc->DrawRectangle ({ tl.x - 10, tl.y - 10, br.x + 10, br.y + 10 }, brush, 1, ss);
				}
			}

			dc->SetAntialiasMode(oldaa);
		}

	}

	virtual HWND GetHWnd() const override final { return base::GetHWnd(); }

	virtual std::optional<LRESULT> WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override
	{
		if ((uMsg == WM_LBUTTONDOWN) || (uMsg == WM_RBUTTONDOWN))
		{
			auto button = (uMsg == WM_LBUTTONDOWN) ? MouseButton::Left : MouseButton::Right;
			return ProcessMouseButtonDown (button, POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });
		}

		if (uMsg == WM_CONTEXTMENU)
			return ProcessWmContextMenu (hwnd, POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });

		return base::WindowProc (hwnd, uMsg, wParam, lParam);
	}

	Object* GetObjectAt (float x, float y) const
	{
		for (auto& b : _project->GetBridges())
		{
			if ((x >= b->GetLeft()) && (x < b->GetRight()) && (y >= b->GetTop()) && (y < b->GetBottom()))
			{
				return b;
			}
		}

		return nullptr;
	};

	std::optional<LRESULT> ProcessMouseButtonDown (MouseButton button, POINT pt)
	{
		auto dLocation = GetDipLocationFromPixelLocation(pt);
		auto wLocation = GetWLocationFromDLocation(dLocation);
		auto clickedObject = GetObjectAt(wLocation.x, wLocation.y);
		if (clickedObject == nullptr)
			_selection->Clear();
		else
			_selection->Select(clickedObject);
		return 0;
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

	virtual void SelectTreeIndex(unsigned int treeIndex) override final
	{
		if (_selectedTreeIndex != treeIndex)
		{
			_selectedTreeIndex = treeIndex;
			::InvalidateRect (GetHWnd(), nullptr, FALSE);
		}
	};

	#pragma region IUnknown
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
	#pragma endregion
};

extern const EditAreaFactory editAreaFactory = [](auto... params) { return ComPtr<IEditArea>(new EditArea(params...), false); };
