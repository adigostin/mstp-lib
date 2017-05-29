
#include "pch.h"
#include "Simulator.h"
#include "Win32/PropertyGrid.h"
#include "Bridge.h"
#include "Port.h"
#include "Wire.h"
#include "EditActions.h"

using namespace std;

static ATOM wndClassAtom;
static constexpr wchar_t PropertiesWindowWndClassName[] = L"PropertiesWindow-{24B42526-2970-4B3C-A753-2DABD22C4BB0}";

using StringPD = PropertyGrid::TypedPD<wstring>;
using uint = unsigned int;

extern const PropertyGrid::TypedPD<wstring> BridgePropAddress;
extern const PropertyGrid::TypedPD<bool> BridgePropStpEnabled;
extern const PropertyGrid::EnumPD::NVP StpVersionNVPs[];
extern const PropertyGrid::EnumPD BridgePropStpVersion;
extern const PropertyGrid::TypedPD<uint> BridgePropPortCount;
extern const PropertyGrid::TypedPD<uint> BridgePropMstiCount;
extern const vector<const PropertyGrid::PD*> BridgeProperties;

extern const PropertyGrid::TypedPD<bool> PortPropAdminEdge;
extern const PropertyGrid::TypedPD<bool> PortPropAutoEdge;
extern const vector<const PropertyGrid::PD*> PortProperties;

class PropertiesWindow : public Window, public IPropertiesWindow
{
	using base = Window;

public:
	ISimulatorApp* const _app;
	IProjectWindow* const _projectWindow;
	IProjectPtr const _project;
	ISelectionPtr const _selection;
	IActionListPtr const _actionList;
	PropertyGrid _pg;

	static constexpr DWORD Style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;

	PropertiesWindow (ISimulatorApp* app,
					  IProjectWindow* projectWindow,
					  IProject* project,
					  ISelection* selection,
					  IActionList* actionList,
					  const RECT& rect,
					  HWND hWndParent)
		: base (app->GetHInstance(), PropertiesWindowWndClassName, 0, Style, rect, hWndParent, nullptr)
		, _app(app)
		, _projectWindow(projectWindow)
		, _project(project)
		, _selection(selection)
		, _actionList(actionList)
		, _pg(app->GetHInstance(), this->Window::GetClientRectPixels(), GetHWnd(), app->GetDWriteFactory(), this, &GetPropertyDescriptors)
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

	static const std::vector<const PropertyGrid::PD*>& GetPropertyDescriptors (const void* selectedObject)
	{
		auto o = static_cast<const Object*>(selectedObject);
		if (dynamic_cast<const Bridge*>(o) != nullptr)
			return BridgeProperties;

		if (dynamic_cast<const Port*>(o) != nullptr)
			return PortProperties;

		throw not_implemented_exception();
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
			window->_pg.SelectObjects ((void**) &selection->GetObjects().at(0), selection->GetObjects().size());
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

// ============================================================================

static const PropertyGrid::TypedPD<wstring> BridgePropAddress
(
	L"Bridge Address",
	[](const void* o) { return static_cast<const Bridge*>(o)->GetBridgeAddressAsWString(); },
	[](void* o, wstring str, const PropertyGrid* pg)
	{
		auto newAddress = ConvertStringToBridgeAddress(str.c_str());
		auto window = static_cast<PropertiesWindow*>(pg->GetAppContext());
		vector<Bridge*> bridges;
		std::transform (window->_selection->GetObjects().begin(), window->_selection->GetObjects().end(), back_inserter(bridges), [](Object* o) { return dynamic_cast<Bridge*>(o); });
		auto action = unique_ptr<EditAction>(new SetBridgeAddressAction(bridges, newAddress));
		window->_actionList->PerformAndAddUserAction (move(action));
	}
);

static const PropertyGrid::TypedPD<bool> BridgePropStpEnabled
(
	L"STP Enabled",
	[](const void* o) { return (bool) STP_IsBridgeStarted(static_cast<const Bridge*>(o)->GetStpBridge()); },
	nullptr
);

static const PropertyGrid::EnumPD::NVP StpVersionNVPs[] = { { L"LegacySTP", STP_VERSION_LEGACY_STP }, { L"RSTP", STP_VERSION_RSTP }, { L"MSTP", STP_VERSION_MSTP }, { 0, 0 } };

static const PropertyGrid::EnumPD BridgePropStpVersion
{
	L"STP Version",
	[](const void* o) { return (int) STP_GetStpVersion(static_cast<const Bridge*>(o)->GetStpBridge()); },
	nullptr,
	StpVersionNVPs
};

static const PropertyGrid::TypedPD<uint> BridgePropPortCount
{
	L"Port Count",
	[](const void* o) { return static_cast<const Bridge*>(o)->GetPorts().size(); },
	nullptr
};

static const PropertyGrid::TypedPD<uint> BridgePropMstiCount
{
	L"MSTI Count",
	[](const void* o) { return STP_GetMstiCount(static_cast<const Bridge*>(o)->GetStpBridge()); },
	nullptr
};

static const vector<const PropertyGrid::PD*> BridgeProperties =
{
	&BridgePropAddress,
	&BridgePropStpEnabled,
	&BridgePropStpVersion,
	&BridgePropPortCount,
	&BridgePropMstiCount,
};

static const PropertyGrid::TypedPD<bool> PortPropAdminEdge
{
	L"AdminEdge",
	[](const void* o)
	{
		auto port = static_cast<const Port*>(o);
		return (bool) STP_GetPortAdminEdge(port->GetBridge()->GetStpBridge(), (unsigned int) port->GetPortIndex());
	},
	nullptr,
};

static const PropertyGrid::TypedPD<bool> PortPropAutoEdge
{
	L"AutoEdge",
	[](const void* o)
	{
		auto port = static_cast<const Port*>(o);
		return (bool) STP_GetPortAutoEdge(port->GetBridge()->GetStpBridge(), (unsigned int) port->GetPortIndex());
	},
	nullptr,
};

static const vector<const PropertyGrid::PD*> PortProperties =
{
	&PortPropAutoEdge,
	&PortPropAdminEdge,
};
