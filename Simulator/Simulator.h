#pragma once
#include "Win32/EventManager.h"
#include "Win32/Win32Defs.h"
#include "UtilityFunctions.h"
#include "stp.h"

struct ISimulatorApp;
struct IProject;
struct IProjectWindow;
struct ISelection;
struct ILogArea;
struct IActionList;
struct DrawingObjects;
class Object;
class Bridge;
class Port;
class Wire;

static constexpr unsigned char DefaultConfigTableDigest[16] = { 0xAC, 0x36, 0x17, 0x7F, 0x50, 0x28, 0x3C, 0xD4, 0xB8, 0x38, 0x21, 0xD8, 0xAB, 0x26, 0xDE, 0x62 };

static constexpr unsigned int MaxVlanNumber = 16; // 4094 must be maximum

enum class MouseButton
{
	None = 0,
	Left = 1,
	Right = 2,
	Middle = 4,
};

// ============================================================================

MIDL_INTERFACE("3ADCEF4B-9335-4DD7-8016-5958883A4347") ISelection : public IUnknown
{
	virtual const std::vector<Object*>& GetObjects() const = 0;
	virtual void Select (Object* o) = 0;
	virtual void Clear() = 0;
	virtual void Add (Object* o) = 0;
	virtual void Remove (Object* o) = 0;

	bool Contains (Object* o) const { return std::find (GetObjects().begin(), GetObjects().end(), o) != GetObjects().end(); }

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

MIDL_INTERFACE("{47DD7E84-9550-42DD-AECE-296878C0C631}") ILogArea abstract : public IUnknown
{
};
_COM_SMARTPTR_TYPEDEF(ILogArea, __uuidof(ILogArea));
using LogAreaFactory = ILogAreaPtr(*const)(HINSTANCE hInstance, HWND hWndParent, const RECT& rect, IDWriteFactory* dWriteFactory, ISelection* selection);
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

MIDL_INTERFACE("09C8FD2D-9A51-4B25-A3B4-3BCD3923FB9F") IEditArea : public IWin32Window
{
	virtual const DrawingObjects& GetDrawingObjects() const = 0;
	virtual void EnterState (std::unique_ptr<EditState>&& state) = 0;
	virtual Port* GetCPAt (D2D1_POINT_2F dLocation, float tolerance) const = 0;
	virtual void RenderSnapRect (ID2D1RenderTarget* rt, D2D1_POINT_2F wLocation) const = 0;
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
											 IDWriteFactory* dWriteFactory);
extern const EditAreaFactory editAreaFactory;

// ============================================================================

MIDL_INTERFACE("62555843-4CB8-43FB-8C91-F229A4D318BD") IProjectWindow : public IWin32Window
{
	struct SelectedVlanNumerChangedEvent : public Event<SelectedVlanNumerChangedEvent, void(IProjectWindow* pw, unsigned int vlanNumber)> { };
	struct DestroyingEvent : public Event<DestroyingEvent, void(IProjectWindow* pw)> { };

	virtual IProject* GetProject() const = 0;
	virtual IEditArea* GetEditArea() const = 0;
	virtual void SelectVlan (unsigned int vlanNumber) = 0;
	virtual unsigned int GetSelectedVlanNumber() const = 0;
	virtual SelectedVlanNumerChangedEvent::Subscriber GetSelectedVlanNumerChangedEvent() = 0;
	virtual DestroyingEvent::Subscriber GetDestroyingEvent() = 0;
	virtual void PostWork (std::function<void()>&& work) = 0;
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
	virtual ~EditAction() = default;
	virtual void Redo() = 0;
	virtual void Undo() = 0;
	virtual std::string GetName() const = 0;
};

MIDL_INTERFACE("3F68DA7D-68A0-411F-A481-D711F8527292") IActionList : public IUnknown
{
	struct ChangedEvent : public Event<ChangedEvent, void(IActionList*)> { };

	virtual ChangedEvent::Subscriber GetChangedEvent() = 0;
	virtual void AddPerformedUserAction (std::unique_ptr<EditAction>&& action) = 0;
	virtual void PerformAndAddUserAction (std::unique_ptr<EditAction>&& action) = 0;
	virtual size_t GetSavePointIndex() const = 0;
	virtual size_t GetEditPointIndex() const = 0;
	virtual size_t GetCount() const = 0;
	virtual void SetSavePoint() = 0;
	virtual void Undo() = 0;
	virtual void Redo() = 0;
	bool ChangedSinceLastSave() const { return GetEditPointIndex() != GetSavePointIndex(); }
	bool CanUndo() const { return GetEditPointIndex() > 0; }
	bool CanRedo() const { return GetEditPointIndex() < GetCount(); }
	virtual EditAction* GetUndoableAction() const = 0;
	virtual EditAction* GetRedoableAction() const = 0;
};
_COM_SMARTPTR_TYPEDEF(IActionList, __uuidof(IActionList));
using ActionListFactory = IActionListPtr(*const)();
extern const ActionListFactory actionListFactory;

// ============================================================================

struct BridgeInsertedEvent : public Event<BridgeInsertedEvent, void(IProject*, size_t index, Bridge*)> { };
struct BridgeRemovingEvent : public Event<BridgeRemovingEvent, void(IProject*, size_t index, Bridge*)> { };

struct WireInsertedEvent : public Event<WireInsertedEvent, void(IProject*, size_t index, Wire*)> { };
struct WireRemovingEvent : public Event<WireRemovingEvent, void(IProject*, size_t index, Wire*)> { };

enum class SaveProjectOption { SaveUnconditionally, SaveIfChangedAskUserFirst };

MIDL_INTERFACE("A7D9A5A8-DB3F-4147-B488-58D260365F65") IProject : public IUnknown
{
	struct ConvertedWirePoint
	{
		Wire* wire;
		size_t pointIndex;
		Port* port;
	};

	struct InvalidateEvent : public Event<InvalidateEvent, void(IProject*)> { };
	struct LoadedEvent : public Event<LoadedEvent, void(IProject*)> { };

	virtual const std::vector<std::unique_ptr<Bridge>>& GetBridges() const = 0;
	virtual void InsertBridge (size_t index, std::unique_ptr<Bridge>&& bridge, std::vector<ConvertedWirePoint>* convertedWirePoints) = 0;
	virtual std::unique_ptr<Bridge> RemoveBridge (size_t index, std::vector<ConvertedWirePoint>* convertedWirePoints) = 0;
	virtual BridgeInsertedEvent::Subscriber GetBridgeInsertedEvent() = 0;
	virtual BridgeRemovingEvent::Subscriber GetBridgeRemovingEvent() = 0;
	virtual const std::vector<std::unique_ptr<Wire>>& GetWires() const = 0;
	virtual void InsertWire (size_t index, std::unique_ptr<Wire>&& wire) = 0;
	virtual std::unique_ptr<Wire> RemoveWire (size_t index) = 0;
	virtual WireInsertedEvent::Subscriber GetWireInsertedEvent() = 0;
	virtual WireRemovingEvent::Subscriber GetWireRemovingEvent() = 0;
	virtual InvalidateEvent::Subscriber GetInvalidateEvent() = 0;
	virtual LoadedEvent::Subscriber GetLoadedEvent() = 0;
	virtual STP_BRIDGE_ADDRESS AllocMacAddressRange (size_t count) = 0;
	virtual const std::wstring& GetFilePath() const = 0;
	virtual HRESULT Save (const wchar_t* filePath) = 0;
	virtual void Load (const wchar_t* filePath) = 0;
	virtual bool IsWireForwarding (Wire* wire, unsigned int vlanNumber, _Out_opt_ bool* hasLoop) const = 0;
	virtual void PauseSimulation() = 0;
	virtual void ResumeSimulation() = 0;
	virtual bool IsSimulationPaused() const = 0;

	std::pair<Wire*, size_t> GetWireConnectedToPort (const Port* port) const;
	Port* FindConnectedPort (Port* txPort) const;
};
_COM_SMARTPTR_TYPEDEF(IProject, __uuidof(IProject));
using ProjectFactory = IProjectPtr(*const)();
extern const ProjectFactory projectFactory;

// ============================================================================

MIDL_INTERFACE("8C5BA174-3A21-4953-BAA4-D04E8F2EB87F") IPropertiesWindow : public IWin32Window
{
};
_COM_SMARTPTR_TYPEDEF(IPropertiesWindow, __uuidof(IPropertiesWindow));
using PropertiesWindowFactory = IPropertiesWindowPtr(*const)(ISimulatorApp* app,
															 IProjectWindow* projectWindow,
															 IProject* project,
															 ISelection* selection,
															 IActionList* actionList,
															 HWND hWndParent,
															 POINT location);
extern const PropertiesWindowFactory propertiesWindowFactory;

// ============================================================================

MIDL_INTERFACE("6438D8FC-058B-4A83-A4DC-2B48AE028D09") IBridgePropsWindow : public IWin32Window
{
};
_COM_SMARTPTR_TYPEDEF(IBridgePropsWindow, __uuidof(IBridgePropsWindow));
using BridgePropsWindowFactory = IBridgePropsWindowPtr(*const)(ISimulatorApp* app,
															   IProjectWindow* projectWindow,
															   IProject* project,
															   ISelection* selection,
															   IActionList* actionList,
															   HWND hwndParent,
															   POINT location);
extern const BridgePropsWindowFactory bridgePropertiesControlFactory;

// ============================================================================

MIDL_INTERFACE("A6A83670-0AE9-41EC-B98E-C1FD369FEB4D") IVlanWindow : public IWin32Window
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
	struct ProjectWindowAddedEvent : public Event<ProjectWindowAddedEvent, void(IProjectWindow*)> { };
	struct ProjectWindowRemovingEvent : public Event<ProjectWindowRemovingEvent, void(IProjectWindow*)> { };
	struct ProjectWindowRemovedEvent : public Event<ProjectWindowRemovedEvent, void(IProjectWindow*)> { };

	virtual HINSTANCE GetHInstance() const = 0;
	virtual IDWriteFactory* GetDWriteFactory() const = 0;
	virtual const wchar_t* GetRegKeyPath() const = 0;
	virtual void AddProjectWindow (IProjectWindow* pw) = 0;
	virtual const std::vector<IProjectWindowPtr>& GetProjectWindows() const = 0;
	virtual const wchar_t* GetAppName() const = 0;
	virtual const wchar_t* GetAppVersionString() const = 0;
	virtual ProjectWindowAddedEvent::Subscriber GetProjectWindowAddedEvent() = 0;
	virtual ProjectWindowRemovingEvent::Subscriber GetProjectWindowRemovingEvent() = 0;
	virtual ProjectWindowRemovedEvent::Subscriber GetProjectWindowRemovedEvent() = 0;
};

// ============================================================================

struct IMSTConfigIdDialog
{
	virtual ~IMSTConfigIdDialog() { }
	virtual UINT ShowModal (HWND hWndParent) = 0; // return IDOK, IDCANCEL, -1 (some error), 0 (hWndParent invalid or closed)
};

using MSTConfigIdDialogFactory = std::unique_ptr<IMSTConfigIdDialog>(*const)(ISimulatorApp* app,
																			 IProjectWindow* projectWindow,
																			 IProject* project,
																			 ISelection* selection);
extern const MSTConfigIdDialogFactory mstConfigIdDialogFactory;
