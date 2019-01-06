#pragma once
#include "object.h"
#include "win32/win32_lib.h"
#include "renderable_object.h"
#include "stp.h"

struct ISimulatorApp;
struct IProject;
struct IProjectWindow;
struct ISelection;
struct log_window_i;
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

struct __declspec(novtable) ISelection
{
	virtual ~ISelection() { }

	virtual const std::vector<edge::object*>& GetObjects() const = 0;
	virtual void Select (edge::object* o) = 0;
	virtual void Clear() = 0;
	virtual void Add (edge::object* o) = 0;
	virtual void Remove (edge::object* o) = 0;

	bool Contains (edge::object* o) const { return std::find (GetObjects().begin(), GetObjects().end(), o) != GetObjects().end(); }

	struct AddedToSelectionEvent : public edge::event<AddedToSelectionEvent, ISelection*, edge::object*> { };
	virtual AddedToSelectionEvent::subscriber GetAddedToSelectionEvent() = 0;

	struct RemovingFromSelectionEvent : public edge::event<RemovingFromSelectionEvent, ISelection*, edge::object*> { };
	virtual RemovingFromSelectionEvent::subscriber GetRemovingFromSelectionEvent() = 0;

	struct ChangedEvent : public edge::event<ChangedEvent, ISelection*> { };
	virtual ChangedEvent::subscriber GetChangedEvent() = 0;
};
using SelectionFactory = std::unique_ptr<ISelection>(*const)(IProject* project);
extern const SelectionFactory selectionFactory;

// ============================================================================

struct __declspec(novtable) log_window_i abstract : virtual edge::win32_window_i
{
	virtual ~log_window_i() { }
};
using log_window_factory_t = std::unique_ptr<log_window_i>(*const)(HINSTANCE hInstance, HWND hWndParent, const RECT& rect, ID3D11DeviceContext1* d3d_dc, IDWriteFactory* dWriteFactory, ISelection* selection);
extern const log_window_factory_t log_window_factory;

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

struct __declspec(novtable) IEditArea : virtual edge::win32_window_i
{
	virtual const DrawingObjects& GetDrawingObjects() const = 0;
	virtual void EnterState (std::unique_ptr<EditState>&& state) = 0;
	virtual Port* GetCPAt (D2D1_POINT_2F dLocation, float tolerance) const = 0;
	virtual void RenderSnapRect (ID2D1RenderTarget* rt, D2D1_POINT_2F wLocation) const = 0;
	virtual void RenderHint (ID2D1RenderTarget* rt,
							 D2D1_POINT_2F dLocation,
							 const wchar_t* text,
							 DWRITE_TEXT_ALIGNMENT ha,
							 DWRITE_PARAGRAPH_ALIGNMENT va,
							 bool smallFont = false) const = 0;
	virtual D2D1::Matrix3x2F GetZoomTransform() const = 0;
};
using EditAreaFactory = std::unique_ptr<IEditArea>(*const)(ISimulatorApp* app,
														   IProjectWindow* pw,
														   IProject* project,
														   ISelection* selection,
														   HWND hWndParent,
														   const RECT& rect,
														   ID3D11DeviceContext1* d3d_dc,
														   IDWriteFactory* dWriteFactory);
extern const EditAreaFactory editAreaFactory;

// ============================================================================

struct __declspec(novtable) IProjectWindow : public virtual edge::win32_window_i
{
	struct SelectedVlanNumerChangedEvent : public edge::event<SelectedVlanNumerChangedEvent, IProjectWindow*, unsigned int> { };

	virtual IProject* GetProject() const = 0;
	virtual IEditArea* GetEditArea() const = 0;
	virtual void SelectVlan (unsigned int vlanNumber) = 0;
	virtual unsigned int GetSelectedVlanNumber() const = 0;
	virtual SelectedVlanNumerChangedEvent::subscriber GetSelectedVlanNumerChangedEvent() = 0;
};

struct project_window_create_params
{
	ISimulatorApp*                   app;
	const std::shared_ptr<IProject>& project;
	SelectionFactory                 selectionFactory;
	EditAreaFactory                  editAreaFactory;
	bool     showPropertiesWindow;
	bool     showLogWindow;
	uint16_t selectedVlan;
	int      nCmdShow;
	ID3D11DeviceContext1* d3d_dc;
	IDWriteFactory*       dwrite_factory;
};

using ProjectWindowFactory = std::unique_ptr<IProjectWindow>(*const)(const project_window_create_params& create_params);
extern const ProjectWindowFactory projectWindowFactory;

// ============================================================================

struct BridgeInsertedEvent : public edge::event<BridgeInsertedEvent, IProject*, size_t, Bridge*> { };
struct BridgeRemovingEvent : public edge::event<BridgeRemovingEvent, IProject*, size_t, Bridge*> { };

struct WireInsertedEvent : public edge::event<WireInsertedEvent, IProject*, size_t, Wire*> { };
struct WireRemovingEvent : public edge::event<WireRemovingEvent, IProject*, size_t, Wire*> { };

enum class SaveProjectOption { SaveUnconditionally, SaveIfChangedAskUserFirst };

struct __declspec(novtable) IProject
{
	virtual ~IProject() { }

	struct ConvertedWirePoint
	{
		Wire* wire;
		size_t pointIndex;
		Port* port;
	};

	struct invalidate_e : public edge::event<invalidate_e, IProject*> { };
	struct LoadedEvent : public edge::event<LoadedEvent, IProject*> { };
	struct ChangedFlagChangedEvent : public edge::event<ChangedFlagChangedEvent, IProject*> { };
	struct ChangedEvent : public edge::event<ChangedEvent, IProject*> { };

	virtual const std::vector<std::unique_ptr<Bridge>>& GetBridges() const = 0;
	virtual void InsertBridge (size_t index, std::unique_ptr<Bridge>&& bridge) = 0;
	virtual std::unique_ptr<Bridge> RemoveBridge (size_t index) = 0;
	virtual BridgeInsertedEvent::subscriber GetBridgeInsertedEvent() = 0;
	virtual BridgeRemovingEvent::subscriber GetBridgeRemovingEvent() = 0;
	virtual const std::vector<std::unique_ptr<Wire>>& GetWires() const = 0;
	virtual void InsertWire (size_t index, std::unique_ptr<Wire>&& wire) = 0;
	virtual std::unique_ptr<Wire> RemoveWire (size_t index) = 0;
	virtual WireInsertedEvent::subscriber GetWireInsertedEvent() = 0;
	virtual WireRemovingEvent::subscriber GetWireRemovingEvent() = 0;
	virtual invalidate_e::subscriber GetInvalidateEvent() = 0;
	virtual LoadedEvent::subscriber GetLoadedEvent() = 0;
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
	virtual ChangedFlagChangedEvent::subscriber GetChangedFlagChangedEvent() = 0;
	virtual ChangedEvent::subscriber GetChangedEvent() = 0;

	std::pair<Wire*, size_t> GetWireConnectedToPort (const Port* port) const;
	Port* FindConnectedPort (Port* txPort) const;
	std::unique_ptr<Wire> RemoveWire (Wire* w);
	std::unique_ptr<Bridge> RemoveBridge (Bridge* b);
};
using ProjectFactory = std::shared_ptr<IProject>(*const)();
extern const ProjectFactory projectFactory;

// ============================================================================

struct __declspec(novtable) IPropertiesWindow : virtual edge::win32_window_i
{
	virtual ~IPropertiesWindow() { }
};
using PropertiesWindowFactory = std::unique_ptr<IPropertiesWindow>(*const)(ISimulatorApp* app,
																		   IProjectWindow* projectWindow,
																		   IProject* project,
																		   ISelection* selection,
																		   const RECT& rect,
																		   HWND hWndParent,
																		   ID3D11DeviceContext1* d3d_dc,
																		   IDWriteFactory* dwrite_factory);
extern const PropertiesWindowFactory propertiesWindowFactory;

// ============================================================================

struct __declspec(novtable) IVlanWindow : virtual edge::win32_window_i
{
};
using VlanWindowFactory = std::unique_ptr<IVlanWindow>(*const)(ISimulatorApp* app,
												 IProjectWindow* pw,
												 const std::shared_ptr<IProject>& project,
												 ISelection* selection,
												 HWND hWndParent,
												 POINT location);
extern const VlanWindowFactory vlanWindowFactory;

// ============================================================================

struct ISimulatorApp
{
	struct ProjectWindowAddedEvent : public edge::event<ProjectWindowAddedEvent, IProjectWindow*> { };
	struct ProjectWindowRemovingEvent : public edge::event<ProjectWindowRemovingEvent, IProjectWindow*> { };
	struct ProjectWindowRemovedEvent : public edge::event<ProjectWindowRemovedEvent, IProjectWindow*> { };

	virtual HINSTANCE GetHInstance() const = 0;
	virtual const wchar_t* GetRegKeyPath() const = 0;
	virtual void AddProjectWindow (std::unique_ptr<IProjectWindow>&& pw) = 0;
	virtual const std::vector<std::unique_ptr<IProjectWindow>>& GetProjectWindows() const = 0;
	virtual const wchar_t* GetAppName() const = 0;
	virtual const wchar_t* GetAppVersionString() const = 0;
	virtual ProjectWindowAddedEvent::subscriber GetProjectWindowAddedEvent() = 0;
	virtual ProjectWindowRemovingEvent::subscriber GetProjectWindowRemovingEvent() = 0;
	virtual ProjectWindowRemovedEvent::subscriber GetProjectWindowRemovedEvent() = 0;
};

// ============================================================================

extern const edge::PropertyEditorFactory mstConfigIdDialogFactory;
