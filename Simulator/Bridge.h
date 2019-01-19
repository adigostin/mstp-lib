
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
using stp_version_property = edge::enum_property<STP_VERSION, stp_version_type_name, stp_version_nvps>;

using mac_address = std::array<uint8_t, 6>;
std::string mac_address_to_string (mac_address from);
template<typename char_type> bool mac_address_from_string (std::basic_string_view<char_type> from, mac_address& to);
static constexpr char mac_address_type_name[] = "mac_address";
using mac_address_property = edge::typed_property<mac_address, mac_address, mac_address, mac_address_type_name, mac_address_to_string, mac_address_from_string>;

class Bridge : public renderable_object
{
	using base = renderable_object;

	float _x;
	float _y;
	float _width;
	float _height;
	std::vector<std::unique_ptr<Port>> _ports;
	bool _powered = true;
	STP_BRIDGE* _stpBridge = nullptr;
	static const STP_CALLBACKS StpCallbacks;
	std::vector<std::unique_ptr<BridgeLogLine>> _logLines;
	BridgeLogLine _currentLogLine;
	HANDLE _oneSecondTimerHandle;
	bool _simulationPaused = false;
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

	HelperWindow _helperWindow;

	// variables used by TransmitGetBuffer/ReleaseBuffer
	std::vector<uint8_t> _txPacketData;
	Port*                _txTransmittingPort;
	unsigned int         _txTimestamp;

public:
	Bridge (unsigned int portCount, unsigned int mstiCount, mac_address macAddress);
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

	virtual void RenderSelection (const edge::zoomable_i* zoomable, ID2D1RenderTarget* rt, const drawing_resources& dos) const override final;
	virtual HTResult HitTest (const edge::zoomable_i* zoomable, D2D1_POINT_2F dLocation, float tolerance) override final;

	STP_BRIDGE* stp_bridge() const { return _stpBridge; }

	struct LogLineGenerated : public edge::event<LogLineGenerated, Bridge*, const BridgeLogLine*> { };
	struct LinkPulseEvent : public edge::event<LinkPulseEvent, Bridge*, size_t, unsigned int> { };
	struct PacketTransmitEvent : public edge::event<PacketTransmitEvent, Bridge*, size_t, PacketInfo&&> { };

	LogLineGenerated::subscriber GetLogLineGeneratedEvent() { return LogLineGenerated::subscriber(this); }
	LinkPulseEvent::subscriber GetLinkPulseEvent() { return LinkPulseEvent::subscriber(this); }
	PacketTransmitEvent::subscriber GetPacketTransmitEvent() { return PacketTransmitEvent::subscriber(this); }

	void ProcessLinkPulse (size_t rxPortIndex, unsigned int timestamp);
	void EnqueuePacket (PacketInfo&& packet, size_t rxPortIndex);

	bool IsPowered() const { return _powered; }
	const std::vector<std::unique_ptr<BridgeLogLine>>& GetLogLines() const { return _logLines; }
	std::array<uint8_t, 6> GetPortAddress (size_t portIndex) const;
	mac_address bridge_address() const;
	void set_bridge_address (mac_address address);

	edge::com_ptr<IXMLDOMElement> Serialize (size_t bridgeIndex, IXMLDOMDocument3* doc) const;
	static std::unique_ptr<Bridge> Deserialize (IXMLDOMElement* element);

	void PauseSimulation();
	void ResumeSimulation();

	// Property getters and setters.
	bool GetStpEnabled() const { return (bool) STP_IsBridgeStarted(_stpBridge); }
	void SetStpEnabled (bool enable);
	STP_VERSION GetStpVersion() const { return STP_GetStpVersion(_stpBridge); }
	void SetStpVersion(STP_VERSION version);
	unsigned int GetPortCount() const { return STP_GetPortCount(_stpBridge); }
	unsigned int GetMstiCount() const { return STP_GetMstiCount(_stpBridge); }
	std::string GetMstConfigIdName() const;
	void SetMstConfigIdName (std::string_view value);
	uint32_t GetMstConfigIdRevLevel() const;
	void SetMstConfigIdRevLevel (uint32_t revLevel);
	std::string GetMstConfigIdDigest() const;
	void SetMstConfigTable (const STP_CONFIG_TABLE_ENTRY* entries, size_t entryCount);
	uint32_t GetBridgeHelloTime() const;
	void SetBridgeHelloTime (uint32_t helloTime);
	uint32_t GetHelloTime() const;
	uint32_t GetBridgeMaxAge() const;
	void SetBridgeMaxAge (uint32_t maxAge);
	uint32_t GetMaxAge() const;
	uint32_t GetBridgeForwardDelay() const;
	void SetBridgeForwardDelay (uint32_t forwardDelay);
	uint32_t GetForwardDelay() const;

private:
	static void CALLBACK OneSecondTimerCallback (void* lpParameter, BOOLEAN TimerOrWaitFired);
	static void OnWmOneSecondTimer (WPARAM wParam, LPARAM lParam);
	static void OnPortInvalidate (void* callbackArg, renderable_object* object);
	static void OnPortPropertyChanged (void* callbackArg, edge::object* object, const edge::property* property);
	static void OnLinkPulseTick(void* callbackArg);
	static void OnWmPacketReceived (WPARAM wParam, LPARAM lParam);
	void ProcessReceivedPackets();

	static void* StpCallback_AllocAndZeroMemory (unsigned int size);
	static void  StpCallback_FreeMemory (void* p);
	static void* StpCallback_TransmitGetBuffer        (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int bpduSize, unsigned int timestamp);
	static void  StpCallback_TransmitReleaseBuffer    (const STP_BRIDGE* bridge, void* bufferReturnedByGetBuffer);
	static void  StpCallback_EnableLearning           (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, unsigned int enable, unsigned int timestamp);
	static void  StpCallback_EnableForwarding         (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, unsigned int enable, unsigned int timestamp);
	static void  StpCallback_FlushFdb                 (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, enum STP_FLUSH_FDB_TYPE flushType);
	static void  StpCallback_DebugStrOut              (const STP_BRIDGE* bridge, int portIndex, int treeIndex, const char* nullTerminatedString, unsigned int stringLength, unsigned int flush);
	static void  StpCallback_OnTopologyChange         (const STP_BRIDGE* bridge);
	static void  StpCallback_OnNotifiedTopologyChange (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, unsigned int timestamp);
	static void  StpCallback_OnPortRoleChanged        (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, STP_PORT_ROLE role, unsigned int timestamp);
	static void  StpCallback_OnConfigChanged          (const STP_BRIDGE* bridge, unsigned int timestamp);
	
public:
	static const mac_address_property       Address;
	static const edge::bool_property        StpEnabled;
	static const stp_version_property       StpVersion;
	static const edge::uint32_property      PortCount;
	static const edge::uint32_property      MstiCount;
	static const edge::temp_string_property MstConfigIdName;
	static const edge::uint32_property      MstConfigIdRevLevel;
	static const edge::temp_string_property MstConfigIdDigest;
	static const edge::uint32_property      BridgeHelloTime;
	static const edge::uint32_property      HelloTime;
	static const edge::uint32_property      BridgeMaxAge;
	static const edge::uint32_property      MaxAge;
	static const edge::uint32_property      BridgeForwardDelay;
	static const edge::uint32_property      ForwardDelay;

	static const edge::property* const _properties[];
	static const edge::type_t _type;
	const edge::type_t* type() const override { return &_type; }
};
