#pragma once
#include "EventManager.h"
#include "Win32Defs.h"
#include "UtilityFunctions.h"

struct ISimulatorApp;
struct IProject;
struct IProjectWindow;
struct ISelection;
struct ILogArea;
struct IDockablePanel;
struct IDockContainer;
struct DrawingObjects;
class Object;
class Bridge;
class Port;
class Wire;

enum class MouseButton
{
	None = 0,
	Left = 1,
	Right = 2,
	Middle = 4,
};

struct IWin32Window
{
	virtual HWND GetHWnd() const = 0;
	virtual RECT GetClientRectPixels() const;

	RECT GetWindowRect() const;
	SIZE GetWindowSize() const;
	SIZE GetClientSize() const;
};

// ============================================================================

struct AddedToSelectionEvent : public Event<AddedToSelectionEvent, void(ISelection*, Object*)> { };
struct RemovingFromSelectionEvent : public Event<RemovingFromSelectionEvent, void(ISelection*, Object*)> { };
struct SelectionChangedEvent : public Event<SelectionChangedEvent, void(ISelection*)> { };

struct ISelection abstract : public IUnknown
{
	virtual const std::vector<ComPtr<Object>>& GetObjects() const = 0;
	virtual void Select (Object* o) = 0;
	virtual void Clear() = 0;
	virtual void Add (Object* o) = 0;
	virtual AddedToSelectionEvent::Subscriber GetAddedToSelectionEvent() = 0;
	virtual RemovingFromSelectionEvent::Subscriber GetRemovingFromSelectionEvent() = 0;
	virtual SelectionChangedEvent::Subscriber GetSelectionChangedEvent() = 0;
};

using SelectionFactory = ComPtr<ISelection>(*const)(IProject* project);
extern const SelectionFactory selectionFactory;

// ============================================================================

struct IDockContainer abstract
{
	virtual ~IDockContainer() { }
	virtual HWND GetHWnd() const = 0;
	virtual RECT GetContentRect() const = 0;
	virtual IDockablePanel* GetOrCreateDockablePanel(Side side, const wchar_t* title) = 0;
	virtual void ResizePanel (IDockablePanel* panel, SIZE size) = 0;
};

using DockContainerFactory = std::unique_ptr<IDockContainer>(*const)(HWND hWndParent, DWORD controlId, const RECT& rect);
extern const DockContainerFactory dockPanelFactory;

// ============================================================================

struct IDockablePanel abstract
{
	virtual ~IDockablePanel() { }

	struct CloseButtonClicked : public Event<CloseButtonClicked, void(IDockablePanel* panel)> {};
	struct SplitterDragging : public Event<SplitterDragging, void(IDockablePanel* panel, SIZE proposedSize)> {};
	struct SplitterDragComplete : public Event<SplitterDragComplete, void(IDockablePanel* panel)> {};

	virtual HWND GetHWnd() const = 0;
	virtual Side GetSide() const = 0;
	virtual POINT GetContentLocation() const = 0;
	virtual SIZE GetContentSize() const = 0;
	virtual CloseButtonClicked::Subscriber GetCloseButtonClickedEvent() = 0;
	virtual SplitterDragging::Subscriber GetSplitterDraggingEvent() = 0;
	virtual SIZE GetPanelSizeFromContentSize (SIZE contentSize) const = 0;

	RECT GetContentRect() const
	{
		auto l = GetContentLocation();
		auto s = GetContentSize();
		return RECT { l.x, l.y, l.x + s.cx, l.y + s.cy };
	}

	SIZE GetWindowSize() const
	{
		RECT wr;
		BOOL bRes = ::GetWindowRect(this->GetHWnd(), &wr);
		if (!bRes)
			throw win32_exception(GetLastError());
		return { wr.right - wr.left, wr.bottom - wr.top };
	}
};

using DockablePanelFactory = std::unique_ptr<IDockablePanel>(*const)(HWND hWndParent, const RECT& rect, Side side, const wchar_t* title);
extern const DockablePanelFactory dockablePanelFactory;

// ============================================================================

struct ILogArea abstract : public IWin32Window
{
	virtual ~ILogArea() { }
	virtual void SelectBridge (Bridge* b) = 0;
};

using LogAreaFactory = std::unique_ptr<ILogArea>(*const)(HWND hWndParent, DWORD controlId, const RECT& rect, ID3D11DeviceContext1* deviceContext, IDWriteFactory* dWriteFactory);
extern const LogAreaFactory logAreaFactory;

// ============================================================================

class EditState;
struct EditStateDeps;

static constexpr float SnapDistance = 6;

struct DialogProcResult
{
	INT_PTR dialogProcResult;
	LRESULT messageResult;
};

struct MouseLocation
{
	POINT pt;
	D2D1_POINT_2F d;
	D2D1_POINT_2F w;
};

struct IEditArea abstract
{
	virtual ~IEditArea() { }
	virtual HWND GetHWnd() const = 0;
	virtual const DrawingObjects& GetDrawingObjects() const = 0;
	virtual void EnterState (std::unique_ptr<EditState>&& state) = 0;
	virtual EditStateDeps MakeEditStateDeps() = 0;
	virtual Port* GetCPAt (D2D1_POINT_2F dLocation, float tolerance) const = 0;
	virtual void RenderHoverCP (ID2D1RenderTarget* rt, Port* port) const = 0;
	virtual D2D1::Matrix3x2F GetZoomTransform() const = 0;
};

using EditAreaFactory = std::unique_ptr<IEditArea>(*const)(IProject* project, IProjectWindow* pw, ISelection* selection, HWND hWndParent, const RECT& rect, ID3D11DeviceContext1* deviceContext, IDWriteFactory* dWriteFactory);
extern const EditAreaFactory editAreaFactory;

// ============================================================================

struct SelectedVlanNumerChangedEvent : public Event<SelectedVlanNumerChangedEvent, void(IProjectWindow* pw, uint16_t vlanNumber)> { };

struct IProjectWindow : public IWin32Window
{
	virtual ~IProjectWindow() { }
	virtual IProject* GetProject() const = 0;
	virtual void SelectVlan (uint16_t vlanNumber) = 0;
	virtual uint16_t GetSelectedVlanNumber() const = 0;
	virtual SelectedVlanNumerChangedEvent::Subscriber GetSelectedVlanNumerChangedEvent() = 0;
};

using ProjectWindowFactory = std::unique_ptr<IProjectWindow>(*const)(ISimulatorApp* app,
																	 IProject* project,
																	 ISelection* selection,
																	 EditAreaFactory editAreaFactory,
																	 int nCmdShow);
extern const ProjectWindowFactory projectWindowFactory;

// ============================================================================

struct BridgeInsertedEvent : public Event<BridgeInsertedEvent, void(IProject*, size_t index, Bridge*)> { };
struct BridgeRemovingEvent : public Event<BridgeRemovingEvent, void(IProject*, size_t index, Bridge*)> { };

struct WireInsertedEvent : public Event<WireInsertedEvent, void(IProject*, size_t index, Wire*)> { };
struct WireRemovingEvent : public Event<WireRemovingEvent, void(IProject*, size_t index, Wire*)> { };

struct ProjectInvalidateEvent : public Event<ProjectInvalidateEvent, void(IProject*)> { };

struct IProject abstract : public IUnknown
{
	virtual const std::vector<ComPtr<Bridge>>& GetBridges() const = 0;
	virtual void InsertBridge (size_t index, Bridge* bridge) = 0;
	virtual void RemoveBridge (size_t index) = 0;
	virtual BridgeInsertedEvent::Subscriber GetBridgeInsertedEvent() = 0;
	virtual BridgeRemovingEvent::Subscriber GetBridgeRemovingEvent() = 0;
	void Add (Bridge* bridge) { InsertBridge (GetBridges().size(), bridge); }

	virtual const std::vector<ComPtr<Wire>>& GetWires() const = 0;
	virtual void InsertWire (size_t index, Wire* wire) = 0;
	virtual void RemoveWire (size_t index) = 0;
	virtual WireInsertedEvent::Subscriber GetWireInsertedEvent() = 0;
	virtual WireRemovingEvent::Subscriber GetWireRemovingEvent() = 0;
	void Add (Wire* wire) { InsertWire (GetWires().size(), wire); }

	void Remove (Bridge* b);
	void Remove (Wire* w);
	void Remove (Object* o);

	virtual ProjectInvalidateEvent::Subscriber GetProjectInvalidateEvent() = 0;

	virtual std::array<uint8_t, 6> AllocMacAddressRange (size_t count) = 0;
	virtual std::pair<Wire*, size_t> GetWireConnectedToPort (const Port* port) const = 0;
	virtual Port* FindReceivingPort (Port* txPort) const = 0;
};

using ProjectFactory = ComPtr<IProject>(*const)();
extern const ProjectFactory projectFactory;

// ============================================================================

struct IPropertiesWindow : public IWin32Window
{
	virtual ~IPropertiesWindow() { }
};

using PropertiesWindowFactory = std::unique_ptr<IPropertiesWindow>(*const)(HWND hWndParent, const RECT& rect, ISelection* selection);
extern const PropertiesWindowFactory propertiesWindowFactory;

// ============================================================================

struct IBridgePropsWindow : public IWin32Window
{
	virtual ~IBridgePropsWindow() { }
};

using BridgePropsWindowFactory = std::unique_ptr<IBridgePropsWindow>(*const)(HWND hwndParent, const RECT& rect, ISelection* selection);
extern const BridgePropsWindowFactory bridgePropertiesControlFactory;

// ============================================================================

struct IVlanWindow : public IWin32Window
{
	virtual ~IVlanWindow() { }
};

using VlanWindowFactory = std::unique_ptr<IVlanWindow>(*const)(HWND hWndParent, POINT location, ISimulatorApp* app, IProject* project, IProjectWindow* projectWindow, ISelection* selection);
extern const VlanWindowFactory vlanWindowFactory;

// ============================================================================

struct ISimulatorApp
{
	virtual ID3D11DeviceContext1* GetD3DDeviceContext() const = 0;
	virtual IDWriteFactory* GetDWriteFactory() const = 0;
	virtual const wchar_t* GetRegKeyPath() const = 0;
	virtual void AddProjectWindow (std::unique_ptr<IProjectWindow>&& pw) = 0;
	virtual const std::vector<std::unique_ptr<IProjectWindow>>& GetProjectWindows() const = 0;
};
