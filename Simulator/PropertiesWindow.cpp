
#include "pch.h"
#include "Simulator.h"
#include "Reflection/PropertyGrid.h"

using namespace std;
using namespace D2D1;

class PropertiesWindow : public D2DWindow, public IPropertiesWindow
{
	using base = D2DWindow;

	unique_ptr<PropertyGrid> _pg1;
	unique_ptr<PropertyGrid> _pg2;
	TextLayout _title1TextLayout;
	TextLayout _title2TextLayout;

public:
	PropertiesWindow (ISimulatorApp* app,
					  IProjectWindow* projectWindow,
					  IProject* project,
					  ISelection* selection,
					  const RECT& rect,
					  HWND hWndParent)
		: base (app->GetHInstance(), WS_EX_CLIENTEDGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, rect, hWndParent, nullptr, app->GetDWriteFactory())
	{
		IDWriteTextFormatPtr format;
		auto hr = app->GetDWriteFactory()->CreateTextFormat (L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12, L"en-US", &format); ThrowIfFailed(hr);

		_title1TextLayout = TextLayout::Create (app->GetDWriteFactory(), format, L"Properties");
		_title2TextLayout = TextLayout::Create (app->GetDWriteFactory(), format, L"VLAN-Specific Properties");

		_pg1.reset (new PropertyGrid(app->GetHInstance(), GetPG1Rect(), GetHWnd(), app->GetDWriteFactory(), projectWindow));
		_pg2.reset (new PropertyGrid(app->GetHInstance(), GetPG2Rect(), GetHWnd(), app->GetDWriteFactory(), projectWindow));
	}

	template<typename... Args>
	static IPropertiesWindowPtr Create (Args... args)
	{
		return IPropertiesWindowPtr (new PropertiesWindow(std::forward<Args>(args)...), false);
	}

	virtual optional<LRESULT> WindowProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override
	{
		auto resultBaseClass = base::WindowProc (hwnd, msg, wParam, lParam);

		if (msg == WM_SIZE)
		{
			if (_pg1 != nullptr)
				_pg1->SetRect(GetPG1Rect());

			if (_pg2 != nullptr)
				_pg2->SetRect(GetPG2Rect());

			return 0;
		}

		return resultBaseClass;
	}

	static constexpr float TitleBarPadding = 3;

	RECT GetPG1Rect() const
	{
		float heightDips = TitleBarPadding + _title1TextLayout.metrics.height + TitleBarPadding;
		LONG heightPixels = GetPixelSizeFromDipSize({ 0, heightDips }).cy;
		auto cs = GetClientSizePixels();
		return { 0, heightPixels, cs.cx, cs.cy / 2 };
	}

	RECT GetPG2Rect() const
	{
		float heightDips = TitleBarPadding + _title1TextLayout.metrics.height + TitleBarPadding;
		LONG heightPixels = GetPixelSizeFromDipSize({ 0, heightDips }).cy;
		auto cs = GetClientSizePixels();
		return { 0, cs.cy / 2 + heightPixels, cs.cx, cs.cy };
	}

	virtual void Render (ID2D1RenderTarget* rt) const override final
	{
		rt->Clear(GetD2DSystemColor(COLOR_ACTIVECAPTION));

		ID2D1SolidColorBrushPtr brush;
		rt->CreateSolidColorBrush (GetD2DSystemColor(COLOR_CAPTIONTEXT), &brush);
		rt->DrawTextLayout (Point2F(TitleBarPadding, TitleBarPadding), _title1TextLayout.layout, brush);
		rt->DrawTextLayout (Point2F(TitleBarPadding, GetClientHeightDips() / 2 + TitleBarPadding), _title2TextLayout.layout, brush);
	}

	virtual PropertyGrid* GetPG1() const override final { return _pg1.get(); }
	virtual PropertyGrid* GetPG2() const override final { return _pg2.get(); }

	virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID riid, void** ppvObject) override { return base::QueryInterface(riid, ppvObject); }
	virtual ULONG STDMETHODCALLTYPE AddRef() override final { return base::AddRef(); }
	virtual ULONG STDMETHODCALLTYPE Release() override final { return base::Release(); }
	virtual HWND GetHWnd() const override final { return base::GetHWnd(); }
};

const PropertiesWindowFactory propertiesWindowFactory = &PropertiesWindow::Create;
