#pragma once
#include "EventManager.h"
#include "Win32Defs.h"
#include "UtilityFunctions.h"
#include "stp.h"

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

struct ISelection
{
	virtual ~ISelection() { }
	virtual const std::vector<Object*>& GetObjects() const = 0;
	virtual void Select (Object* o) = 0;
	virtual void Clear() = 0;
	virtual void Add (Object* o) = 0;

	struct AddedToSelectionEvent : public Event<AddedToSelectionEvent, void(ISelection*, Object*)> { };
	virtual AddedToSelectionEvent::Subscriber GetAddedToSelectionEvent() = 0;

	struct RemovingFromSelectionEvent : public Event<RemovingFromSelectionEvent, void(ISelection*, Object*)> { };
	virtual RemovingFromSelectionEvent::Subscriber GetRemovingFromSelectionEvent() = 0;

	struct ChangedEvent : public Event<ChangedEvent, void(ISelection*)> { };
	virtual ChangedEvent::Subscriber GetChangedEvent() = 0;
};

using SelectionFactory = ISelection*(*const)(IProject* project);
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
	virtual Port* GetCPAt (D2D1_POINT_2F dLocation, float tolerance) const = 0;
	virtual void RenderHoverCP (ID2D1RenderTarget* rt, Port* port) const = 0;
	virtual D2D1::Matrix3x2F GetZoomTransform() const = 0;
};

using EditAreaFactory = std::unique_ptr<IEditArea>(*const)(ISimulatorApp* app,
														   IProjectWindow* pw,
														   HWND hWndParent,
														   const RECT& rect,
														   ID3D11DeviceContext1* deviceContext,
														   IDWriteFactory* dWriteFactory);
extern const EditAreaFactory editAreaFactory;

// ============================================================================

struct IProjectWindow : public IWin32Window
{
	virtual ~IProjectWindow() { }
	virtual IProject* GetProject() const = 0;
	virtual const std::shared_ptr<IProject>& GetProjectPtr() const = 0;
	virtual ISelection* GetSelection() const = 0;
	virtual IEditArea* GetEditArea() const = 0;
	virtual void SelectVlan (unsigned int vlanNumber) = 0;
	virtual unsigned int GetSelectedVlanNumber() const = 0;

	struct SelectedVlanNumerChangedEvent : public Event<SelectedVlanNumerChangedEvent, void(IProjectWindow* pw, unsigned int vlanNumber)> { };
	virtual SelectedVlanNumerChangedEvent::Subscriber GetSelectedVlanNumerChangedEvent() = 0;

	struct ClosedEvent : public Event<ClosedEvent, void(IProjectWindow* pw)> { };
	virtual ClosedEvent::Subscriber GetClosedEvent() = 0;
};

using ProjectWindowFactory = std::unique_ptr<IProjectWindow>(*const)(ISimulatorApp* app,
																	 const std::shared_ptr<IProject>& project,
																	 SelectionFactory selectionFactory,
																	 EditAreaFactory editAreaFactory,
																	 int nCmdShow,
																	 unsigned int selectedVlan);
extern const ProjectWindowFactory projectWindowFactory;

// ============================================================================

struct EditAction
{
	virtual void Undo() = 0;
	virtual void Redo() = 0;
};

struct IActionList
{
	virtual ~IActionList() { }

	struct ChangedEvent : public Event<ChangedEvent, void(IActionList*)> { };

	virtual ChangedEvent::Subscriber GetChangedEvent() = 0;
	virtual void AddPerformedUserAction (std::wstring&& actionName, std::unique_ptr<EditAction>&& action) = 0;
	virtual void PerformAndAddUserAction (std::wstring&& actionName, std::unique_ptr<EditAction>&& action) = 0;
	virtual size_t GetSavePointIndex() const = 0;
	virtual size_t GetEditPointIndex() const = 0;
};
using ActionListFactory = std::unique_ptr<IActionList>(*const)();
extern const ActionListFactory actionListFactory;

// ============================================================================

struct BridgeInsertedEvent : public Event<BridgeInsertedEvent, void(IProject*, size_t index, Bridge*)> { };
struct BridgeRemovingEvent : public Event<BridgeRemovingEvent, void(IProject*, size_t index, Bridge*)> { };

struct WireInsertedEvent : public Event<WireInsertedEvent, void(IProject*, size_t index, Wire*)> { };
struct WireRemovingEvent : public Event<WireRemovingEvent, void(IProject*, size_t index, Wire*)> { };

struct ProjectInvalidateEvent : public Event<ProjectInvalidateEvent, void(IProject*)> { };

enum class SaveProjectOption { SaveUnconditionally, SaveIfChangedAskUserFirst };

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

	virtual IActionList* GetActionList() const = 0;

	virtual ProjectInvalidateEvent::Subscriber GetProjectInvalidateEvent() = 0;
	virtual std::array<uint8_t, 6> AllocMacAddressRange (size_t count) = 0;

	virtual const std::wstring& GetFilePath() const = 0;
	virtual HRESULT Save (const wchar_t* filePath) = 0;

	size_t AddBridge (std::unique_ptr<Bridge>&& bridge);
	size_t AddWire (std::unique_ptr<Wire>&& wire);

	std::unique_ptr<Object> Remove (Object* o);

	std::pair<Wire*, size_t> GetWireConnectedToPort (const Port* port) const;
	Port* FindConnectedPort (Port* txPort) const;
};

using ProjectFactory = std::unique_ptr<IProject>(*const)(ISimulatorApp* app, ActionListFactory actionListFactory);
extern const ProjectFactory projectFactory;

// ============================================================================

struct IPropertiesWindow : public IWin32Window
{
	virtual ~IPropertiesWindow() { }
};

using PropertiesWindowFactory = std::unique_ptr<IPropertiesWindow>(*const)(ISimulatorApp* app,
																		   IProjectWindow* projectWindow,
																		   HWND hWndParent,
																		   POINT location);
extern const PropertiesWindowFactory propertiesWindowFactory;

// ============================================================================

struct IBridgePropsWindow : public IWin32Window
{
	virtual ~IBridgePropsWindow() { }
};

using BridgePropsWindowFactory = std::unique_ptr<IBridgePropsWindow>(*const)(ISimulatorApp* app,
																			 IProjectWindow* projectWindow,
																			 HWND hwndParent,
																			 POINT location);
extern const BridgePropsWindowFactory bridgePropertiesControlFactory;

// ============================================================================

struct IVlanWindow : public IWin32Window
{
	virtual ~IVlanWindow() { }
};

using VlanWindowFactory = std::unique_ptr<IVlanWindow>(*const)(ISimulatorApp* app,
															   IProjectWindow* projectWindow,
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

using MSTConfigIdDialogFactory = std::unique_ptr<IMSTConfigIdDialog>(*const)(ISimulatorApp* app, IProjectWindow* projectWindow);
extern const MSTConfigIdDialogFactory mstConfigIdDialogFactory;
