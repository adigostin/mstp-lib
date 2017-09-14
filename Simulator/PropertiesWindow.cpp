
#include "pch.h"
#include "Simulator.h"
#include "PropertyGrid.h"
#include "Bridge.h"

using namespace std;
using namespace D2D1;

class PropertiesWindow : public D2DWindow, public IPropertiesWindow
{
	using base = D2DWindow;

	ISelectionPtr const _selection;
	IProjectWindow* const _projectWindow;
	IProjectPtr const _project;
	unique_ptr<PropertyGrid> _pg1;
	unique_ptr<PropertyGrid> _pg2;
	IDWriteTextFormatPtr _titleTextFormat;
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
		, _projectWindow(projectWindow)
		, _project(project)
		, _selection(selection)
	{
		auto hr = app->GetDWriteFactory()->CreateTextFormat (L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12, L"en-US", &_titleTextFormat); assert(SUCCEEDED(hr));

		CreateTitleTextLayouts();

		_pg1.reset (new PropertyGrid(app->GetHInstance(), GetPG1Rect(), GetHWnd(), app->GetDWriteFactory(), projectWindow));
		_pg2.reset (new PropertyGrid(app->GetHInstance(), GetPG2Rect(), GetHWnd(), app->GetDWriteFactory(), projectWindow));

		_pg1->GetPropertyChangedByUserEvent().AddHandler (&OnPropertyChangedInPG, this);
		_pg2->GetPropertyChangedByUserEvent().AddHandler (&OnPropertyChangedInPG, this);
		_projectWindow->GetSelectedVlanNumerChangedEvent().AddHandler (&OnSelectedVlanChanged, this);
		_selection->GetChangedEvent().AddHandler (&OnSelectionChanged, this);
	}

	virtual ~PropertiesWindow()
	{
		_selection->GetChangedEvent().RemoveHandler (&OnSelectionChanged, this);
		_projectWindow->GetSelectedVlanNumerChangedEvent().RemoveHandler (&OnSelectedVlanChanged, this);
		_pg2->GetPropertyChangedByUserEvent().RemoveHandler (&OnPropertyChangedInPG, this);
		_pg1->GetPropertyChangedByUserEvent().RemoveHandler (&OnPropertyChangedInPG, this);
	}

	template<typename... Args>
	static IPropertiesWindowPtr Create (Args... args)
	{
		return IPropertiesWindowPtr (new PropertiesWindow(std::forward<Args>(args)...), false);
	}

	void CreateTitleTextLayouts()
	{
		_title1TextLayout = TextLayout::Create (GetDWriteFactory(), _titleTextFormat, L"Properties");
		wstringstream ss;
		ss << L"VLAN " << _projectWindow->GetSelectedVlanNumber() << L" Specific Properties";
		_title2TextLayout = TextLayout::Create (GetDWriteFactory(), _titleTextFormat, ss.str().c_str());
	}

	void SetSelectionToPGs()
	{
		if (_selection->GetObjects().empty())
		{
			_pg1->SelectObjects(nullptr, 0);
			_pg2->SelectObjects(nullptr, 0);
		}
		else
		{
			_pg1->SelectObjects (_selection->GetObjects().data(), _selection->GetObjects().size());

			if (all_of (_selection->GetObjects().begin(), _selection->GetObjects().end(), [](Object* o) { return o->Is<Bridge>(); }))
			{
				std::vector<BridgeTree*> bridgeTrees;
				for (Object* o : _selection->GetObjects())
				{
					auto b = static_cast<Bridge*>(o);
					auto treeIndex = STP_GetTreeIndexFromVlanNumber(b->GetStpBridge(), _projectWindow->GetSelectedVlanNumber());
					bridgeTrees.push_back (b->GetTrees().at(treeIndex).get());
				}

				_pg2->SelectObjects ((Object**) &bridgeTrees[0], bridgeTrees.size());
			}
			else
				_pg2->SelectObjects(nullptr, 0);
		}
	}

	static void OnSelectionChanged (void* callbackArg, ISelection* selection)
	{
		auto window = static_cast<PropertiesWindow*>(callbackArg);
		window->SetSelectionToPGs();
	}

	static void OnSelectedVlanChanged (void* callbackArg, IProjectWindow* pw, unsigned int selectedVlan)
	{
		auto window = static_cast<PropertiesWindow*>(callbackArg);
		window->SetSelectionToPGs();
		window->CreateTitleTextLayouts();
		::InvalidateRect(window->GetHWnd(), nullptr, FALSE);
	}

	static void OnPropertyChangedInPG (void* callbackArg, const Property* property)
	{
		auto pw = static_cast<PropertiesWindow*>(callbackArg);
		pw->_project->SetModified(true);
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

	static constexpr D2D1_RECT_F TitleBarPadding = { 3, 1, 3, 1 };

	RECT GetPG1Rect() const
	{
		float heightDips = TitleBarPadding.top + _title1TextLayout.metrics.height + TitleBarPadding.bottom;
		LONG heightPixels = GetPixelSizeFromDipSize({ 0, heightDips }).cy;
		auto cs = GetClientSizePixels();
		return { 0, heightPixels, cs.cx, cs.cy / 2 };
	}

	RECT GetPG2Rect() const
	{
		float heightDips = TitleBarPadding.top + _title1TextLayout.metrics.height + TitleBarPadding.bottom;
		LONG heightPixels = GetPixelSizeFromDipSize({ 0, heightDips }).cy;
		auto cs = GetClientSizePixels();
		return { 0, cs.cy / 2 + heightPixels, cs.cx, cs.cy };
	}

	virtual void Render (ID2D1RenderTarget* rt) const override final
	{
		rt->Clear(GetD2DSystemColor(COLOR_ACTIVECAPTION));

		ID2D1SolidColorBrushPtr brush;
		rt->CreateSolidColorBrush (GetD2DSystemColor(COLOR_CAPTIONTEXT), &brush);
		rt->DrawTextLayout (Point2F(TitleBarPadding.left, TitleBarPadding.top), _title1TextLayout.layout, brush);
		rt->DrawTextLayout (Point2F(TitleBarPadding.left, GetClientHeightDips() / 2 + TitleBarPadding.top), _title2TextLayout.layout, brush);
	}

	virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID riid, void** ppvObject) override { return base::QueryInterface(riid, ppvObject); }
	virtual ULONG STDMETHODCALLTYPE AddRef() override final { return base::AddRef(); }
	virtual ULONG STDMETHODCALLTYPE Release() override final { return base::Release(); }
	virtual HWND GetHWnd() const override final { return base::GetHWnd(); }
};

const PropertiesWindowFactory propertiesWindowFactory = &PropertiesWindow::Create;
