#pragma once
#include "Win32/Win32Defs.h"
#include "Object.h"
#include "stp.h"

struct ISimulatorApp;
struct IProject;
struct IProjectWindow;
struct ISelection;
struct ILogArea;
class Bridge;
class Port;
class Wire;

static constexpr unsigned char DefaultConfigTableDigest[16] = { 0xAC, 0x36, 0x17, 0x7F, 0x50, 0x28, 0x3C, 0xD4, 0xB8, 0x38, 0x21, 0xD8, 0xAB, 0x26, 0xDE, 0x62 };

static constexpr unsigned int MaxVlanNumber = 16; // 4094 must be maximum

static constexpr wchar_t FileExtensionWithoutDot[] = L"stp";
static constexpr wchar_t FileExtensionWithDot[] = L".stp";

enum class MouseButton
{
	None = 0,
	Left = 1,
	Right = 2,
	Middle = 4,
};

// ============================================================================

struct __declspec(novtable) ISelection : public IUnknown
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
using SelectionFactory = com_ptr<ISelection>(*const)(IProject* project);
extern const SelectionFactory selectionFactory;

// ============================================================================

struct __declspec(novtable) ILogArea abstract : public IWin32Window
{
};
using LogAreaFactory = com_ptr<ILogArea>(*const)(HINSTANCE hInstance, HWND hWndParent, const RECT& rect, IDWriteFactory* dWriteFactory, ISelection* selection);
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

struct __declspec(novtable) IEditArea : public IWin32Window
{
	virtual const DrawingObjects& GetDrawingObjects() const = 0;
	virtual void EnterState (std::unique_ptr<EditState>&& state) = 0;
	virtual Port* GetCPAt (D2D1_POINT_2F dLocation, float tolerance) const = 0;
	virtual void RenderSnapRect (ID2D1RenderTarget* rt, D2D1_POINT_2F wLocation) const = 0;
	virtual void RenderHint (ID2D1RenderTarget* rt, float centerX, float y, const wchar_t* text, bool smallFont = false, bool alignBottom = false) const = 0;
	virtual D2D1::Matrix3x2F GetZoomTransform() const = 0;
};
using EditAreaFactory = com_ptr<IEditArea>(*const)(ISimulatorApp* app,
											 IProjectWindow* pw,
											 IProject* project,
											 ISelection* selection,
											 HWND hWndParent,
											 const RECT& rect,
											 IDWriteFactory* dWriteFactory);
extern const EditAreaFactory editAreaFactory;

// ============================================================================

struct __declspec(novtable) IProjectWindow : public IWindowWithWorkQueue
{
	struct SelectedVlanNumerChangedEvent : public Event<SelectedVlanNumerChangedEvent, void(IProjectWindow* pw, unsigned int vlanNumber)> { };
	struct DestroyingEvent : public Event<DestroyingEvent, void(IProjectWindow* pw)> { };

	virtual IProject* GetProject() const = 0;
	virtual IEditArea* GetEditArea() const = 0;
	virtual void SelectVlan (unsigned int vlanNumber) = 0;
	virtual unsigned int GetSelectedVlanNumber() const = 0;
	virtual SelectedVlanNumerChangedEvent::Subscriber GetSelectedVlanNumerChangedEvent() = 0;
	virtual DestroyingEvent::Subscriber GetDestroyingEvent() = 0;
};
using ProjectWindowFactory = com_ptr<IProjectWindow>(*const)(ISimulatorApp* app,
													   IProject* project,
													   SelectionFactory selectionFactory,
													   EditAreaFactory editAreaFactory,
													   bool showPropertiesWindow,
													   bool showLogWindow,
													   int nCmdShow,
													   unsigned int selectedVlan);
extern const ProjectWindowFactory projectWindowFactory;

// ============================================================================

struct BridgeInsertedEvent : public Event<BridgeInsertedEvent, void(IProject*, size_t index, Bridge*)> { };
struct BridgeRemovingEvent : public Event<BridgeRemovingEvent, void(IProject*, size_t index, Bridge*)> { };

struct WireInsertedEvent : public Event<WireInsertedEvent, void(IProject*, size_t index, Wire*)> { };
struct WireRemovingEvent : public Event<WireRemovingEvent, void(IProject*, size_t index, Wire*)> { };

enum class SaveProjectOption { SaveUnconditionally, SaveIfChangedAskUserFirst };

struct __declspec(novtable) IProject : public IUnknown
{
	struct ConvertedWirePoint
	{
		Wire* wire;
		size_t pointIndex;
		Port* port;
	};

	struct InvalidateEvent : public Event<InvalidateEvent, void(IProject*)> { };
	struct LoadedEvent : public Event<LoadedEvent, void(IProject*)> { };
	struct ChangedFlagChangedEvent : public Event<ChangedFlagChangedEvent, void(IProject*)> { };
	struct ChangedEvent : public Event<ChangedEvent, void(IProject*)> { };

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
	virtual bool GetChangedFlag() const = 0;
	virtual void SetChangedFlag (bool projectChangedFlag) = 0;
	virtual ChangedFlagChangedEvent::Subscriber GetChangedFlagChangedEvent() = 0;
	virtual ChangedEvent::Subscriber GetChangedEvent() = 0;

	std::pair<Wire*, size_t> GetWireConnectedToPort (const Port* port) const;
	Port* FindConnectedPort (Port* txPort) const;
};
using ProjectFactory = com_ptr<IProject>(*const)();
extern const ProjectFactory projectFactory;

// ============================================================================

struct __declspec(novtable) IPropertiesWindow : public IWin32Window
{
};
using PropertiesWindowFactory = com_ptr<IPropertiesWindow>(*const)(ISimulatorApp* app,
															 IProjectWindow* projectWindow,
															 IProject* project,
															 ISelection* selection,
															 const RECT& rect,
															 HWND hWndParent);
extern const PropertiesWindowFactory propertiesWindowFactory;

// ============================================================================

struct __declspec(novtable) IVlanWindow : public IWin32Window
{
};
using VlanWindowFactory = com_ptr<IVlanWindow>(*const)(ISimulatorApp* app,
												 IProjectWindow* pw,
												 IProject* project,
												 ISelection* selection,
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
	virtual const std::vector<com_ptr<IProjectWindow>>& GetProjectWindows() const = 0;
	virtual const wchar_t* GetAppName() const = 0;
	virtual const wchar_t* GetAppVersionString() const = 0;
	virtual ProjectWindowAddedEvent::Subscriber GetProjectWindowAddedEvent() = 0;
	virtual ProjectWindowRemovingEvent::Subscriber GetProjectWindowRemovingEvent() = 0;
	virtual ProjectWindowRemovedEvent::Subscriber GetProjectWindowRemovedEvent() = 0;
};

// ============================================================================

extern const PropertyEditorFactory mstConfigIdDialogFactory;
