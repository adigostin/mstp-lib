
#pragma once
#include "EventManager.h"
#include "Simulator.h"
#include "mstp-lib/stp.h"

struct BridgeLogLine
{
	std::string text;
	int portIndex;
	int treeIndex;
};

struct BridgeAddressChangedEvent : public Event<BridgeAddressChangedEvent, void(Bridge*)> { };
struct StpEnabledChangedEvent : public Event<StpEnabledChangedEvent, void(Bridge*)> { };
struct StpVersionChangedEvent : public Event<StpVersionChangedEvent, void(Bridge*)> { };
struct BridgeLogLineGenerated : public Event<BridgeLogLineGenerated, void(Bridge*, const BridgeLogLine& line)> { };
struct PortCountChangedEvent : public Event<PortCountChangedEvent, void(Bridge*)> { };
struct TreeCountChangedEvent : public Event<TreeCountChangedEvent, void(Bridge*)> { };
struct MstConfigNameChangedEvent : public Event<MstConfigNameChangedEvent, void(Bridge*)> { };
struct MstConfigRevLevelChangedEvent : public Event<MstConfigRevLevelChangedEvent, void(Bridge*)> { };

constexpr unsigned int MaxVlanNumber = 4094;

class Bridge : public Object
{
	IProject* const _project;
	float _x;
	float _y;
	float _width;
	float _height;
	std::vector<ComPtr<Port>> _ports;
	bool _powered = true;
	STP_BRIDGE* _stpBridge = nullptr;
	static const STP_CALLBACKS StpCallbacks;
	std::vector<BridgeLogLine> _logLines;
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

public:
	Bridge (IProject* project, unsigned int portCount, const std::array<uint8_t, 6>& macAddress);
protected:
	~Bridge();

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
	void SetLocation (float x, float y);

	D2D1_RECT_F GetBounds() const { return { _x, _y, _x + _width, _y + _height }; }
	const std::vector<ComPtr<Port>>& GetPorts() const { return _ports; }
	std::array<uint8_t, 6> GetBridgeAddress() const;
	std::wstring GetBridgeAddressAsString() const;

	virtual void Render (ID2D1RenderTarget* dc, const DrawingObjects& dos, uint16_t vlanNumber) const override final;
	virtual void RenderSelection (const IZoomable* zoomable, ID2D1RenderTarget* rt, const DrawingObjects& dos) const override final;
	virtual HTResult HitTest (const IZoomable* zoomable, D2D1_POINT_2F dLocation, float tolerance) override final;

	BridgeAddressChangedEvent::Subscriber GetBridgeAddressChangedEvent() { return BridgeAddressChangedEvent::Subscriber(_em); }
	StpEnabledChangedEvent::Subscriber GetStpEnabledChangedEvent() { return StpEnabledChangedEvent::Subscriber(_em); }
	StpVersionChangedEvent::Subscriber GetStpVersionChangedEvent() { return StpVersionChangedEvent::Subscriber(_em); }
	BridgeLogLineGenerated::Subscriber GetBridgeLogLineGeneratedEvent() { return BridgeLogLineGenerated::Subscriber(_em); }
	PortCountChangedEvent::Subscriber GetPortCountChangedEvent() { return PortCountChangedEvent::Subscriber(_em); }
	TreeCountChangedEvent::Subscriber GetTreeCountChangedEvent() { return TreeCountChangedEvent::Subscriber(_em); }
	MstConfigNameChangedEvent::Subscriber GetMstConfigNameChangedEvent() { return MstConfigNameChangedEvent::Subscriber(_em); }
	MstConfigRevLevelChangedEvent::Subscriber GetMstConfigRevLevelChangedEvent() { return MstConfigRevLevelChangedEvent::Subscriber(_em); }

	bool IsPowered() const { return _powered; }
	void StartStp (unsigned int timestamp);
	void StopStp (unsigned int timestamp);
	bool IsStpStarted() const;
	STP_PORT_ROLE GetStpPortRole (unsigned int portIndex, unsigned int treeIndex) const;
	bool GetStpPortLearning      (unsigned int portIndex, unsigned int treeIndex) const;
	bool GetStpPortForwarding    (unsigned int portIndex, unsigned int treeIndex) const;
	bool GetStpPortOperEdge      (unsigned int portIndex) const;
	bool GetPortAdminEdge (unsigned int portIndex) const;
	void SetPortAdminEdge (unsigned int portIndex, bool adminEdge, unsigned int timestamp);
	bool GetPortAutoEdge  (unsigned int portIndex) const;
	void SetPortAutoEdge  (unsigned int portIndex, bool autoEdge, unsigned int timestamp);
	uint16_t GetStpBridgePriority (unsigned int treeIndex) const;
	unsigned int GetStpTreeIndexFromVlanNumber (uint16_t vlanNumber) const;
	const std::vector<BridgeLogLine>& GetLogLines() const { return _logLines; }
	bool IsPortForwardingOnVlan (unsigned int portIndex, uint16_t vlanNumber) const;
	bool IsStpRootBridge() const;
	STP_VERSION GetStpVersion() const;
	void SetStpVersion (STP_VERSION stpVersion, unsigned int timestamp);
	std::wstring GetStpVersionString() const;
	static std::wstring GetStpVersionString (STP_VERSION stpVersion);
	std::string GetMstConfigName() const;
	void SetMstConfigName (const char* name, unsigned int timestamp);
	unsigned short GetMstConfigRevLevel() const;
	void SetMstConfigRevLevel (unsigned short revLevel, unsigned int timestamp);
	std::array<uint8_t, 16> GetMstConfigDigest();

private:
	static void CALLBACK OneSecondTimerCallback (void* lpParameter, BOOLEAN TimerOrWaitFired);
	static void CALLBACK MacOperationalTimerCallback (void* lpParameter, BOOLEAN TimerOrWaitFired);
	static LRESULT CALLBACK HelperWindowProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
	void ComputeMacOperational();
	void ProcessReceivedPacket();
	std::array<uint8_t, 6> GetPortAddress (size_t portIndex) const;

	static void* StpCallback_AllocAndZeroMemory (unsigned int size);
	static void  StpCallback_FreeMemory (void* p);
	static void* StpCallback_TransmitGetBuffer (STP_BRIDGE* bridge, unsigned int portIndex, unsigned int bpduSize, unsigned int timestamp);
	static void  StpCallback_TransmitReleaseBuffer (STP_BRIDGE* bridge, void* bufferReturnedByGetBuffer);
	static void  StpCallback_EnableLearning (STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, unsigned int enable);
	static void  StpCallback_EnableForwarding (STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, unsigned int enable);
	static void  StpCallback_FlushFdb (STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, enum STP_FLUSH_FDB_TYPE flushType);
	static void  StpCallback_DebugStrOut (STP_BRIDGE* bridge, int portIndex, int treeIndex, const char* nullTerminatedString, unsigned int stringLength, unsigned int flush);
	static void  StpCallback_OnTopologyChange (STP_BRIDGE* bridge);
	static void  StpCallback_OnNotifiedTopologyChange (STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex);
	static void  StpCallback_OnPortRoleChanged (STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, STP_PORT_ROLE role);
};

