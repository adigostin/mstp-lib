#pragma once
#include "EventManager.h"
#include "Win32Defs.h"
#include "UtilityFunctions.h"
#include "mstp-lib/stp.h"

struct ISimulatorApp;
struct IProject;
struct IProjectWindow;
struct ISelection;
struct ILogArea;
struct IDockablePanel;
struct IDockContainer;
struct IActionList;
struct DrawingObjects;
class Object;
class Bridge;
class Port;
class Wire;

static constexpr std::array<uint8_t, 16> DefaultConfigTableDigest = { 0xAC, 0x36, 0x17, 0x7F, 0x50, 0x28, 0x3C, 0xD4, 0xB8, 0x38, 0x21, 0xD8, 0xAB, 0x26, 0xDE, 0x62 };

static constexpr unsigned int MaxVlanNumber = 16; // 4094 must be maximum

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

struct ISelection
{
	virtual ~ISelection() { }
	virtual const std::vector<Object*>& GetObjects() const = 0;
	virtual void Select (Object* o) = 0;
	virtual void Clear() = 0;
	virtual void Add (Object* o) = 0;
	virtual AddedToSelectionEvent::Subscriber GetAddedToSelectionEvent() = 0;
	virtual RemovingFromSelectionEvent::Subscriber GetRemovingFromSelectionEvent() = 0;
	virtual SelectionChangedEvent::Subscriber GetSelectionChangedEvent() = 0;
};

using SelectionFactory = ISelection*(*const)(const std::shared_ptr<IProject>& project);
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

	struct VisibleChangedEvent : public Event<VisibleChangedEvent, void(IDockablePanel* panel, bool visible)> {};
	struct SplitterDragging : public Event<SplitterDragging, void(IDockablePanel* panel, SIZE proposedSize)> {};

	virtual HWND GetHWnd() const = 0;
	virtual Side GetSide() const = 0;
	virtual POINT GetContentLocation() const = 0;
	virtual SIZE GetContentSize() const = 0;
	virtual VisibleChangedEvent::Subscriber GetVisibleChangedEvent() = 0;
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

using LogAreaFactory = std::unique_ptr<ILogArea>(*const)(HWND hWndParent, const RECT& rect, ID3D11DeviceContext1* deviceContext, IDWriteFactory* dWriteFactory);
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

using EditAreaFactory = IEditArea*(*const)(ISimulatorApp* app,
										   IProjectWindow* pw, 
										   const std::shared_ptr<IProject>& project,
										   const std::shared_ptr<ISelection>& selection,
										   const std::shared_ptr<IActionList>& actionList,
										   HWND hWndParent,
										   const RECT& rect,
										   ID3D11DeviceContext1* deviceContext,
										   IDWriteFactory* dWriteFactory);
extern const EditAreaFactory editAreaFactory;

// ============================================================================

struct SelectedVlanNumerChangedEvent : public Event<SelectedVlanNumerChangedEvent, void(IProjectWindow* pw, unsigned int vlanNumber)> { };

struct IProjectWindow : public IWin32Window
{
	virtual ~IProjectWindow() { }
	virtual IProject* GetProject() const = 0;
	virtual void SelectVlan (unsigned int vlanNumber) = 0;
	virtual unsigned int GetSelectedVlanNumber() const = 0;
	virtual SelectedVlanNumerChangedEvent::Subscriber GetSelectedVlanNumerChangedEvent() = 0;
};

using ProjectWindowFactory = IProjectWindow*(*const)(ISimulatorApp* app,
													 const std::shared_ptr<IProject>& project,
													 const std::shared_ptr<ISelection>& selection,
													 const std::shared_ptr<IActionList>& actionList,
													 EditAreaFactory editAreaFactory,
													 int nCmdShow,
													 unsigned int selectedVlan);
extern const ProjectWindowFactory projectWindowFactory;

// ============================================================================

struct BridgeInsertedEvent : public Event<BridgeInsertedEvent, void(IProject*, size_t index, Bridge*)> { };
struct BridgeRemovingEvent : public Event<BridgeRemovingEvent, void(IProject*, size_t index, Bridge*)> { };

struct WireInsertedEvent : public Event<WireInsertedEvent, void(IProject*, size_t index, Wire*)> { };
struct WireRemovingEvent : public Event<WireRemovingEvent, void(IProject*, size_t index, Wire*)> { };

struct ProjectInvalidateEvent : public Event<ProjectInvalidateEvent, void(IProject*)> { };

struct IProject abstract
{
	virtual ~IProject() { }

	virtual const std::vector<std::unique_ptr<Bridge>>& GetBridges() const = 0;
	virtual void InsertBridge (size_t index, std::unique_ptr<Bridge>&& bridge) = 0;
	virtual std::unique_ptr<Bridge> RemoveBridge (size_t index) = 0;
	virtual BridgeInsertedEvent::Subscriber GetBridgeInsertedEvent() = 0;
	virtual BridgeRemovingEvent::Subscriber GetBridgeRemovingEvent() = 0;

	virtual const std::vector<std::unique_ptr<Wire>>& GetWires() const = 0;
	virtual void InsertWire (size_t index, std::unique_ptr<Wire>&& wire) = 0;
	virtual std::unique_ptr<Wire> RemoveWire (size_t index) = 0;
	virtual WireInsertedEvent::Subscriber GetWireInsertedEvent() = 0;
	virtual WireRemovingEvent::Subscriber GetWireRemovingEvent() = 0;
	virtual ProjectInvalidateEvent::Subscriber GetProjectInvalidateEvent() = 0;
	virtual std::array<uint8_t, 6> AllocMacAddressRange (size_t count) = 0;

	virtual void Save (const wchar_t* path) const = 0;

	void Add (std::unique_ptr<Bridge>&& bridge) { InsertBridge (GetBridges().size(), std::move(bridge)); }
	void Add (std::unique_ptr<Wire>&& wire)     { InsertWire   (GetWires().size(),   std::move(wire));   }
	std::unique_ptr<Bridge> Remove (Bridge* b);
	std::unique_ptr<Wire> Remove (Wire* w);
	std::unique_ptr<Object> Remove (Object* o);

	std::pair<Wire*, size_t> GetWireConnectedToPort (const Port* port) const;
	Port* FindConnectedPort (Port* txPort) const;
};

using ProjectFactory = IProject*(*const)();
extern const ProjectFactory projectFactory;

// ============================================================================

struct IPropertiesWindow : public IWin32Window
{
	virtual ~IPropertiesWindow() { }
};

using PropertiesWindowFactory = IPropertiesWindow*(*const)(ISimulatorApp* app,
														   IProjectWindow* projectWindow,
														   const std::shared_ptr<IProject>& project,
														   const std::shared_ptr<ISelection>& selection,
														   const std::shared_ptr<IActionList>& actionList,
														   HWND hWndParent,
														   POINT location);
extern const PropertiesWindowFactory propertiesWindowFactory;

// ============================================================================

struct IBridgePropsWindow : public IWin32Window
{
	virtual ~IBridgePropsWindow() { }
};

using BridgePropsWindowFactory = IBridgePropsWindow*(*const)(ISimulatorApp* app,
															 IProjectWindow* projectWindow,
															 const std::shared_ptr<IProject>& project,
															 const std::shared_ptr<ISelection>& selection,
															 const std::shared_ptr<IActionList>& actionList,
															 HWND hwndParent,
															 POINT location);
extern const BridgePropsWindowFactory bridgePropertiesControlFactory;

// ============================================================================

struct IVlanWindow : public IWin32Window
{
	virtual ~IVlanWindow() { }
};

using VlanWindowFactory = IVlanWindow*(*const)(ISimulatorApp* app,
											   IProjectWindow* projectWindow,
											   const std::shared_ptr<IProject>& project,
											   const std::shared_ptr<ISelection>& selection,
											   const std::shared_ptr<IActionList>& actionList,
											   HWND hWndParent,
											   POINT location);
extern const VlanWindowFactory vlanWindowFactory;

// ============================================================================

struct ISimulatorApp
{
	virtual HINSTANCE GetHInstance() const = 0;
	virtual ID3D11DeviceContext1* GetD3DDeviceContext() const = 0;
	virtual IDWriteFactory* GetDWriteFactory() const = 0;
	virtual const wchar_t* GetRegKeyPath() const = 0;
	virtual void AddProjectWindow (std::unique_ptr<IProjectWindow>&& pw) = 0;
	virtual const std::vector<std::unique_ptr<IProjectWindow>>& GetProjectWindows() const = 0;
	virtual const wchar_t* GetAppName() const = 0;
};

// ============================================================================

struct IMSTConfigIdDialog
{
	virtual ~IMSTConfigIdDialog() { }
	virtual UINT ShowModal (HWND hWndParent) = 0; // return IDOK, IDCANCEL, -1 (some error), 0 (hWndParent invalid or closed)
};

using MSTConfigIdDialogFactory = IMSTConfigIdDialog* (*const)(ISimulatorApp* app, IProjectWindow* projectWindow, IProject* project, ISelection* selection);
extern const MSTConfigIdDialogFactory mstConfigIdDialogFactory;

// ============================================================================

enum class UndoOrRedo { Undo, Redo };

struct IActionList
{
	virtual ~IActionList() { }
	virtual void AddAndPerformEditAction (std::function<void(UndoOrRedo which)>&& action) = 0;
};
using ActionListFactory = IActionList*(*const)();
extern const ActionListFactory actionListFactory;
