
#pragma once
#include "Win32/EventManager.h"
#include "BridgeTree.h"
#include "Port.h"

struct BridgeLogLine
{
	std::string text;
	int portIndex;
	int treeIndex;
};

class Bridge : public RenderableObject
{
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
	struct HelperWindow : EventManager
	{
		HWND _hwnd;
		HANDLE _linkPulseTimerHandle;

		HelperWindow();
		~HelperWindow();

		static LRESULT CALLBACK SubclassProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

		struct LinkPulseEvent : public Event<LinkPulseEvent, void()> { };
		LinkPulseEvent::Subscriber GetLinkPulseEvent() { return LinkPulseEvent::Subscriber(this); }
	};

	HelperWindow _helperWindow;

	// variables used by TransmitGetBuffer/ReleaseBuffer
	std::vector<uint8_t> _txPacketData;
	Port*                _txTransmittingPort;
	unsigned int         _txTimestamp;

public:
	Bridge (unsigned int portCount, unsigned int mstiCount, const unsigned char macAddress[6]);
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

	const std::vector<std::unique_ptr<BridgeTree>>& GetTrees() const { return _trees; }
	const std::vector<std::unique_ptr<Port>>& GetPorts() const { return _ports; }

	void Render (ID2D1RenderTarget* dc, const DrawingObjects& dos, unsigned int vlanNumber, const D2D1_COLOR_F& configIdColor) const;

	virtual void RenderSelection (const IZoomable* zoomable, ID2D1RenderTarget* rt, const DrawingObjects& dos) const override final;
	virtual HTResult HitTest (const IZoomable* zoomable, D2D1_POINT_2F dLocation, float tolerance) override final;

	STP_BRIDGE* GetStpBridge() const { return _stpBridge; }

	struct LogLineGenerated : public Event<LogLineGenerated, void(Bridge*, const BridgeLogLine* line)> { };
	struct LinkPulseEvent : public Event<LinkPulseEvent, void(Bridge*, size_t txPortIndex, unsigned int timestamp)> { };
	struct PacketTransmitEvent : public Event<PacketTransmitEvent, void(Bridge*, size_t txPortIndex, PacketInfo&& packet)> { };

	LogLineGenerated::Subscriber GetLogLineGeneratedEvent() { return LogLineGenerated::Subscriber(this); }
	LinkPulseEvent::Subscriber GetLinkPulseEvent() { return LinkPulseEvent::Subscriber(this); }
	PacketTransmitEvent::Subscriber GetPacketTransmitEvent() { return PacketTransmitEvent::Subscriber(this); }

	void ProcessLinkPulse (size_t rxPortIndex, unsigned int timestamp);
	void EnqueuePacket (PacketInfo&& packet, size_t rxPortIndex);

	bool IsPowered() const { return _powered; }
	const std::vector<std::unique_ptr<BridgeLogLine>>& GetLogLines() const { return _logLines; }
	std::array<uint8_t, 6> GetPortAddress (size_t portIndex) const;
	std::array<uint8_t, 6> GetBridgeAddress() const;

	com_ptr<IXMLDOMElement> Serialize (size_t bridgeIndex, IXMLDOMDocument3* doc) const;
	static std::unique_ptr<Bridge> Deserialize (IXMLDOMElement* element);

	void PauseSimulation();
	void ResumeSimulation();

	// Property getters and setters.
	bool GetStpEnabled() const { return (bool) STP_IsBridgeStarted(_stpBridge); }
	void SetStpEnabled (bool enable);
	int GetStpVersionAsInt() const { return (int) STP_GetStpVersion(_stpBridge); }
	void SetStpVersionFromInt (int version);
	unsigned int GetPortCount() const { return STP_GetPortCount(_stpBridge); }
	unsigned int GetMstiCount() const { return STP_GetMstiCount(_stpBridge); }
	std::wstring GetMstConfigIdName() const;
	void SetMstConfigIdName (std::wstring value);
	unsigned short GetMstConfigIdRevLevel() const;
	void SetMstConfigIdRevLevel (unsigned short revLevel);
	std::wstring GetMstConfigIdDigest() const;
	void SetMstConfigTable (const STP_CONFIG_TABLE_ENTRY* entries, size_t entryCount);
	uint32_t GetBridgeHelloTime() const;
	void SetBridgeHelloTime (uint32_t helloTime);
	uint32_t GetHelloTime() const;
	uint32_t GetBridgeMaxAge() const;
	void SetBridgeMaxAge (uint32_t maxAge);
	uint32_t GetMaxAge() const;

private:
	static void CALLBACK OneSecondTimerCallback (void* lpParameter, BOOLEAN TimerOrWaitFired);
	static void OnWmOneSecondTimer (WPARAM wParam, LPARAM lParam);
	static void OnPortInvalidate (void* callbackArg, Object* object);
	static void OnPortPropertyChanged (void* callbackArg, Object* object, const Property* property);
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

private:
	std::string GetBridgeAddressAsString() const;
	std::wstring GetBridgeAddressAsWString() const;
	void SetBridgeAddressFromWString (std::wstring address);

	static const PropertyGroup CommonPropGroup;
	static const TypedProperty<std::wstring> Address;
	static const TypedProperty<bool> StpEnabled;
	static const EnumProperty StpVersion;
	static const TypedProperty<unsigned int> PortCount;
	static const TypedProperty<unsigned int> MstiCount;
	static const PropertyGroup MstConfigIdGroup;
	static const TypedProperty<std::wstring> MstConfigIdName;
	static const TypedProperty<unsigned short> MstConfigIdRevLevel;
	static const TypedProperty<std::wstring> MstConfigIdDigest;
	static const TypedProperty<uint32_t> BridgeHelloTime;
	static const TypedProperty<uint32_t> HelloTime;
	static const TypedProperty<uint32_t> BridgeMaxAge;
	static const TypedProperty<uint32_t> MaxAge;

	static const PropertyOrGroup* const Properties[];
	virtual const PropertyOrGroup* const* GetProperties() const override final { return Properties; }
};

STP_BRIDGE_ADDRESS ConvertStringToBridgeAddress (const wchar_t* str);
std::string ConvertBridgeAddressToString (const unsigned char address[6]);
std::wstring ConvertBridgeAddressToWString (const unsigned char address[6]);
