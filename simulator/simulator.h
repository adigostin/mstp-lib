
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once
#include "selectable_object.h"
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

static constexpr unsigned char DefaultConfigTableDigest[16] = { 0xAC, 0x36, 0x17, 0x7F, 0x50, 0x28, 0x3C, 0xD4, 0xB8, 0x38, 0x21, 0xD8, 0xAB, 0x26, 0xDE, 0x62 };

// Maximum VLAN number supported by the simulator (too large a number would complicate the UI).
// The maximum VLAN number allowed by specs is 4094.
static constexpr uint32_t max_vlan_number = 16;

static constexpr wchar_t FileExtensionWithoutDot[] = L"stp";
static constexpr wchar_t FileExtensionWithDot[] = L".stp";

static constexpr char app_version_string[] = "2.2";

// ============================================================================

struct __declspec(novtable) selection_i : pg::object_list_i
{
	virtual const std::vector<edge::object*>& objects() const = 0;
	virtual void select (edge::object* o) = 0;
	virtual void clear() = 0;
	virtual void add (edge::object* o) = 0;
	virtual void remove (edge::object* o) = 0;
};
using selection_factory_t = std::unique_ptr<selection_i>(project_i* project);

// ============================================================================

struct __declspec(novtable) log_window_i : edge::win32_window_i
{
};

std::unique_ptr<log_window_i> make_log_window (HWND hWndParent, const RECT& rect, 
	ID3D11DeviceContext1* d3d_dc, IDWriteFactory* dwrite_factory, ID2D1Factory1* d2d_factory,
	selection_i* selection, const std::shared_ptr<project_i>& project, edge::theme_color_provider_i* tcp);

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

struct __declspec(novtable) edit_window_i : edge::win32_window_i
{
	//virtual edge::win32_window_i* window() = 0;
	virtual edge::d2d_renderer_i* renderer() = 0;
	virtual edge::zoomer* zoomer() = 0;
	virtual const struct drawing_resources& drawing_resources() const = 0;
	virtual void EnterState (std::unique_ptr<edit_state>&& state) = 0;
	virtual port* GetCPAt (D2D1_POINT_2F dLocation, float tolerance) const = 0;
	virtual void RenderSnapRect (ID2D1DeviceContext* dc, D2D1_POINT_2F wLocation) const = 0;
	virtual void render_hint (ID2D1DeviceContext* dc,
							 D2D1_POINT_2F dLocation,
							 std::string_view text,
							 DWRITE_TEXT_ALIGNMENT ha,
							 DWRITE_PARAGRAPH_ALIGNMENT va,
							 bool smallFont = false) const = 0;
	virtual void zoom_all() = 0;
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
	ID2D1Factory1* d2d_factory;
};
using edit_window_factory_t = std::unique_ptr<edit_window_i>(const edit_window_create_params& cps);

// ============================================================================

struct properties_window_create_params
{
	HWND hwnd_parent;
	RECT rect;
	edge::theme_color_provider_i* tcp;
	ID3D11DeviceContext* d3d_dc;
	IDWriteFactory* dwrite_factory;
	ID2D1Factory1* d2d_factory;
};

struct __declspec(novtable) properties_window_i : edge::win32_window_i
{
	virtual pg::property_grid_i* pg() const = 0;
};

using properties_window_factory_t = std::unique_ptr<properties_window_i>(const properties_window_create_params& cps);

// ============================================================================

struct __declspec(novtable) project_window_i
{
	virtual ~project_window_i() = default;
	virtual HWND hwnd() const = 0;
	virtual const std::shared_ptr<project_i>& project() const = 0;
	virtual void select_vlan (uint32_t vlanNumber) = 0;
	virtual uint32_t selected_vlan_number() const = 0;
	struct selected_vlan_number_changed_e : public edge::event<selected_vlan_number_changed_e, project_window_i*, uint32_t> { };
	virtual selected_vlan_number_changed_e::subscriber selected_vlan_number_changed() = 0;
	struct closed_e : public edge::event<closed_e, project_window_i*> { };
	virtual closed_e::subscriber closed() = 0;
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
	ID2D1Factory1*        d2d_factory;
};

using project_window_factory_t = std::unique_ptr<project_window_i>(const project_window_create_params& create_params);

// ============================================================================

enum class save_project_option { save_unconditionally, save_if_changed_ask_user_first };

struct __declspec(novtable) project_i : edge::object, edge::notify_property_change, edge::hierarchy_root_i, edge::string_convert_context_i
{
	virtual ~project_i() = default;

	struct invalidate_e : public edge::event<invalidate_e, project_i*> { };
	struct loaded_e     : public edge::event<loaded_e, project_i*> { };
	struct saved_e      : public edge::event<saved_e, project_i*> { };
	struct changed_flag_changed_event : public edge::event<changed_flag_changed_event, project_i*> { };
	struct ChangedEvent : public edge::event<ChangedEvent, project_i*> { };

	//virtual object* as_object() = 0;
	virtual invalidate_e::subscriber invalidated() = 0;
	virtual loaded_e::subscriber loaded() = 0;
	virtual saved_e::subscriber saved() = 0;
	virtual mac_address alloc_mac_address_range (size_t count) = 0;
	virtual const std::wstring& file_path() const = 0;
	virtual void save (const wchar_t* path) = 0;
	virtual void load (const wchar_t* path) = 0;
	virtual bool IsWireForwarding (wire* wire, uint32_t vlanNumber, _Out_opt_ bool* hasLoop) const = 0;
	virtual void pause_simulation() = 0;
	virtual void resume_simulation() = 0;
	virtual bool simulation_paused() const = 0;
	virtual bool GetChangedFlag() const = 0;
	virtual void SetChangedFlag (bool projectChangedFlag) = 0;
	virtual changed_flag_changed_event::subscriber changed_flag_changed() = 0;
	virtual ChangedEvent::subscriber GetChangedEvent() = 0;
	virtual const edge::typed_object_collection_property1<bridge>* bridges_property() const = 0;
	virtual const edge::typed_object_collection_property1<wire>* wires_property() const = 0;
	virtual size_t bridge_count() const = 0;
	virtual bridge* bridge_at(size_t index) const = 0;
	virtual size_t wire_count() const = 0;
	virtual wire* wire_at(size_t index) const = 0;
	virtual const std::vector<std::unique_ptr<bridge>>& bridges() const = 0;
	virtual const std::vector<std::unique_ptr<wire>>& wires() const = 0;

	std::pair<wire*, size_t> GetWireConnectedToPort (const port* port) const;
	port* find_connected_port (port* txPort) const;
};
using project_factory_t = std::shared_ptr<project_i>();

// ============================================================================

struct __declspec(novtable) vlan_window_i : edge::win32_window_i
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
	IDWriteFactory* dwrite_factory,
	ID2D1Factory1* d2d_factory);
extern const vlan_window_factory_t vlan_window_factory;

// ============================================================================

struct __declspec(novtable) simulator_app_i : edge::theme_color_provider_i
{
	struct project_window_added_e    : edge::event<project_window_added_e, project_window_i*> { };
	struct project_window_removing_e : edge::event<project_window_removing_e, project_window_i*> { };
	struct project_window_removed_e  : edge::event<project_window_removed_e, project_window_i*> { };

	virtual const wchar_t* GetRegKeyPath() const = 0;
	virtual void add_project_window (std::unique_ptr<project_window_i>&& pw) = 0;
	virtual const std::vector<std::unique_ptr<project_window_i>>& project_windows() const = 0;
	virtual const char* app_name() const = 0;
	virtual const wchar_t* app_namew() const = 0;
	virtual const char* app_version_string() const = 0;
	virtual project_window_added_e::subscriber project_window_added() = 0;
	virtual project_window_removing_e::subscriber project_window_removing() = 0;
	virtual project_window_removed_e::subscriber project_window_removed() = 0;
	virtual selection_factory_t* selection_factory() const = 0;
	virtual edit_window_factory_t* edit_window_factory() const = 0;
	virtual project_window_factory_t* project_window_factory() const = 0;
	virtual project_factory_t* project_factory() const = 0;
	virtual properties_window_factory_t* properties_window_factory() const = 0;
};

// ============================================================================
