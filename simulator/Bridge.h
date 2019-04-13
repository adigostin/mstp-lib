
#pragma once
#include "BridgeTree.h"
#include "Port.h"

struct BridgeLogLine
{
	std::string text;
	int portIndex;
	int treeIndex;
};

static constexpr edge::NVP stp_version_nvps[] =  {
	{ "LegacySTP", STP_VERSION_LEGACY_STP },
	{ "RSTP", STP_VERSION_RSTP },
	{ "MSTP", STP_VERSION_MSTP },
	{ nullptr, 0 }
};
static constexpr char stp_version_type_name[] = "stp_version";
using stp_version_p = edge::enum_property<STP_VERSION, stp_version_type_name, stp_version_nvps>;

using mac_address = std::array<uint8_t, 6>;
std::string mac_address_to_string (mac_address from);
template<typename char_type> bool mac_address_from_string (std::basic_string_view<char_type> from, mac_address& to);
struct mac_address_property_traits
{
	static constexpr char type_name[] = "mac_address";
	using value_t = mac_address;
	using param_t = mac_address;
	using return_t = mac_address;
	static std::string to_string (mac_address from) { return mac_address_to_string(from); }
	static bool from_string (std::string_view from, mac_address& to) { return mac_address_from_string(from, to); }
};
using mac_address_p = edge::typed_property<mac_address_property_traits>;

extern edge::property_editor_factory_t config_id_editor_factory;
using config_id_digest_p = edge::typed_property<edge::temp_string_property_traits, nullptr, config_id_editor_factory>;

using edge::object_collection_property;
using edge::typed_object_collection_property;

struct project_i;

class Bridge : public project_child
{
	using base = project_child;

	float _x;
	float _y;
	float _width;
	float _height;
	std::vector<std::unique_ptr<Port>> _ports;
	bool _powered = true;
	STP_BRIDGE* _stpBridge = nullptr;
	bool _bpdu_trapping_enabled = false;
	static const STP_CALLBACKS StpCallbacks;
	std::vector<std::unique_ptr<BridgeLogLine>> _logLines;
	BridgeLogLine _currentLogLine;
	HANDLE _oneSecondTimerHandle;
	std::queue<std::pair<size_t, PacketInfo>> _rxQueue;
	std::vector<std::unique_ptr<BridgeTree>> _trees;

	// Let's keep things simple and do everything on the GUI thread.
	struct HelperWindow : edge::event_manager
	{
		HWND _hwnd;
		HANDLE _linkPulseTimerHandle;

		HelperWindow();
		~HelperWindow();

		static LRESULT CALLBACK SubclassProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

		struct LinkPulseEvent : public edge::event<LinkPulseEvent> { };
		LinkPulseEvent::subscriber GetLinkPulseEvent() { return LinkPulseEvent::subscriber(this); }
	};

	std::unique_ptr<HelperWindow> _helper_window = nullptr;

	// variables used by TransmitGetBuffer/ReleaseBuffer
	std::vector<uint8_t> _txPacketData;
	Port*                _txTransmittingPort;
	unsigned int         _txTimestamp;

public:
	Bridge (uint32_t portCount, uint32_t mstiCount, mac_address macAddress);
	virtual ~Bridge();

	static constexpr int HTCodeInner = 1;

	static constexpr float DefaultHeight = 100;
	static constexpr float OutlineWidth = 2;
	static constexpr float MinWidth = 180;
	static constexpr float RoundRadius = 8;

	float GetLeft() const { return _x; }
	float GetRight() const { return _x + _width; }
	float GetTop() const { return _y; }
	float GetBottom() const { return _y + _height; }
	float GetWidth() const { return _width; }
	float GetHeight() const { return _height; }
	D2D1_POINT_2F GetLocation() const { return { _x, _y }; }
	void SetLocation (float x, float y);
	void SetLocation (D2D1_POINT_2F location) { SetLocation (location.x, location.y); }
	D2D1_RECT_F GetBounds() const { return { _x, _y, _x + _width, _y + _height }; }

	void SetCoordsForInteriorPort (Port* port, D2D1_POINT_2F proposedLocation);

	const std::vector<std::unique_ptr<BridgeTree>>& trees() const { return _trees; }
	const std::vector<std::unique_ptr<Port>>& GetPorts() const { return _ports; }

	void Render (ID2D1RenderTarget* dc, const drawing_resources& dos, unsigned int vlanNumber, const D2D1_COLOR_F& configIdColor) const;

	virtual void render_selection (const edge::zoomable_i* zoomable, ID2D1RenderTarget* rt, const drawing_resources& dos) const override final;
	virtual HTResult hit_test (const edge::zoomable_i* zoomable, D2D1_POINT_2F dLocation, float tolerance) override final;

	STP_BRIDGE* stp_bridge() const { return _stpBridge; }

	struct LogLineGenerated : public edge::event<LogLineGenerated, Bridge*, const BridgeLogLine*> { };
	struct log_cleared_e : public edge::event<log_cleared_e, Bridge*> { };
	struct LinkPulseEvent : public edge::event<LinkPulseEvent, Bridge*, size_t, unsigned int> { };
	struct PacketTransmitEvent : public edge::event<PacketTransmitEvent, Bridge*, size_t, PacketInfo&&> { };

	LogLineGenerated::subscriber GetLogLineGeneratedEvent() { return LogLineGenerated::subscriber(this); }
	log_cleared_e::subscriber log_cleared() { return log_cleared_e::subscriber(this); }
	LinkPulseEvent::subscriber GetLinkPulseEvent() { return LinkPulseEvent::subscriber(this); }
	PacketTransmitEvent::subscriber GetPacketTransmitEvent() { return PacketTransmitEvent::subscriber(this); }

	void ProcessLinkPulse (size_t rxPortIndex, unsigned int timestamp);
	void EnqueuePacket (PacketInfo&& packet, size_t rxPortIndex);

	bool IsPowered() const { return _powered; }
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
	uint32_t port_count() const { return STP_GetPortCount(_stpBridge); }
	uint32_t msti_count() const { return STP_GetMstiCount(_stpBridge); }
	std::string mst_config_id_name() const;
	void set_mst_config_id_name (std::string_view mst_config_id_name);
	uint32_t GetMstConfigIdRevLevel() const;
	void SetMstConfigIdRevLevel (uint32_t revLevel);
	std::string GetMstConfigIdDigest() const;
	void SetMstConfigTable (const STP_CONFIG_TABLE_ENTRY* entries, size_t entryCount);
	uint32_t GetBridgeMaxAge() const;
	void SetBridgeMaxAge (uint32_t maxAge);
	uint32_t GetBridgeForwardDelay() const;
	void SetBridgeForwardDelay (uint32_t forwardDelay);

private:
	virtual void on_added_to_project(project_i* project) override;
	virtual void on_removing_from_project(project_i* project) override;

	static void CALLBACK OneSecondTimerCallback (void* lpParameter, BOOLEAN TimerOrWaitFired);
	static void OnWmOneSecondTimer (WPARAM wParam, LPARAM lParam);
	static void OnPortInvalidate (void* callbackArg, renderable_object* object);
	static void OnLinkPulseTick(void* callbackArg);
	static void OnWmPacketReceived (WPARAM wParam, LPARAM lParam);
	void ProcessReceivedPackets();

	static void* StpCallback_AllocAndZeroMemory (unsigned int size);
	static void  StpCallback_FreeMemory (void* p);
	static void* StpCallback_TransmitGetBuffer        (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int bpduSize, unsigned int timestamp);
	static void  StpCallback_TransmitReleaseBuffer    (const STP_BRIDGE* bridge, void* bufferReturnedByGetBuffer);
	static void  StpCallback_EnableBpduTrapping       (const STP_BRIDGE* bridge, bool enable, unsigned int timestamp);
	static void  StpCallback_EnableLearning           (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, bool enable, unsigned int timestamp);
	static void  StpCallback_EnableForwarding         (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, bool enable, unsigned int timestamp);
	static void  StpCallback_FlushFdb                 (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, enum STP_FLUSH_FDB_TYPE flushType);
	static void  StpCallback_DebugStrOut              (const STP_BRIDGE* bridge, int portIndex, int treeIndex, const char* nullTerminatedString, unsigned int stringLength, unsigned int flush);
	static void  StpCallback_OnTopologyChange         (const STP_BRIDGE* bridge, unsigned int treeIndex, unsigned int timestamp);
	static void  StpCallback_OnNotifiedTopologyChange (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, unsigned int timestamp);
	static void  StpCallback_OnPortRoleChanged        (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, STP_PORT_ROLE role, unsigned int timestamp);

	float x() const { return _x; }
	void set_x (float x) { base::set_and_invalidate(&x_property, _x, x); }
	float y() const { return _y; }
	void set_y (float y) { base::set_and_invalidate(&y_property, _y, y); }
	float width() const { return _width; }
	void set_width (float width) { base::set_and_invalidate(&width_property, _width, width); }
	float height() const { return _height; }
	void set_height (float height) { base::set_and_invalidate(&height_property, _height, height); }

	static const uint32_p      bridge_index_property;
	static const mac_address_p bridge_address_property;
	static const compat_bool_p stp_enabled_property;
	static const stp_version_p stp_version_property;
	static const uint32_p      port_count_property;
	static const uint32_p      msti_count_property;
	static const temp_string_p mst_config_id_name_property;
	static const uint32_p      MstConfigIdRevLevel;
	static const config_id_digest_p  MstConfigIdDigest;
	static const uint32_p      migrate_time_property;
	static const uint32_p      bridge_hello_time_property;
	static const uint32_p      bridge_max_age_property;
	static const uint32_p      bridge_forward_delay_property;
	static const uint32_p      tx_hold_count_property;
	static const uint32_p      max_hops_property;
	static const float_p x_property;
	static const float_p y_property;
	static const float_p width_property;
	static const float_p height_property;

	static const property* const _properties[];
	static const xtype<Bridge, uint32_p, uint32_p, mac_address_p> _type;
	const struct type* type() const override { return &_type; }
};
