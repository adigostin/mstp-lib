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

struct __declspec(novtable) IWin32Window
{
	virtual HWND GetHWnd() const = 0;
	virtual RECT GetClientRectPixels() const;

	RECT GetWindowRect() const;
	SIZE GetWindowSize() const;
	SIZE GetClientSize() const;
};

// ============================================================================

MIDL_INTERFACE("3ADCEF4B-9335-4DD7-8016-5958883A4347") ISelection : public IUnknown
{
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
_COM_SMARTPTR_TYPEDEF(ISelection, __uuidof(ISelection));
using SelectionFactory = ISelectionPtr(*const)(IProject* project);
extern const SelectionFactory selectionFactory;

// ============================================================================

MIDL_INTERFACE("E97899CA-925F-43A7-A0C2-F8743A914BAB") IDockContainer : public IUnknown
{
	virtual HWND GetHWnd() const = 0;
	virtual RECT GetContentRect() const = 0;
	virtual IDockablePanel* CreatePanel (const char* panelUniqueName, Side side, const wchar_t* title) = 0;
	virtual IDockablePanel* GetPanel (const char* panelUniqueName) const = 0;
	virtual void ResizePanel (IDockablePanel* panel, SIZE size) = 0;
};
_COM_SMARTPTR_TYPEDEF(IDockContainer, __uuidof(IDockContainer));
using DockContainerFactory = IDockContainerPtr(*const)(HWND hWndParent, const RECT& rect);
extern const DockContainerFactory dockContainerFactory;

// ============================================================================

MIDL_INTERFACE("EE540D38-79DC-479B-9619-D253EB9BA812") IDockablePanel : public IUnknown
{
	struct VisibleChangedEvent : public Event<VisibleChangedEvent, void(IDockablePanel* panel, bool visible)> {};
	struct SplitterDragging : public Event<SplitterDragging, void(IDockablePanel* panel, SIZE proposedSize)> {};

	virtual const std::string& GetUniqueName() const = 0;
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
_COM_SMARTPTR_TYPEDEF(IDockablePanel, __uuidof(IDockablePanel));
using DockablePanelFactory = IDockablePanelPtr(*const)(const char* panelUniqueName, HWND hWndParent, const RECT& rect, Side side, const wchar_t* title);
extern const DockablePanelFactory dockablePanelFactory;

// ============================================================================

MIDL_INTERFACE("{47DD7E84-9550-42DD-AECE-296878C0C631}") ILogArea abstract : public IUnknown
{
};
_COM_SMARTPTR_TYPEDEF(ILogArea, __uuidof(ILogArea));
using LogAreaFactory = ILogAreaPtr(*const)(HWND hWndParent, const RECT& rect, ID3D11DeviceContext1* deviceContext, IDWriteFactory* dWriteFactory, ISelection* selection);
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

MIDL_INTERFACE("09C8FD2D-9A51-4B25-A3B4-3BCD3923FB9F") IEditArea : public IUnknown
{
	virtual HWND GetHWnd() const = 0;
	virtual const DrawingObjects& GetDrawingObjects() const = 0;
	virtual void EnterState (std::unique_ptr<EditState>&& state) = 0;
	virtual Port* GetCPAt (D2D1_POINT_2F dLocation, float tolerance) const = 0;
	virtual void RenderHoverCP (ID2D1RenderTarget* rt, Port* port) const = 0;
	virtual D2D1::Matrix3x2F GetZoomTransform() const = 0;
};
_COM_SMARTPTR_TYPEDEF(IEditArea, __uuidof(IEditArea));
using EditAreaFactory = IEditAreaPtr(*const)(ISimulatorApp* app,
											 IProjectWindow* pw,
											 IProject* project,
											 IActionList* actionList,
											 ISelection* selection,
											 HWND hWndParent,
											 const RECT& rect,
											 ID3D11DeviceContext1* deviceContext,
											 IDWriteFactory* dWriteFactory);
extern const EditAreaFactory editAreaFactory;

// ============================================================================

MIDL_INTERFACE("62555843-4CB8-43FB-8C91-F229A4D318BD") IProjectWindow : public IUnknown
{
	virtual HWND GetHWnd() const = 0;
	virtual IProject* GetProject() const = 0;
	virtual IEditArea* GetEditArea() const = 0;
	virtual void SelectVlan (unsigned int vlanNumber) = 0;
	virtual unsigned int GetSelectedVlanNumber() const = 0;

	struct SelectedVlanNumerChangedEvent : public Event<SelectedVlanNumerChangedEvent, void(IProjectWindow* pw, unsigned int vlanNumber)> { };
	virtual SelectedVlanNumerChangedEvent::Subscriber GetSelectedVlanNumerChangedEvent() = 0;

	struct ClosedEvent : public Event<ClosedEvent, void(IProjectWindow* pw)> { };
	virtual ClosedEvent::Subscriber GetClosedEvent() = 0;
};
_COM_SMARTPTR_TYPEDEF(IProjectWindow, __uuidof(IProjectWindow));
using ProjectWindowFactory = IProjectWindowPtr(*const)(ISimulatorApp* app,
													   IProject* project,
													   SelectionFactory selectionFactory,
													   IActionList* actionList,
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

MIDL_INTERFACE("3F68DA7D-68A0-411F-A481-D711F8527292") IActionList : public IUnknown
{
	struct ChangedEvent : public Event<ChangedEvent, void(IActionList*)> { };

	virtual ChangedEvent::Subscriber GetChangedEvent() = 0;
	virtual void AddPerformedUserAction (std::wstring&& actionName, std::unique_ptr<EditAction>&& action) = 0;
	virtual void PerformAndAddUserAction (std::wstring&& actionName, std::unique_ptr<EditAction>&& action) = 0;
	virtual size_t GetSavePointIndex() const = 0;
	virtual size_t GetEditPointIndex() const = 0;
};
_COM_SMARTPTR_TYPEDEF(IActionList, __uuidof(IActionList));
using ActionListFactory = IActionListPtr(*const)();
extern const ActionListFactory actionListFactory;

// ============================================================================

struct BridgeInsertedEvent : public Event<BridgeInsertedEvent, void(IProject*, size_t index, Bridge*)> { };
struct BridgeRemovingEvent : public Event<BridgeRemovingEvent, void(IProject*, size_t index, Bridge*)> { };

struct WireInsertedEvent : public Event<WireInsertedEvent, void(IProject*, size_t index, Wire*)> { };
struct WireRemovingEvent : public Event<WireRemovingEvent, void(IProject*, size_t index, Wire*)> { };

struct ProjectInvalidateEvent : public Event<ProjectInvalidateEvent, void(IProject*)> { };

enum class SaveProjectOption { SaveUnconditionally, SaveIfChangedAskUserFirst };

MIDL_INTERFACE("A7D9A5A8-DB3F-4147-B488-58D260365F65") IProject : public IUnknown
{
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

	virtual const std::wstring& GetFilePath() const = 0;
	virtual HRESULT Save (const wchar_t* filePath) = 0;

	size_t AddBridge (std::unique_ptr<Bridge>&& bridge);
	size_t AddWire (std::unique_ptr<Wire>&& wire);

	std::unique_ptr<Object> Remove (Object* o);

	std::pair<Wire*, size_t> GetWireConnectedToPort (const Port* port) const;
	Port* FindConnectedPort (Port* txPort) const;
};
_COM_SMARTPTR_TYPEDEF(IProject, __uuidof(IProject));
using ProjectFactory = IProjectPtr(*const)();
extern const ProjectFactory projectFactory;

// ============================================================================

MIDL_INTERFACE("8C5BA174-3A21-4953-BAA4-D04E8F2EB87F") IPropertiesWindow : public IWin32Window, public IUnknown
{
};
_COM_SMARTPTR_TYPEDEF(IPropertiesWindow, __uuidof(IPropertiesWindow));
using PropertiesWindowFactory = IPropertiesWindowPtr(*const)(ISimulatorApp* app,
															 IProjectWindow* projectWindow,
															 ISelection* selection,
															 HWND hWndParent,
															 POINT location);
extern const PropertiesWindowFactory propertiesWindowFactory;

// ============================================================================

MIDL_INTERFACE("6438D8FC-058B-4A83-A4DC-2B48AE028D09") IBridgePropsWindow : public IWin32Window, public IUnknown
{
};
_COM_SMARTPTR_TYPEDEF(IBridgePropsWindow, __uuidof(IBridgePropsWindow));
using BridgePropsWindowFactory = IBridgePropsWindowPtr(*const)(ISimulatorApp* app,
															   IProjectWindow* projectWindow,
															   ISelection* selection,
															   HWND hwndParent,
															   POINT location);
extern const BridgePropsWindowFactory bridgePropertiesControlFactory;

// ============================================================================

MIDL_INTERFACE("A6A83670-0AE9-41EC-B98E-C1FD369FEB4D") IVlanWindow : public IWin32Window, public IUnknown
{
};
_COM_SMARTPTR_TYPEDEF(IVlanWindow, __uuidof(IVlanWindow));
using VlanWindowFactory = IVlanWindowPtr(*const)(ISimulatorApp* app,
												 IProjectWindow* pw,
												 IProject* project,
												 ISelection* selection,
												 IActionList* actionList,
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
	virtual void AddProjectWindow (IProjectWindow* pw) = 0;
	virtual const std::vector<IProjectWindowPtr>& GetProjectWindows() const = 0;
	virtual const wchar_t* GetAppName() const = 0;
};

// ============================================================================

struct IMSTConfigIdDialog
{
	virtual ~IMSTConfigIdDialog() { }
	virtual UINT ShowModal (HWND hWndParent) = 0; // return IDOK, IDCANCEL, -1 (some error), 0 (hWndParent invalid or closed)
};

using MSTConfigIdDialogFactory = std::unique_ptr<IMSTConfigIdDialog>(*const)(ISimulatorApp* app,
																			 IProjectWindow* projectWindow,
																			 ISelection* selection);
extern const MSTConfigIdDialogFactory mstConfigIdDialogFactory;
