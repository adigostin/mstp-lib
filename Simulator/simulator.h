#pragma once
#include "object.h"
#include "win32/win32_lib.h"
#include "renderable_object.h"
#include "stp.h"
#include "Bridge.h"

struct simulator_app_i;
struct IProject;
struct IProjectWindow;
struct selection_i;
struct log_window_i;
class Bridge;
class Port;
class Wire;

static constexpr unsigned char DefaultConfigTableDigest[16] = { 0xAC, 0x36, 0x17, 0x7F, 0x50, 0x28, 0x3C, 0xD4, 0xB8, 0x38, 0x21, 0xD8, 0xAB, 0x26, 0xDE, 0x62 };

// Maximum VLAN number supported by the simulator (too large a number would complicate the UI).
// The maximum VLAN number allowed by specs is 4094.
static constexpr uint32_t max_vlan_number = 16;

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

struct __declspec(novtable) selection_i
{
	virtual ~selection_i() { }

	virtual const std::vector<edge::object*>& objects() const = 0;
	virtual void select (edge::object* o) = 0;
	virtual void clear() = 0;
	virtual void add (edge::object* o) = 0;
	virtual void remove (edge::object* o) = 0;

	bool contains (edge::object* o) const { return std::find (objects().begin(), objects().end(), o) != objects().end(); }

	struct added_e : public edge::event<added_e, selection_i*, edge::object*> { };
	virtual added_e::subscriber added() = 0;

	struct removing_e : public edge::event<removing_e, selection_i*, edge::object*> { };
	virtual removing_e::subscriber removing() = 0;

	struct changed_e : public edge::event<changed_e, selection_i*> { };
	virtual changed_e::subscriber changed() = 0;
};
using selection_factory_t = std::unique_ptr<selection_i>(*const)(IProject* project);
extern const selection_factory_t selection_factory;

// ============================================================================

struct __declspec(novtable) log_window_i abstract : virtual edge::win32_window_i
{
	virtual ~log_window_i() { }
};
using log_window_factory_t = std::unique_ptr<log_window_i>(*const)(HINSTANCE hInstance, HWND hWndParent, const RECT& rect, ID3D11DeviceContext1* d3d_dc, IDWriteFactory* dWriteFactory, selection_i* selection);
extern const log_window_factory_t log_window_factory;

// ============================================================================

class edit_state;

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

struct __declspec(novtable) edit_area_i : virtual edge::win32_window_i
{
	virtual const struct drawing_resources& drawing_resources() const = 0;
	virtual void EnterState (std::unique_ptr<edit_state>&& state) = 0;
	virtual Port* GetCPAt (D2D1_POINT_2F dLocation, float tolerance) const = 0;
	virtual void RenderSnapRect (ID2D1RenderTarget* rt, D2D1_POINT_2F wLocation) const = 0;
	virtual void render_hint (ID2D1RenderTarget* rt,
							 D2D1_POINT_2F dLocation,
							 const wchar_t* text,
							 DWRITE_TEXT_ALIGNMENT ha,
							 DWRITE_PARAGRAPH_ALIGNMENT va,
							 bool smallFont = false) const = 0;
	virtual D2D1::Matrix3x2F GetZoomTransform() const = 0;
};
using edit_area_factory_t = std::unique_ptr<edit_area_i>(*const)(simulator_app_i* app,
																 IProjectWindow* pw,
																 IProject* project,
																 selection_i* selection,
																 HWND hWndParent,
																 const RECT& rect,
																 ID3D11DeviceContext1* d3d_dc,
																 IDWriteFactory* dWriteFactory);
extern const edit_area_factory_t edit_area_factory;

// ============================================================================

struct __declspec(novtable) IProjectWindow : public virtual edge::win32_window_i
{
	struct selected_vlan_number_changed_e : public edge::event<selected_vlan_number_changed_e, IProjectWindow*, uint32_t> { };

	virtual IProject* GetProject() const = 0;
	virtual edit_area_i* GetEditArea() const = 0;
	virtual void select_vlan (uint32_t vlanNumber) = 0;
	virtual uint32_t selected_vlan_number() const = 0;
	virtual selected_vlan_number_changed_e::subscriber selected_vlan_number_changed() = 0;
};

struct project_window_create_params
{
	simulator_app_i*                 app;
	const std::shared_ptr<IProject>& project;
	selection_factory_t              selection_factory;
	edit_area_factory_t              edit_area_factory;
	bool     show_property_grid;
	bool     showLogWindow;
	uint32_t selectedVlan;
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
	virtual mac_address AllocMacAddressRange (size_t count) = 0;
	virtual const std::wstring& GetFilePath() const = 0;
	virtual HRESULT Save (const wchar_t* filePath) = 0;
	virtual void Load (const wchar_t* filePath) = 0;
	virtual bool IsWireForwarding (Wire* wire, uint32_t vlanNumber, _Out_opt_ bool* hasLoop) const = 0;
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

struct __declspec(novtable) vlan_window_i : virtual edge::win32_window_i
{
};
using vlan_window_factory_t = std::unique_ptr<vlan_window_i>(*const)(
	simulator_app_i* app,
	IProjectWindow* pw,
	const std::shared_ptr<IProject>& project,
	selection_i* selection,
	HWND hWndParent,
	POINT location,
	ID3D11DeviceContext1* d3d_dc,
	IDWriteFactory* dwrite_factory);
extern const vlan_window_factory_t vlan_window_factory;

// ============================================================================

struct simulator_app_i
{
	struct project_window_added_e    : edge::event<project_window_added_e, IProjectWindow*> { };
	struct project_window_removing_e : edge::event<project_window_removing_e, IProjectWindow*> { };
	struct project_window_removed_e  : edge::event<project_window_removed_e, IProjectWindow*> { };

	virtual HINSTANCE GetHInstance() const = 0;
	virtual const wchar_t* GetRegKeyPath() const = 0;
	virtual void add_project_window (std::unique_ptr<IProjectWindow>&& pw) = 0;
	virtual const std::vector<std::unique_ptr<IProjectWindow>>& project_windows() const = 0;
	virtual const wchar_t* GetAppName() const = 0;
	virtual const wchar_t* GetAppVersionString() const = 0;
	virtual project_window_added_e::subscriber project_window_added() = 0;
	virtual project_window_removing_e::subscriber project_window_removing() = 0;
	virtual project_window_removed_e::subscriber project_window_removed() = 0;
};

// ============================================================================

extern const edge::PropertyEditorFactory mstConfigIdDialogFactory;
