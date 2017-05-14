
#pragma once
#include "EventManager.h"
#include "Simulator.h"

struct BridgeLogLine
{
	std::string text;
	int portIndex;
	int treeIndex;
};

struct BridgeLogLineGenerated : public Event<BridgeLogLineGenerated, void(Bridge*, const BridgeLogLine* line)> { };
struct BridgeConfigChangedEvent : public Event<BridgeConfigChangedEvent, void(Bridge*)> { };

class Bridge : public Object
{
	IProject* const _project;
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
	TimerQueueTimer_unique_ptr _macOperationalTimerHandle;
	static HWND _helperWindow;
	static uint32_t _helperWindowRefCount;

	struct RxPacketInfo
	{
		std::vector<uint8_t> data;
		unsigned int portIndex;
		unsigned int timestamp;
		std::vector<std::array<uint8_t, 6>> txPortPath;
	};
	std::queue<RxPacketInfo> _rxQueue;

	// variables used by TransmitGetBuffer/ReleaseBuffer
	std::vector<uint8_t> _txPacketData;
	Port*                _txTransmittingPort;
	Port*                _txReceivingPort;
	unsigned int         _txTimestamp;

private:
	Bridge (IProject* project, unsigned int portCount, unsigned int mstiCount, const std::array<uint8_t, 6>& macAddress);
public:
	template<typename... Args>
	static std::unique_ptr<Bridge> Create (Args... args) { return std::unique_ptr<Bridge>(new Bridge(std::forward<Args>(args)...)); }

	virtual ~Bridge();

public:
	static constexpr int HTCodeInner = 1;

	static constexpr float DefaultHeight = 100;
	static constexpr float OutlineWidth = 2;
	static constexpr float MinWidth = 180;
	static constexpr float RoundRadius = 8;

	IProject* GetProject() const { return _project; }

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

	std::wstring GetBridgeAddressAsString() const;

	void Render (ID2D1RenderTarget* dc, const DrawingObjects& dos, unsigned int vlanNumber, const D2D1_COLOR_F& configIdColor) const;

	virtual void RenderSelection (const IZoomable* zoomable, ID2D1RenderTarget* rt, const DrawingObjects& dos) const override final;
	virtual HTResult HitTest (const IZoomable* zoomable, D2D1_POINT_2F dLocation, float tolerance) override final;

	STP_BRIDGE* GetStpBridge() const { return _stpBridge; }

	BridgeLogLineGenerated::Subscriber GetBridgeLogLineGeneratedEvent() { return BridgeLogLineGenerated::Subscriber(*this); }
	BridgeConfigChangedEvent::Subscriber GetBridgeConfigChangedEvent() { return BridgeConfigChangedEvent::Subscriber(*this); }

	bool IsPowered() const { return _powered; }
	const std::vector<std::unique_ptr<BridgeLogLine>>& GetLogLines() const { return _logLines; }

	IXMLDOMElementPtr Serialize (IXMLDOMDocument3* doc) const;
	static std::unique_ptr<Bridge> Deserialize (IXMLDOMElement* element);

private:
	static void CALLBACK OneSecondTimerCallback (void* lpParameter, BOOLEAN TimerOrWaitFired);
	static void CALLBACK MacOperationalTimerCallback (void* lpParameter, BOOLEAN TimerOrWaitFired);
	static LRESULT CALLBACK HelperWindowProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
	static void OnPortInvalidate (void* callbackArg, Object* object);
	void ComputeMacOperational();
	void ProcessReceivedPacket();
	std::array<uint8_t, 6> GetPortAddress (size_t portIndex) const;

	static void* StpCallback_AllocAndZeroMemory (unsigned int size);
	static void  StpCallback_FreeMemory (void* p);
	static void* StpCallback_TransmitGetBuffer (STP_BRIDGE* bridge, unsigned int portIndex, unsigned int bpduSize, unsigned int timestamp);
	static void  StpCallback_TransmitReleaseBuffer (STP_BRIDGE* bridge, void* bufferReturnedByGetBuffer);
	static void  StpCallback_EnableLearning (STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, unsigned int enable, unsigned int timestamp);
	static void  StpCallback_EnableForwarding (STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, unsigned int enable, unsigned int timestamp);
	static void  StpCallback_FlushFdb (STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, enum STP_FLUSH_FDB_TYPE flushType);
	static void  StpCallback_DebugStrOut (STP_BRIDGE* bridge, int portIndex, int treeIndex, const char* nullTerminatedString, unsigned int stringLength, unsigned int flush);
	static void  StpCallback_OnTopologyChange (STP_BRIDGE* bridge);
	static void  StpCallback_OnNotifiedTopologyChange (STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, unsigned int timestamp);
	static void  StpCallback_OnPortRoleChanged (STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, STP_PORT_ROLE role, unsigned int timestamp);
	static void  StpCallback_OnConfigChanged (struct STP_BRIDGE* bridge, unsigned int timestamp);
};

