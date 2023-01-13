
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once
#include "port.h"
#include "edge/xml_serializer.h"
#include "pg/include/pg/property_grid.h"
#include "edge/om/value_collection_property.h"

struct BridgeLogLine
{
	std::string text;
	int portIndex;
	int treeIndex;
};

extern const edge::nvp stp_version_nvps[];
extern const char stp_version_type_name[];
using stp_version_traits = edge::enum_property_traits<STP_VERSION, stp_version_type_name, stp_version_nvps>;
using stp_version_p = edge::static_value_property<stp_version_traits>;

struct mac_address_property_traits
{
	static const char type_name[];
	using value_t = mac_address;
	static void to_string (mac_address from, edge::out_sstream_i* to, const edge::string_convert_context_i*);
	static void from_string (std::string_view from, mac_address& to, const edge::string_convert_context_i*);
};
using mac_address_p = static_ui_prop<mac_address_property_traits>;

extern std::unique_ptr<pg::property_editor_i> create_config_id_editor (pg::object_list_i& objects);

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

	virtual std::unique_ptr<pg::property_editor_i> create_editor(pg::object_list_i& objects) const override
	{
		return create_config_id_editor(objects);
	}

	virtual bool ui_visible() const override { return true; }
};

struct project_i;
class bridge_tree;

class bridge : public renderable_object_i, public edge::custom_serialize_object_i
{
	edge::event_manager _em;
	project_i* _project = nullptr;
	float _x;
	float _y;
	float _width;
	float _height;
	std::vector<std::unique_ptr<port>> _ports;
	STP_BRIDGE* _stpBridge = nullptr;
	bool _bpdu_trapping_enabled = false;
	static const STP_CALLBACKS StpCallbacks;
	std::vector<std::unique_ptr<BridgeLogLine>> _logLines;
	BridgeLogLine _currentLogLine;
	std::queue<std::pair<size_t, packet_t>> _rxQueue;
	std::vector<std::unique_ptr<bridge_tree>> _trees;
	bool _deserializing = false;
	bool _enable_stp_after_deserialize;

	// Let's keep things simple and do everything on the GUI thread.
	static HINSTANCE _hinstance;
	static UINT_PTR _link_pulse_timer_id;
	static UINT_PTR _one_second_timer_id;
	static std::unordered_set<bridge*> _created_bridges;
	HWND _helper_window = nullptr;

	// variables used by TransmitGetBuffer/ReleaseBuffer
	std::vector<uint8_t> _txPacketData;
	port*                _txTransmittingPort;
	unsigned int         _txTimestamp;

public:
	bridge (size_t port_count, size_t msti_count, mac_address macAddress);
	virtual ~bridge();

	virtual edge::hierarchy_object_i* parent() const override;
	void set_parent (project_i* parent);
	project_i* project() const;

	edge::property_changing_e::subscriber property_changing() { return edge::property_changing_e::subscriber(_em); }
	edge::property_changed_e::subscriber property_changed() { return edge::property_changed_e::subscriber(_em); }

	static constexpr int HTCodeInner = 1;

	static constexpr float DefaultHeight = 100;
	static constexpr float OutlineWidth = 2;
	static constexpr float MinWidth = 180;
	static constexpr float RoundRadius = 8;

	float left() const { return _x; }
	float right() const { return _x + _width; }
	float top() const { return _y; }
	float bottom() const { return _y + _height; }
	D2D1_POINT_2F location() const { return { _x, _y }; }
	void set_location (float x, float y);
	void set_location (D2D1_POINT_2F location) { set_location (location.x, location.y); }
	D2D1_RECT_F bounds() const { return { _x, _y, _x + _width, _y + _height }; }

	void move_port (port* port, D2D1_POINT_2F proposedLocation);

	const std::vector<std::unique_ptr<bridge_tree>>& trees() const { return _trees; }
	const std::vector<std::unique_ptr<port>>& ports() const { return _ports; }

	void render (ID2D1RenderTarget* dc, const drawing_resources& dos, unsigned int vlanNumber, const D2D1_COLOR_F& configIdColor) const;

	struct invalidate_e : public edge::event<invalidate_e, bridge*> { };
	invalidate_e::subscriber invalidate() { return invalidate_e::subscriber(_em); }

	virtual void render_selection (const edge::zoomer* zoomer, const drawing_resources& dos) const override final;
	virtual ht_result hit_test (const D2D1::Matrix3x2F& wtr, D2D1_POINT_2F dLocation, float tolerance) override final;
	virtual D2D1_RECT_F extent() const override { return bounds(); }

	STP_BRIDGE* stp_bridge() const { return _stpBridge; }

	struct log_line_generated_e : public edge::event<log_line_generated_e, bridge*, const BridgeLogLine*> { };
	struct log_cleared_e : public edge::event<log_cleared_e, bridge*> { };
	struct packet_transmit_e : public edge::cancelable_event<packet_transmit_e, bool, bridge*, size_t, packet_t&&> { };

	log_line_generated_e::subscriber log_line_generated() { return log_line_generated_e::subscriber(_em); }
	log_cleared_e::subscriber log_cleared() { return log_cleared_e::subscriber(_em); }
	packet_transmit_e::subscriber packet_transmit() { return packet_transmit_e::subscriber(_em); }

	void enqueue_received_packet (packet_t&& packet, size_t rxPortIndex);

	const std::vector<std::unique_ptr<BridgeLogLine>>& GetLogLines() const { return _logLines; }
	void clear_log();
	std::array<uint8_t, 6> GetPortAddress (size_t portIndex) const;

	// Property getters and setters.
	mac_address bridge_address() const;
	void set_bridge_address (mac_address address);
	bool stp_enabled() const { return (bool) STP_IsBridgeStarted(_stpBridge); }
	void set_stp_enabled(bool enable);
	STP_VERSION stp_version() const { return STP_GetStpVersion(_stpBridge); }
	void set_stp_version(STP_VERSION version);
	size_t port_count() const { return STP_GetPortCount(_stpBridge); }
	size_t msti_count() const { return STP_GetMstiCount(_stpBridge); }
	std::string mst_config_id_name() const;
	void set_mst_config_id_name (std::string mst_config_id_name);
	uint32_t GetMstConfigIdRevLevel() const;
	void SetMstConfigIdRevLevel (uint32_t revLevel);
	std::string GetMstConfigIdDigest() const;
	void SetMstConfigTable (const STP_CONFIG_TABLE_ENTRY* entries, size_t entryCount);
	uint32_t bridge_max_age() const { return (uint32_t) STP_GetBridgeMaxAge(_stpBridge); }
	void set_bridge_max_age (uint32_t value);
	uint32_t bridge_forward_delay() const { return (uint32_t) STP_GetBridgeForwardDelay(_stpBridge); }
	void set_bridge_forward_delay (uint32_t value);
	uint32_t tx_hold_count() const { return STP_GetTxHoldCount(_stpBridge); }
	void set_tx_hold_count (uint32_t value);
private:
	static void on_port_invalidated (void* arg, port* p);
	void OnLinkPulseTick();
	void ProcessReceivedPackets();

	static void* StpCallback_AllocAndZeroMemory (unsigned int size);
	static void  StpCallback_FreeMemory (void* p);
	static void* StpCallback_TransmitGetBuffer        (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int bpduSize, unsigned int timestamp);
	static void  StpCallback_TransmitReleaseBuffer    (const STP_BRIDGE* bridge, void* bufferReturnedByGetBuffer);
	static void  StpCallback_EnableBpduTrapping       (const STP_BRIDGE* bridge, bool enable, unsigned int timestamp);
	static void  StpCallback_EnableLearning           (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, bool enable, unsigned int timestamp);
	static void  StpCallback_EnableForwarding         (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, bool enable, unsigned int timestamp);
	static void  StpCallback_FlushFdb                 (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, enum STP_FLUSH_FDB_TYPE flushType, unsigned int timestamp);
	static void  StpCallback_DebugStrOut              (const STP_BRIDGE* bridge, int portIndex, int treeIndex, const char* nullTerminatedString, unsigned int stringLength, unsigned int flush);
	static void  StpCallback_OnTopologyChange         (const STP_BRIDGE* bridge, unsigned int treeIndex, unsigned int timestamp);
	static void  StpCallback_OnPortRoleChanged        (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, STP_PORT_ROLE role, unsigned int timestamp);

	// custom_serialize_object_i
	virtual void sort_xml_properties (std::vector<const property*>& props) const override { }
	virtual void on_deserializing (edge::xml_deserializer_i* de) override;
	virtual void on_deserialized  (edge::xml_deserializer_i* de) override;

public:
	float x() const { return _x; }
	void set_x (float x);
	float y() const { return _y; }
	void set_y (float y);
	float width() const { return _width; }
	void set_width (float width);
	float height() const { return _height; }
	void set_height (float height);

private:
	size_t mst_config_table_get_value_count() const;
	uint32_t mst_config_table_get_value(size_t i) const;
	void mst_config_table_set_value(size_t i, uint32_t value);
	bool mst_config_table_changed(size_t i) const;

	uint32_t bridge_migrate_time() const { return migrate_time_property.default_value().value(); }
	uint32_t bridge_hello_time() const { return bridge_hello_time_property.default_value().value(); }
	uint32_t max_hops() const { return max_hops_property.default_value().value(); }

public:
	static const mac_address_p bridge_address_property;
	static const bool_p        stp_enabled_property;
	static const stp_version_p stp_version_property;
	static const size_p        port_count_property;
	static const size_p        msti_count_property;
	static const string_p      mst_config_id_name_property;
	static const edge::typed_value_collection_property<edge::uint32_property_traits> mst_config_table_property;
	static const uint32_p      mst_config_id_rev_level;
	static const config_id_digest_p  mst_config_id_digest;
	static const uint32_p      migrate_time_property;
	static const uint32_p      bridge_hello_time_property;
	static const uint32_p      bridge_max_age_property;
	static const uint32_p      bridge_forward_delay_property;
	static const uint32_p      tx_hold_count_property;
	static const uint32_p      max_hops_property;
	static const edge::static_value_property<edge::float_property_traits> x_property;
	static const edge::static_value_property<edge::float_property_traits> y_property;
	static const edge::static_value_property<edge::float_property_traits> width_property;
	static const edge::static_value_property<edge::float_property_traits> height_property;
	static const edge::typed_object_collection_property1<bridge_tree> trees_prop;
	static const edge::typed_object_collection_property1<port> ports_prop;

	static const property* const _properties[];
	static const xtype<bridge, edge::size_t_property_traits, edge::size_t_property_traits, mac_address_property_traits> _type;
	virtual const edge::concrete_type* type() const override { return &_type; }
};
