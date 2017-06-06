
#pragma once
#include "Win32/EventManager.h"
#include "Port.h"

struct BridgeLogLine
{
	std::string text;
	int portIndex;
	int treeIndex;
};

class Bridge : public Object
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
	TimerQueueTimer_unique_ptr _oneSecondTimerHandle;
	bool _simulationPaused = false;
	std::queue<std::pair<size_t, PacketInfo>> _rxQueue;

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

	static HelperWindow _helperWindow;

	// variables used by TransmitGetBuffer/ReleaseBuffer
	std::vector<uint8_t> _txPacketData;
	Port*                _txTransmittingPort;
	unsigned int         _txTimestamp;

public:
	Bridge (unsigned int portCount, unsigned int mstiCount, const unsigned char macAddress[6]);
	virtual ~Bridge();

	virtual const Property* const* GetProperties() const override final;

public:
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

	const std::vector<std::unique_ptr<Port>>& GetPorts() const { return _ports; }

	std::string GetBridgeAddressAsString() const;
	std::wstring GetBridgeAddressAsWString() const;

	void Render (ID2D1RenderTarget* dc, const DrawingObjects& dos, unsigned int vlanNumber, const D2D1_COLOR_F& configIdColor) const;

	virtual void RenderSelection (const IZoomable* zoomable, ID2D1RenderTarget* rt, const DrawingObjects& dos) const override final;
	virtual HTResult HitTest (const IZoomable* zoomable, D2D1_POINT_2F dLocation, float tolerance) override final;

	STP_BRIDGE* GetStpBridge() const { return _stpBridge; }

	struct LogLineGenerated : public Event<LogLineGenerated, void(Bridge*, const BridgeLogLine* line)> { };
	struct ConfigChangedEvent : public Event<ConfigChangedEvent, void(Bridge*)> { };
	struct LinkPulseEvent : public Event<LinkPulseEvent, void(Bridge*, size_t txPortIndex, unsigned int timestamp)> { };
	struct PacketTransmitEvent : public Event<PacketTransmitEvent, void(Bridge*, size_t txPortIndex, PacketInfo&& packet)> { };

	LogLineGenerated::Subscriber GetLogLineGeneratedEvent() { return LogLineGenerated::Subscriber(this); }
	ConfigChangedEvent::Subscriber GetConfigChangedEvent() { return ConfigChangedEvent::Subscriber(this); }
	LinkPulseEvent::Subscriber GetLinkPulseEvent() { return LinkPulseEvent::Subscriber(this); }
	PacketTransmitEvent::Subscriber GetPacketTransmitEvent() { return PacketTransmitEvent::Subscriber(this); }

	void ProcessLinkPulse (size_t rxPortIndex, unsigned int timestamp);
	void EnqueuePacket (PacketInfo&& packet, size_t rxPortIndex);

private:
	static void OnWmPacketReceived (WPARAM wParam, LPARAM lParam);
	void ProcessReceivedPackets();

public:
	bool IsPowered() const { return _powered; }
	const std::vector<std::unique_ptr<BridgeLogLine>>& GetLogLines() const { return _logLines; }
	std::array<uint8_t, 6> GetPortAddress (size_t portIndex) const;

	IXMLDOMElementPtr Serialize (size_t bridgeIndex, IXMLDOMDocument3* doc) const;
	static std::unique_ptr<Bridge> Deserialize (IXMLDOMElement* element);

	void PauseSimulation();
	void ResumeSimulation();

private:
	static void CALLBACK OneSecondTimerCallback (void* lpParameter, BOOLEAN TimerOrWaitFired);
	static void OnWmOneSecondTimer (WPARAM wParam, LPARAM lParam);
	static void OnPortInvalidate (void* callbackArg, Object* object);
	static void OnLinkPulseTick(void* callbackArg);

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
};

