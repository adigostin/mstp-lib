
#include "pch.h"
#include "Simulator.h"
#include "PropertyGrid.h"
#include "Bridge.h"
#include "Port.h"
#include "Wire.h"

using namespace std;

static ATOM wndClassAtom;
static constexpr wchar_t PropertiesWindowWndClassName[] = L"PropertiesWindow-{24B42526-2970-4B3C-A753-2DABD22C4BB0}";

class PropertiesWindow : public Window, public IPropertiesWindow
{
	using base = Window;

public:
	ISimulatorApp* const _app;
	IProjectWindow* const _projectWindow;
	IProjectPtr const _project;
	ISelectionPtr const _selection;
	PropertyGrid _pg;

	static constexpr DWORD Style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;

	PropertiesWindow (ISimulatorApp* app,
					  IProjectWindow* projectWindow,
					  IProject* project,
					  ISelection* selection,
					  const RECT& rect,
					  HWND hWndParent)
		: base (app->GetHInstance(), PropertiesWindowWndClassName, 0, Style, rect, hWndParent, nullptr)
		, _app(app)
		, _projectWindow(projectWindow)
		, _project(project)
		, _selection(selection)
		, _pg(app, projectWindow, project, this->Window::GetClientRectPixels(), GetHWnd(), app->GetDWriteFactory())
	{
		_selection->GetAddedToSelectionEvent().AddHandler (&OnObjectAddedToSelection, this);
		_selection->GetRemovingFromSelectionEvent().AddHandler (&OnObjectRemovingFromSelection, this);
		_selection->GetChangedEvent().AddHandler (&OnSelectionChanged, this);
		_projectWindow->GetSelectedVlanNumerChangedEvent().AddHandler (&OnSelectedVlanChanged, this);
	}

	~PropertiesWindow()
	{
		_projectWindow->GetSelectedVlanNumerChangedEvent().RemoveHandler (&OnSelectedVlanChanged, this);
		_selection->GetChangedEvent().RemoveHandler (&OnSelectionChanged, this);
		_selection->GetRemovingFromSelectionEvent().RemoveHandler (&OnObjectRemovingFromSelection, this);
		_selection->GetAddedToSelectionEvent().RemoveHandler (&OnObjectAddedToSelection, this);
	}

	static void OnObjectAddedToSelection (void* callbackArg, ISelection* selection, Object* o)
	{
		auto window = static_cast<PropertiesWindow*>(callbackArg);
		auto bridge = dynamic_cast<Bridge*>(o);
		if (bridge != nullptr)
			bridge->GetConfigChangedEvent().AddHandler (&OnBridgeConfigChanged, window);
	}

	static void OnObjectRemovingFromSelection (void* callbackArg, ISelection* selection, Object* o)
	{
		auto window = static_cast<PropertiesWindow*>(callbackArg);
		auto bridge = dynamic_cast<Bridge*>(o);
		if (bridge != nullptr)
			bridge->GetConfigChangedEvent().RemoveHandler (&OnBridgeConfigChanged, window);
	}

	static void OnSelectionChanged (void* callbackArg, ISelection* selection)
	{
		auto window = static_cast<PropertiesWindow*>(callbackArg);
		if (selection->GetObjects().empty())
			window->_pg.SelectObjects(nullptr, 0);
		else
			window->_pg.SelectObjects (&selection->GetObjects().at(0), selection->GetObjects().size());
	}

	static void OnBridgeConfigChanged (void* callbackArg, Bridge* b)
	{
		auto window = static_cast<PropertiesWindow*>(callbackArg);
		window->_pg.ReloadPropertyValues();
	}

	static void OnSelectedVlanChanged (void* callbackArg, IProjectWindow* pw, unsigned int vlanNumber)
	{
		auto window = static_cast<PropertiesWindow*>(callbackArg);
		window->_pg.ReloadPropertyValues();
	}

	virtual std::optional<LRESULT> WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override final
	{
		auto resultBaseClass = base::WindowProc (hwnd, msg, wParam, lParam);

		if (msg == WM_SIZE)
		{
			::MoveWindow (_pg.GetHWnd(), 0, 0, GetClientWidthPixels(), GetClientHeightPixels(), TRUE);
			::UpdateWindow (_pg.GetHWnd());
			return 0;
		}

		return resultBaseClass;
	}

	virtual HWND GetHWnd() const override final { return base::GetHWnd(); }
	virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID riid, void** ppvObject) override { return base::QueryInterface(riid, ppvObject); }
	virtual ULONG STDMETHODCALLTYPE AddRef() override { return base::AddRef(); }
	virtual ULONG STDMETHODCALLTYPE Release() override { return base::Release(); }
};

template <typename... Args>
static IPropertiesWindowPtr Create (Args... args)
{
	return IPropertiesWindowPtr(new PropertiesWindow (std::forward<Args>(args)...), false);
};

const PropertiesWindowFactory propertiesWindowFactory = &Create;
