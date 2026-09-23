
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#pragma once
#include "stp.h"
#include "SimulatorIDL.h"
#include "edge/edge_d2d.h"
#include "pg/property_grid.h"

struct ISimulatorApp;
struct IProjectWindow;
struct IStpProject;
struct IBridge;
struct IPort;

static constexpr unsigned char DefaultConfigTableDigest[16] = { 0xAC, 0x36, 0x17, 0x7F, 0x50, 0x28, 0x3C, 0xD4, 0xB8, 0x38, 0x21, 0xD8, 0xAB, 0x26, 0xDE, 0x62 };

// Maximum VLAN number supported by the simulator (too large a number would complicate the UI).
// The maximum VLAN number allowed by specs is 4094.
static constexpr uint32_t max_vlan_number = 16;

static constexpr wchar_t FileExtensionWithoutDot[] = L"stp";
static constexpr wchar_t FileExtensionWithDot[] = L".stp";

static constexpr wchar_t app_version_string[] = L"2.4";

extern const char stp_disabled_text[];

// ============================================================================

struct drawing_resources
{
	com_ptr<IDWriteFactory> _dWriteFactory;
	com_ptr<ID2D1SolidColorBrush> _poweredFillBrush;
	com_ptr<ID2D1SolidColorBrush> _unpoweredBrush;
	com_ptr<ID2D1SolidColorBrush> _brushWindowText;
	com_ptr<ID2D1SolidColorBrush> _brushWindow;
	com_ptr<ID2D1SolidColorBrush> _brushHighlight;
	com_ptr<ID2D1SolidColorBrush> _brushDiscardingPort;
	com_ptr<ID2D1SolidColorBrush> _brushLearningPort;
	com_ptr<ID2D1SolidColorBrush> _brushForwarding;
	com_ptr<ID2D1SolidColorBrush> _brushNoForwardingWire;
	com_ptr<ID2D1SolidColorBrush> _brushLoop;
	com_ptr<ID2D1SolidColorBrush> _brushTempWire;
	com_ptr<ID2D1StrokeStyle> _strokeStyleForwardingWire;
	com_ptr<ID2D1StrokeStyle> _strokeStyleNoForwardingWire;
	com_ptr<IDWriteTextFormat> _regularTextFormat;
	com_ptr<IDWriteTextFormat> _smallTextFormat;
	com_ptr<IDWriteTextFormat> _smallBoldTextFormat;
	com_ptr<ID2D1StrokeStyle> _strokeStyleSelectionRect;
};

struct DECLSPEC_NOVTABLE DECLSPEC_UUID("9F626A71-4C9E-4F25-A821-FBBE33748563") ISelectableObject : IUnknown
{
	virtual void render_selection (ID2D1DeviceContext* dc, const edge::IZoomer* zoomer, const drawing_resources& dos) const = 0;
	virtual int32_t hit_test (const D2D1::Matrix3x2F& wtr, D2D1_POINT_2F dLocation, float tolerance) = 0;
	virtual RECT extent() const noexcept = 0;

	D2D1_RECT_F extentf() const noexcept 
	{
		auto r = extent();
		return { (float)r.left, (float)r.top, (float)r.right, (float)r.bottom };
	}
};

// ============================================================================

using loose_wire_end = POINT;
using connected_wire_end = IPort*;
using wire_end = std::variant<loose_wire_end, connected_wire_end>;

struct DECLSPEC_NOVTABLE DECLSPEC_UUID("5338CBD3-FF51-4DF0-AE6A-D854EC4748FB") IWire : ISelectableObject
{
	virtual IStpProject* parent() const = 0;
	virtual void set_parent (IStpProject* parent) = 0;

	virtual std::array<wire_end, 2>& points() = 0;
	const std::array<wire_end, 2>& points() const { return const_cast<IWire*>(this)->points(); }

	const wire_end& point (size_t i) const { return points()[i]; }
	wire_end& point (size_t i) { return points()[i]; }
	virtual void set_point (size_t i, wire_end point) = 0;

	wire_end p0() const { return points()[0]; }
	void set_p0 (wire_end p0) { set_point(0, p0); }
	void set_p0 (IPort* p0) { set_point(0, p0); }
	wire_end p1() const { return points()[1]; }
	void set_p1 (wire_end p1) { set_point(1, p1); }
	void set_p1 (IPort* p1) { set_point(1, p1); }

	virtual POINT point_coords (size_t pointIndex) const noexcept = 0;

	virtual void render (ID2D1RenderTarget* rt, const drawing_resources& dos, bool forwarding, bool isPartOfLoop) const = 0;
};

HRESULT MakeWire (IWire** ppWire);

// ============================================================================

struct DECLSPEC_NOVTABLE DECLSPEC_UUID("3CB4F8D1-F2A0-47A2-9002-22301E26E315") IPortTree : IUnknown
{
	virtual IPort* port() const = 0;

	virtual void flush_fdb (unsigned int timestamp) = 0;

	virtual bool fdb_flush_text_visible() const = 0;

	virtual size_t tree_index() const = 0;

	//static const size_p tree_index_property;
	//static const bool_p learning_property;
	//static const bool_p forwarding_property;
	//static const port_role_p role_property;
	//static const uint32_p admin_internal_port_path_cost_property;
	//static const uint32_p internal_port_path_cost_property;
	//static const property* const _properties[];
	//static const xtype<IPortTree> _type;
	//virtual const edge::concrete_type* type() const override final;
};

HRESULT MakePortTree (IPort* port, uint32_t treeIndex, IPortTree** ppPortTree);

// ============================================================================

static constexpr LONG PortInteriorWidth = 30;
static constexpr LONG PortInteriorDepth = 16;
static constexpr LONG PortToPortSpacing = 16;
static constexpr LONG PortExteriorWidth = 10;
static constexpr LONG PortExteriorHeight = 20;
static constexpr LONG PortOutlineWidth = 2;

struct DECLSPEC_NOVTABLE DECLSPEC_UUID("0BED0316-A211-4A8D-9BC5-3CF548E4AFD5") IPort : ISelectableObject
{
	static constexpr uint8_t HTCodeInnerOuter = 1;
	static constexpr uint8_t HTCodeCP = 2;

	virtual IBridge* bridge() const = 0;
	virtual uint32_t port_index() const = 0;
	virtual POINT GetCPLocation() const = 0;
	virtual bool mac_operational() const = 0;
	virtual bool IsForwarding (unsigned int vlanNumber) const = 0;
	virtual void Move (POINT proposedLocation) = 0;
	virtual uint32_t treeCount() const = 0;
	virtual IPortTree* treeAt(uint32_t i) = 0;
	virtual HRESULT STDMETHODCALLTYPE Render (ID2D1RenderTarget* dc, const drawing_resources& dos, unsigned int vlanNumber) const noexcept = 0;
	virtual uint32_t SupportedSpeed() const = 0;
	virtual void SetActualSpeed (uint32_t value) = 0;
};

HRESULT MakePort (IBridge* parent, uint32_t portIndex, PortSide side, LONG offset, IPort** ppPort);

// ============================================================================

struct DECLSPEC_NOVTABLE DECLSPEC_UUID("A77C1690-ADA8-4296-8757-A4D9B3D0C603") IBridgeTree : IUnknown
{
	virtual void on_topology_change (unsigned int timestamp) = 0;
	virtual std::string root_bridge_id() const = 0;
};

HRESULT MakeBridgeTree (IBridge* bridge, uint32_t treeIndex, IBridgeTree** ppBridgeTree);

// ============================================================================

struct BridgeLogLine
{
	std::string text;
	int portIndex;
	int treeIndex;
};

using mac_address = std::array<uint8_t, 6>;

struct frame_t
{
	uint32_t timestamp;
	std::vector<uint8_t> data;;
	std::vector<mac_address> tx_path_taken;
};

struct link_pulse_t
{
	uint32_t timestamp;
	uint32_t sender_supported_speed;
};

using packet_t = std::variant<link_pulse_t, frame_t>;

// TODO: decouple the high-frequency OnPacketTransmit from the low-frequency OnLogLineGenerated/OnLogCleared.
struct DECLSPEC_NOVTABLE DECLSPEC_UUID("BA7597FD-AF3D-4995-9ABE-092201884C1D") IBridgeEvents : IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE OnLogLineGenerated (IBridge*, const BridgeLogLine*) = 0;
	virtual HRESULT STDMETHODCALLTYPE OnLogCleared (IBridge*) = 0;

	// The packet is meant to be moved-from by a single sink. The application ensures this.
	virtual HRESULT STDMETHODCALLTYPE OnPacketTransmit (IBridge* sender, ULONG txPortIndex, packet_t&& packet) = 0;
};
/*
extern std::unique_ptr<pg::property_editor_i> create_config_id_editor (pg::IObjectList& objects);

struct config_id_digest_p : edge::static_value_property<edge::temp_string_property_traits>, pg::ui_property_i, pg::pg_custom_editor_i
{
	using base = edge::static_value_property<edge::temp_string_property_traits>;

	const pg::property_group* const _group;
	const char* const _description;

	config_id_digest_p (const char* name, const pg::property_group* group, const char* description, getter_t getter, setter_t setter, std::optional<value_t> default_value = std::nullopt)
		: base (name, getter, setter, std::move(default_value))
		, _group(group)
		, _description(description)
	{ }

	virtual const char* description() const override { return _description; }

	virtual const pg::property_group* group() const override { return _group; }

	virtual std::unique_ptr<pg::property_editor_i> create_editor(pg::IObjectList& objects) const override
	{
		return create_config_id_editor(objects);
	}

	virtual bool ui_visible() const override { return true; }
};
*/
struct DECLSPEC_NOVTABLE DECLSPEC_UUID("11BD73E6-2E25-4E85-A71D-EDB075244751") IStpPropertyChangeSink : IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE OnStpPropertyChanging(IBridge*, unsigned int portIndex, unsigned int treeIndex, STP_PROPERTY prop, unsigned int timestamp) noexcept = 0;
	virtual HRESULT STDMETHODCALLTYPE OnStpPropertyChanged (IBridge*, unsigned int portIndex, unsigned int treeIndex, STP_PROPERTY prop, unsigned int timestamp) noexcept = 0;
};

struct IStpProject;
struct IBridgeTree;

struct DECLSPEC_NOVTABLE DECLSPEC_UUID("CDE56C38-78B7-4835-827F-DA9D3E54B032") IBridge : ISelectableObject
{
	virtual IStpProject* parent() const = 0;;
	virtual void set_parent (IStpProject* parent) = 0;

	//edge::property_changing_e::subscriber property_changing() { return edge::property_changing_e::subscriber(_em); }
	//edge::property_changed_e::subscriber property_changed() { return edge::property_changed_e::subscriber(_em); }

	static constexpr uint8_t HTCodeInner = 1;

	static constexpr LONG DefaultHeight = 100;
	static constexpr float OutlineWidth = 2;
	static constexpr LONG MinWidth = 180;
	static constexpr float RoundRadius = 8;

	virtual POINT location() const noexcept = 0;
	virtual void set_location (POINT) noexcept = 0;
	virtual SIZE size() const noexcept = 0;
	LONG left() const { return location().x; }
	LONG right() const { return location().x + size().cx; }
	LONG top() const { return location().y; }
	LONG bottom() const { return location().y + size().cy; }
	LONG x() const { return location().x; }
	LONG y() const { return location().y; }
	LONG width() const { return size().cx; }
	LONG height() const { return size().cy; }

	virtual ULONG TreeCount() const = 0;
	virtual IBridgeTree* TreeAt(ULONG i) const = 0;

	virtual ULONG PortCount() const = 0;
	virtual IPort* PortAt (ULONG i) const = 0;

	virtual HRESULT STDMETHODCALLTYPE Render (ID2D1RenderTarget* dc, const drawing_resources& dos, unsigned int vlanNumber, const D2D1_COLOR_F& configIdColor) const = 0;

	virtual STP_BRIDGE* stp_bridge() const = 0;

	virtual void enqueue_received_packet (packet_t&& packet, ULONG rxPortIndex);

	virtual const std::vector<std::unique_ptr<BridgeLogLine>>& GetLogLines() const = 0;
	virtual void clear_log() = 0;
	std::array<uint8_t, 6> GetPortAddress (size_t portIndex) const;

	uint32_t msti_count() const { return STP_GetMstiCount(stp_bridge()); }
};

HRESULT MakeBridge (uint32_t portCount, uint32_t mstiCount, mac_address macAddress, IBridge** ppBridge);

// ============================================================================

struct DECLSPEC_NOVTABLE DECLSPEC_UUID("251AC28E-9F19-4306-8217-5BBC3345F1E8") ISelection : edge::IObjectList
{
	virtual HRESULT STDMETHODCALLTYPE Select (IDispatch* o) noexcept = 0;
	virtual HRESULT STDMETHODCALLTYPE Clear() noexcept = 0;
	virtual HRESULT STDMETHODCALLTYPE Add (IDispatch* o) noexcept = 0;
	virtual HRESULT STDMETHODCALLTYPE Remove (IDispatch* o) noexcept = 0;
};
using selection_factory_t = HRESULT(IStpProject* project, ISelection** ppSelection);

// ============================================================================

struct DECLSPEC_NOVTABLE DECLSPEC_UUID("DBFEA9F6-363D-4E85-9707-F8CF20B4570D") IVlanSelectionEvents : IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE OnVlanSelectionChanging (DWORD dwOld) = 0;
	virtual HRESULT STDMETHODCALLTYPE OnVlanSelectionChanged (DWORD dwNew) = 0;
};

struct DECLSPEC_NOVTABLE DECLSPEC_UUID("A4D5FA98-518D-4137-A7DD-7B6A1215AECC") IVlanSelection : IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE SelectVlan (DWORD dwVlan) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetSelectedVlan (DWORD* pdwVlan) = 0;
};

// ============================================================================

struct DECLSPEC_NOVTABLE DECLSPEC_UUID("538AC2A8-D4CA-4C37-8BAC-13C458E88ED2") ILogWindow : IUnknown
{
	virtual HWND hwnd() const = 0;
};

HRESULT MakeLogWindow (ISimulatorApp* app, HWND hWndParent, const RECT& rect,
					   ISelection* selection, IStpProject* project, ILogWindow** ppLogWindow);

// ============================================================================

class edit_state;

static constexpr float SnapDistance = 4;

struct DialogProcResult
{
	INT_PTR dialogProcResult;
	LRESULT messageResult;
};

struct mouse_location
{
	POINT pt;
	D2D1_POINT_2F d;
	POINT w;
};

struct DECLSPEC_NOVTABLE DECLSPEC_UUID("ECDD431B-40A0-4676-B006-8C0110CC678C") IEditWindow : IUnknown
{
	virtual HWND hWnd() const = 0;
	virtual edge::ID2DRenderer* renderer() = 0;
	virtual edge::IZoomer* zoomer() = 0;
	virtual const struct drawing_resources& drawing_resources() const = 0;
	virtual void EnterState (std::unique_ptr<edit_state>&& state) = 0;
	virtual IPort* GetCPAt (D2D1_POINT_2F dLocation, float tolerance) const = 0;
	virtual void RenderSnapRect (ID2D1DeviceContext* dc, POINT wLocation) const = 0;
	virtual HRESULT STDMETHODCALLTYPE RenderHint (ID2D1DeviceContext* dc, D2D1_POINT_2F dLocation, const wchar_t* text,
		DWRITE_TEXT_ALIGNMENT ha, DWRITE_PARAGRAPH_ALIGNMENT va, bool smallFont) const noexcept  = 0;
	virtual void zoom_all() = 0;
};

struct EditWindowCreateParams
{
	ISimulatorApp* app;
	IProjectWindow* pw;
	IStpProject* project;
	ISelection* selection;
	HWND hWndParent;
	RECT rect;
};
using edit_window_factory_t = HRESULT(const EditWindowCreateParams& cps, IEditWindow** ppEditWindow);

// ============================================================================

struct properties_window_create_params
{
	ISimulatorApp* app;
	HWND hwnd_parent;
	RECT rect;
	IVlanSelection* vlanSel;
	edge::IObjectList* selection;
};

struct DECLSPEC_NOVTABLE DECLSPEC_UUID("3CD3F193-C3AC-4472-A99D-CA15929898EE") IPropertiesWindow : IUnknown
{
	virtual HWND hWnd() const = 0;
	virtual pg::IPropertyGrid* pg() const = 0;
};
HRESULT MakePropertiesWindow (const properties_window_create_params& cps, IPropertiesWindow** ppPW);
using properties_window_factory_t = decltype(MakePropertiesWindow);

// ============================================================================

struct DECLSPEC_NOVTABLE DECLSPEC_UUID("8D344BD1-7493-43AC-8A46-4EDF0C960DAB") IProjectWindowEventsSink : IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE OnProjectWindowClosed (IProjectWindow* pw) = 0;
};

struct DECLSPEC_NOVTABLE DECLSPEC_UUID("31165615-DAC0-427D-BD00-8E79217AC39B") IProjectWindow : IUnknown
{
	virtual HWND hwnd() const = 0;
	virtual IStpProject* project() const = 0;
	virtual HRESULT STDMETHODCALLTYPE GetSelection (ISelection** ppSelection) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetVlanSelection (IVlanSelection** ppVlanSelection) = 0;
};

struct project_window_create_params
{
	ISimulatorApp* app;
	IStpProject*       project;
	bool     show_property_grid;
	bool     showLogWindow;
	uint32_t selectedVlan;
	int      nCmdShow;
};

using project_window_factory_t = HRESULT(const project_window_create_params& create_params, IProjectWindow** ppProjectWindow);

// ============================================================================

struct DECLSPEC_NOVTABLE DECLSPEC_UUID("B343DD08-210D-44E3-A759-4D3EB8918C3E") IProjectEventsSink : IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE OnProjectLoaded (IStpProject*) = 0;
	virtual HRESULT STDMETHODCALLTYPE OnProjectSaved (IStpProject*) = 0;
	virtual HRESULT STDMETHODCALLTYPE OnProjectChangedFlagChanged (IStpProject*) = 0;
};

struct DECLSPEC_NOVTABLE DECLSPEC_UUID("D1B260C3-2943-49E8-B575-C614857FC163") IStpProject : IUnknown, edge::string_convert_context_i
{
	virtual HRESULT STDMETHODCALLTYPE AllocMACAddressRange (size_t count, mac_address& addressOut) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetFilePath(BSTR* pbstrFilePath) = 0;
	virtual HRESULT STDMETHODCALLTYPE Save (const wchar_t* path) = 0;
	virtual HRESULT STDMETHODCALLTYPE Load (const wchar_t* path) = 0;
	virtual void pause_simulation() = 0;
	virtual void resume_simulation() = 0;
	virtual bool simulation_paused() const = 0;
	virtual bool GetChangedFlag() const = 0;
	virtual void SetChangedFlag (bool projectChangedFlag) = 0;

	virtual HRESULT InsertBridge (ULONG index, IBridge* bridge) = 0;
	HRESULT AddBridge (IBridge* bridge) { return InsertBridge(BridgeCount(), bridge); }
	virtual HRESULT RemoveBridge (ULONG index, _Outptr_opt_ IBridge** ppRemoved = nullptr) = 0;
	virtual ULONG STDMETHODCALLTYPE BridgeCount() const noexcept = 0;
	virtual IBridge* STDMETHODCALLTYPE BridgeAt(ULONG index) const noexcept = 0;

	virtual HRESULT InsertWire (ULONG index, com_ptr<IWire> wire) = 0;
	HRESULT AddWire (com_ptr<IWire> wire) { return InsertWire(WireCount(), std::move(wire)); }
	virtual HRESULT RemoveWire (ULONG index, _Outptr_opt_ IWire** ppRemoved = nullptr) = 0;
	virtual ULONG STDMETHODCALLTYPE WireCount() const noexcept = 0;
	virtual IWire* STDMETHODCALLTYPE WireAt(ULONG index) const noexcept = 0;
	virtual HRESULT STDMETHODCALLTYPE DeleteObjects (edge::IObjectList* objects) = 0;

	std::pair<IWire*, size_t> GetWireConnectedToPort (IPort* port) const;
	IPort* find_connected_port (IPort* txPort) const;
	bool IsWireForwarding (IWire* wire, uint32_t vlanNumber, _Out_opt_ bool* isPartOfLoop) const;
};
using project_factory_t = HRESULT(IStpProject**);
HRESULT MakeProject (IStpProject** ppProject);

// ============================================================================

struct DECLSPEC_NOVTABLE DECLSPEC_UUID("E5F597A4-ACA4-4BEE-B60B-235D56BC597E") IVlanWindow : IUnknown
{
	virtual HWND HWnd() const = 0;
	virtual SIZE PreferredSize() const = 0;
	virtual HRESULT STDMETHODCALLTYPE GetVlanSelection (IVlanSelection** ppVlanSelection) = 0;
};
using vlan_window_factory_t = HRESULT (ISimulatorApp* app, IProjectWindow* pw, IStpProject* project,
									   ISelection* selection, DWORD vlan, HWND hWndParent, POINT location, IVlanWindow** ppVlanWindow);

// ============================================================================

struct DECLSPEC_NOVTABLE DECLSPEC_UUID("70A3136A-9E07-457D-B8D1-FAE468D5A0B2") IProjectWindowCollectionEventsSink : IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE OnProjectWindowInserting (IProjectWindow*) = 0;
	virtual HRESULT STDMETHODCALLTYPE OnProjectWindowInserted (IProjectWindow*) = 0;
	virtual HRESULT STDMETHODCALLTYPE OnProjectWindowRemoving (IProjectWindow*) = 0;
	virtual HRESULT STDMETHODCALLTYPE OnProjectWindowRemoved (IProjectWindow*) = 0;
};

struct VlanWindowCreateParams
{
	ISimulatorApp* app;
	IProjectWindow* pw;
	IStpProject* project;
	ISelection* selection;
	DWORD vlan;
	HWND hWndParent;
	POINT location;
};

struct DECLSPEC_NOVTABLE DECLSPEC_UUID("B11C7D2D-9AF8-4450-A8ED-A955B5C52D10") ISimulatorApp : IUnknown
{
	virtual const wchar_t* GetRegKeyPath() const = 0;
	virtual HRESULT STDMETHODCALLTYPE AddProjectWindow (IProjectWindow* pw) = 0;
	virtual HRESULT STDMETHODCALLTYPE OpenWindowForVlan (IStpProject* project, DWORD vlanNumber, _Out_opt_ HWND* phWnd) = 0;
	virtual ULONG STDMETHODCALLTYPE ProjectWindowCount() const = 0;
	virtual IProjectWindow* STDMETHODCALLTYPE ProjectWindowAt (ULONG i) const = 0;
	virtual const wchar_t* app_name() const = 0;
	virtual const wchar_t* app_version_string() const = 0;
	virtual selection_factory_t* selection_factory() const = 0;
	virtual edit_window_factory_t* edit_window_factory() const = 0;
	virtual project_window_factory_t* project_window_factory() const = 0;
	virtual project_factory_t* project_factory() const = 0;
	virtual properties_window_factory_t* properties_window_factory() const = 0;
	virtual HRESULT STDMETHODCALLTYPE CreateVlanWindow (const VlanWindowCreateParams* params, IVlanWindow** ppVlanWindow) = 0;
	virtual edge::IThemeColorProvider* GetThemeColorProvider() const = 0;
	virtual ID3D11DeviceContext1* GetD3DDC() = 0;
	virtual IDWriteFactory* GetDWriteFactory() = 0;
	virtual ID2D1Factory1* GetD2DFactory() = 0;
};

struct DECLSPEC_NOVTABLE DECLSPEC_UUID("1BC286EB-8EDB-4F7E-BBC8-BB15DB4EE18C") IGetWrappedObject : IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE GetWrappedObject (REFIID riid, void** ppvObject) = 0;
};

// ============================================================================

struct DECLSPEC_NOVTABLE DECLSPEC_UUID("4382144E-D9BF-4F01-963E-AEFD17640492") IInvalidateSink : IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE OnInvalidate (const RECT* rectw) noexcept = 0;
	HRESULT STDMETHODCALLTYPE OnInvalidate(const RECT& rectw) { return OnInvalidate(&rectw); }
};

inline HRESULT NotifyPropertyChanging (ConnectionPointImpl<IPropertyChangeSink>* cp,
									   IUnknown* obj,
									   DISPID dispid,
									   const PropertyChangeArgs* args = nullptr)
{
	return cp->Notify([obj,args,dispid](IPropertyChangeSink* sink)
					  {
						  return sink->OnPropertyChanging(obj, dispid, args);
					  });
}


inline HRESULT NotifyPropertyChanged (ConnectionPointImpl<IPropertyChangeSink>* cp,
									  IUnknown* obj,
									  DISPID dispid,
									  const PropertyChangeArgs* args = nullptr)
{
	return cp->Notify([obj,args,dispid](IPropertyChangeSink* sink)
					  {
						  return sink->OnPropertyChanged(obj, dispid, args);
					  });
}

inline HRESULT NotifyInvalidate (ConnectionPointImpl<IInvalidateSink>* cp, const RECT& rectw)
{
	return cp->Notify([&rectw](IInvalidateSink* sink)
		{
			return sink->OnInvalidate(rectw);
		});
}

inline HRESULT NotifyInvalidate (ConnectionPointImpl<IInvalidateSink>* cp, const RECT* rectw)
{
	return cp->Notify([rectw](IInvalidateSink* sink)
		{
			return sink->OnInvalidate(rectw);
		});
}

// Meant for Insert/Remove/Replace collection property change notifications, multiple selected objects.
// The caller is responsible for initializing the array of child objects.
inline PropertyChangeArgs MakeObjectCollectionPropertyChangeArgs (
	CollectionChangeType type, 
	DWORD index, 
	DWORD count,
	IDispatch* const* childObjs)
{
	PropertyChangeArgs args;
	args.propertyType = PropertyType::Collection;
	args.collectionChangeArgs.changeType = type;
	args.collectionChangeArgs.setInsertRemoveArgs.childObjs = childObjs;
	args.collectionChangeArgs.setInsertRemoveArgs.index = index;
	args.collectionChangeArgs.setInsertRemoveArgs.count = count;
	return args;
}

// When vlanSel is null, the returned list exposes CIST trees only.
HRESULT MakeTreeSelection (edge::IObjectList* selection, IVlanSelection* vlanSel, edge::IObjectList** ppTreeSelection);

HRESULT BridgeAddressToString (const mac_address& addr, BSTR* pbstrBridgeAddress);
HRESULT BridgeAddressFromString (const wchar_t* pszBridgeAddress, mac_address& addr);

template<typename GetAt> requires std::is_invocable_r_v<HRESULT, GetAt, ULONG, IDispatch**>
HRESULT GetItems (ULONG cItems, GetAt getAt, SAFEARRAY** ppsaItems)
{
	auto sa = unique_safearray(SafeArrayCreateVector(VT_DISPATCH, 0, cItems)); RETURN_HR_IF(E_OUTOFMEMORY, !sa);
	for (LONG i = 0; i < (LONG)cItems; i++)
	{
		com_ptr<IDispatch> pDisp;
		auto hr = getAt((ULONG)i, &pDisp); RETURN_IF_FAILED(hr);
		hr = SafeArrayPutElement(sa.get(), &i, pDisp.get()); RETURN_IF_FAILED(hr);
	}

	*ppsaItems = sa.release();
	return S_OK;
}

template<typename TItem, typename Inserter> requires std::is_invocable_r_v<HRESULT, Inserter, com_ptr<TItem>&&>
HRESULT PutItems (SAFEARRAY* psaItems, Inserter inserter)
{
	VARTYPE vt;
	auto hr = SafeArrayGetVartype(psaItems, &vt); RETURN_IF_FAILED(hr);
	RETURN_HR_IF(E_NOTIMPL, vt != VT_DISPATCH);
	UINT dim = SafeArrayGetDim(psaItems);
	RETURN_HR_IF(E_NOTIMPL, dim != 1);
	LONG lbound;
	hr = SafeArrayGetLBound(psaItems, 1, &lbound); RETURN_IF_FAILED(hr);
	RETURN_HR_IF(E_NOTIMPL, lbound != 0);
	LONG ubound;
	hr = SafeArrayGetUBound(psaItems, 1, &ubound); RETURN_IF_FAILED(hr);

	vector_nothrow<com_ptr<TItem>> items;
	bool resized = items.try_resize((size_t)(ubound - lbound + 1)); RETURN_HR_IF(E_OUTOFMEMORY, !resized);

	for (LONG i = 0; i <= ubound; i++)
	{
		com_ptr<IDispatch> child;
		hr = SafeArrayGetElement (psaItems, &i, child.addressof()); RETURN_IF_FAILED(hr);
     hr = child->QueryInterface(IID_PPV_ARGS(&items[(size_t)(i - lbound)])); RETURN_IF_FAILED(hr);
	}

	for (auto& item : items)
	{
		hr = inserter(std::move(item)); RETURN_IF_FAILED(hr);
	}

	return S_OK;
}

void RenderExteriorNonStpPort (ID2D1RenderTarget* dc, const drawing_resources& dos, bool macOperational);
void RenderExteriorStpPort (ID2D1RenderTarget* dc, const drawing_resources& dos, STP_PORT_ROLE role, bool learning, bool forwarding, bool operEdge);

HRESULT SetErrorInfo (HRESULT return_hr, const wchar_t* text);
HRESULT SetErrorInfoStpDisabled();

HRESULT CreateMSTConfigIdEditor (edge::IObjectList* objs, pg::ICustomPropertyEditor** ppEditor);

inline D2D1_POINT_2F point_to_pointf (POINT p) noexcept
{
	return D2D1_POINT_2F { (float)p.x, (float)p.y };
}

inline POINT pointf_to_point (D2D1_POINT_2F p) noexcept
{
	return { (LONG)std::roundf(p.x), (LONG)std::roundf(p.y) };
}
