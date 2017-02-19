
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

struct BridgeStartedEvent : public Event<BridgeStartedEvent, void(Bridge*)> { };
struct BridgeStoppingEvent : public Event<BridgeStoppingEvent, void(Bridge*)> { };
struct BridgeLogLineGenerated : public Event<BridgeLogLineGenerated, void(Bridge*, const BridgeLogLine& line)> { };

class Bridge : public Object
{
	float _x;
	float _y;
	float _width;
	float _height;
	std::vector<ComPtr<Port>> _ports;
	std::array<uint8_t, 6> _macAddress;
	bool _powered = true;
	STP_BRIDGE* _stpBridge = nullptr; // when nullptr, STP is disabled in the bridge
	std::mutex _stpBridgeMutex;
	std::thread::id _guiThreadId;
	static const STP_CALLBACKS StpCallbacks;
	std::vector<BridgeLogLine> _logLines;
	BridgeLogLine _currentLogLine;

public:
	Bridge (unsigned int portCount, const std::array<uint8_t, 6>& macAddress);
protected:
	~Bridge();

public:
	static constexpr int HTCodeInner = 1;

	static constexpr float DefaultHeight = 120;
	static constexpr float OutlineWidth = 4;
	static constexpr float MinWidth = 250;
	static constexpr float RoundRadius = 8;

	float GetLeft() const { return _x; }
	float GetRight() const { return _x + _width; }
	float GetTop() const { return _y; }
	float GetBottom() const { return _y + _height; }
	float GetWidth() const { return _width; }
	float GetHeight() const { return _height; }
	void SetLocation (float x, float y);

	D2D1_RECT_F GetBounds() const { return { _x, _y, _x + _width, _y + _height }; }
	const std::vector<ComPtr<Port>>& GetPorts() const { return _ports; }
	std::array<uint8_t, 6> GetMacAddress() const { return _macAddress; }
	
	virtual void Render (ID2D1RenderTarget* dc, const DrawingObjects& dos, IDWriteFactory* dWriteFactory, uint16_t vlanNumber) const override final;
	virtual void RenderSelection (const IZoomable* zoomable, ID2D1RenderTarget* rt, const DrawingObjects& dos) const override final;
	virtual HTResult HitTest (const IZoomable* zoomable, D2D1_POINT_2F dLocation, float tolerance) override final;

	BridgeStartedEvent::Subscriber GetBridgeStartedEvent() { return BridgeStartedEvent::Subscriber(_em); }
	BridgeStoppingEvent::Subscriber GetBridgeStoppingEvent() { return BridgeStoppingEvent::Subscriber(_em); }
	BridgeLogLineGenerated::Subscriber GetBridgeLogLineGeneratedEvent() { return BridgeLogLineGenerated::Subscriber(_em); }

	bool IsPowered() const { return _powered; }
	void EnableStp (STP_VERSION stpVersion, uint16_t treeCount, uint32_t timestamp);
	void DisableStp (uint32_t timestamp);
	bool IsStpEnabled() const { return _stpBridge != nullptr; }
	uint16_t GetTreeCount() const;
	STP_PORT_ROLE GetStpPortRole (uint16_t portIndex, uint16_t treeIndex) const;
	bool GetStpPortLearning (uint16_t portIndex, uint16_t treeIndex) const;
	bool GetStpPortForwarding (uint16_t portIndex, uint16_t treeIndex) const;
	bool GetStpPortOperEdge (uint16_t portIndex) const;
	uint16_t GetStpBridgePriority (uint16_t treeIndex) const;
	uint16_t GetStpTreeIndexFromVlanNumber (uint16_t vlanNumber) const;
	const std::vector<BridgeLogLine>& GetLogLines() const { return _logLines; }

private:	
	static void* StpCallback_AllocAndZeroMemory (unsigned int size);
	static void  StpCallback_FreeMemory (void* p);
	static void  StpCallback_EnableLearning (STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, bool enable);
	static void  StpCallback_EnableForwarding (STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, bool enable);
	static void  StpCallback_FlushFdb (STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, enum STP_FLUSH_FDB_TYPE flushType);
	static void  StpCallback_DebugStrOut (STP_BRIDGE* bridge, int portIndex, int treeIndex, const char* nullTerminatedString, unsigned int stringLength, bool flush);
};

