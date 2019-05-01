#pragma once
#include "object.h"
#include "win32/win32_lib.h"
#include "renderable_object.h"
#include "stp.h"
#include "bridge.h"
#include "wire.h"

struct simulator_app_i;
struct project_i;
struct project_window_i;
struct selection_i;
struct log_window_i;
class bridge;
class port;
class wire;

using edge::property_changing_e;
using edge::property_changed_e;
using edge::property_change_args;

static constexpr unsigned char DefaultConfigTableDigest[16] = { 0xAC, 0x36, 0x17, 0x7F, 0x50, 0x28, 0x3C, 0xD4, 0xB8, 0x38, 0x21, 0xD8, 0xAB, 0x26, 0xDE, 0x62 };

// Maximum VLAN number supported by the simulator (too large a number would complicate the UI).
// The maximum VLAN number allowed by specs is 4094.
static constexpr uint32_t max_vlan_number = 16;

static constexpr wchar_t FileExtensionWithoutDot[] = L"stp";
static constexpr wchar_t FileExtensionWithDot[] = L".stp";

static constexpr char app_version_string[] = "2.2";

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
using selection_factory_t = std::unique_ptr<selection_i>(*)(project_i* project);

// ============================================================================

struct __declspec(novtable) log_window_i abstract : virtual edge::win32_window_i
{
	virtual ~log_window_i() { }
};
using log_window_factory_t = std::unique_ptr<log_window_i>(*const)(HINSTANCE hInstance, HWND hWndParent, const RECT& rect, ID3D11DeviceContext1* d3d_dc, IDWriteFactory* dWriteFactory, selection_i* selection, const std::shared_ptr<project_i>& project);
extern const log_window_factory_t log_window_factory;

// ============================================================================

class edit_state;

static constexpr float SnapDistance = 6;

struct DialogProcResult
{
	INT_PTR dialogProcResult;
	LRESULT messageResult;
};

struct mouse_location
{
	POINT pt;
	D2D1_POINT_2F d;
	D2D1_POINT_2F w;
};

struct __declspec(novtable) edit_window_i : virtual edge::win32_window_i
{
	virtual const struct drawing_resources& drawing_resources() const = 0;
	virtual void EnterState (std::unique_ptr<edit_state>&& state) = 0;
	virtual port* GetCPAt (D2D1_POINT_2F dLocation, float tolerance) const = 0;
	virtual void RenderSnapRect (ID2D1RenderTarget* rt, D2D1_POINT_2F wLocation) const = 0;
	virtual void render_hint (ID2D1RenderTarget* rt,
							 D2D1_POINT_2F dLocation,
							 const wchar_t* text,
							 DWRITE_TEXT_ALIGNMENT ha,
							 DWRITE_PARAGRAPH_ALIGNMENT va,
							 bool smallFont = false) const = 0;
	virtual D2D1::Matrix3x2F GetZoomTransform() const = 0;
};
struct edit_window_create_params
{
	simulator_app_i* app;
	project_window_i* pw;
	project_i* project;
	selection_i* selection;
	HWND hWndParent;
	RECT rect;
	ID3D11DeviceContext1* d3d_dc;
	IDWriteFactory* dWriteFactory;
};
using edit_window_factory_t = std::unique_ptr<edit_window_i>(*)(const edit_window_create_params& cps);

// ============================================================================

struct __declspec(novtable) project_window_i : public virtual edge::win32_window_i
{
	struct selected_vlan_number_changed_e : public edge::event<selected_vlan_number_changed_e, project_window_i*, uint32_t> { };

	virtual project_i* project() const = 0;
	virtual void select_vlan (uint32_t vlanNumber) = 0;
	virtual uint32_t selected_vlan_number() const = 0;
	virtual selected_vlan_number_changed_e::subscriber selected_vlan_number_changed() = 0;
};

struct project_window_create_params
{
	simulator_app_i*           app;
	const std::shared_ptr<project_i>& project;
	bool     show_property_grid;
	bool     showLogWindow;
	uint32_t selectedVlan;
	int      nCmdShow;
	ID3D11DeviceContext1* d3d_dc;
	IDWriteFactory*       dwrite_factory;
};

using project_window_factory_t = std::unique_ptr<project_window_i>(*)(const project_window_create_params& create_params);

// ============================================================================

enum class save_project_option { save_unconditionally, save_if_changed_ask_user_first };

struct __declspec(novtable) project_i
{
	virtual ~project_i() { }

	struct invalidate_e : public edge::event<invalidate_e, project_i*> { };
	struct LoadedEvent : public edge::event<LoadedEvent, project_i*> { };
	struct ChangedFlagChangedEvent : public edge::event<ChangedFlagChangedEvent, project_i*> { };
	struct ChangedEvent : public edge::event<ChangedEvent, project_i*> { };

	virtual const std::vector<std::unique_ptr<bridge>>& bridges() const = 0;
	virtual void insert_bridge (size_t index, std::unique_ptr<bridge>&& bridge) = 0;
	virtual std::unique_ptr<bridge> remove_bridge (size_t index) = 0;
	virtual const std::vector<std::unique_ptr<wire>>& wires() const = 0;
	virtual void insert_wire (size_t index, std::unique_ptr<wire>&& wire) = 0;
	virtual std::unique_ptr<wire> remove_wire (size_t index) = 0;
	virtual invalidate_e::subscriber GetInvalidateEvent() = 0;
	virtual LoadedEvent::subscriber GetLoadedEvent() = 0;
	virtual mac_address AllocMacAddressRange (size_t count) = 0;
	virtual const std::wstring& GetFilePath() const = 0;
	virtual HRESULT save (const wchar_t* filePath) = 0;
	virtual HRESULT load (const wchar_t* filePath) = 0;
	virtual bool IsWireForwarding (wire* wire, uint32_t vlanNumber, _Out_opt_ bool* hasLoop) const = 0;
	virtual void pause_simulation() = 0;
	virtual void resume_simulation() = 0;
	virtual bool simulation_paused() const = 0;
	virtual bool GetChangedFlag() const = 0;
	virtual void SetChangedFlag (bool projectChangedFlag) = 0;
	virtual ChangedFlagChangedEvent::subscriber GetChangedFlagChangedEvent() = 0;
	virtual ChangedEvent::subscriber GetChangedEvent() = 0;
	virtual const object_collection_property* bridges_prop() const = 0;
	virtual const object_collection_property* wires_prop() const = 0;
	virtual property_changing_e::subscriber property_changing() = 0;
	virtual property_changed_e::subscriber property_changed() = 0;

	std::pair<wire*, size_t> GetWireConnectedToPort (const port* port) const;
	port* FindConnectedPort (port* txPort) const;
	std::unique_ptr<wire> remove_wire (wire* w);
	std::unique_ptr<bridge> remove_bridge (bridge* b);
	port* port_at (connected_wire_end end) const;
};
using project_factory_t = std::shared_ptr<project_i>(*const)();

// ============================================================================

struct __declspec(novtable) vlan_window_i : virtual edge::win32_window_i
{
	virtual SIZE preferred_size() const = 0;
};
using vlan_window_factory_t = std::unique_ptr<vlan_window_i>(*const)(
	simulator_app_i* app,
	project_window_i* pw,
	const std::shared_ptr<project_i>& project,
	selection_i* selection,
	HWND hWndParent,
	POINT location,
	ID3D11DeviceContext1* d3d_dc,
	IDWriteFactory* dwrite_factory);
extern const vlan_window_factory_t vlan_window_factory;

// ============================================================================

struct simulator_app_i
{
	struct project_window_added_e    : edge::event<project_window_added_e, project_window_i*> { };
	struct project_window_removing_e : edge::event<project_window_removing_e, project_window_i*> { };
	struct project_window_removed_e  : edge::event<project_window_removed_e, project_window_i*> { };

	virtual HINSTANCE GetHInstance() const = 0;
	virtual const wchar_t* GetRegKeyPath() const = 0;
	virtual void add_project_window (std::unique_ptr<project_window_i>&& pw) = 0;
	virtual const std::vector<std::unique_ptr<project_window_i>>& project_windows() const = 0;
	virtual const char* app_name() const = 0;
	virtual const wchar_t* app_namew() const = 0;
	virtual const char* app_version_string() const = 0;
	virtual project_window_added_e::subscriber project_window_added() = 0;
	virtual project_window_removing_e::subscriber project_window_removing() = 0;
	virtual project_window_removed_e::subscriber project_window_removed() = 0;
	virtual selection_factory_t selection_factory() const = 0;
	virtual edit_window_factory_t edit_window_factory() const = 0;
	virtual project_window_factory_t project_window_factory() const = 0;
	virtual project_factory_t project_factory() const = 0;
};

// ============================================================================
