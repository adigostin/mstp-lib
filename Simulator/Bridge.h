
#pragma once
#include "EventManager.h"
#include "SimulatorDefs.h"
#include "mstp-lib/stp.h"

static constexpr float PortLongSize = 30;
static constexpr float PortShortSize = 15;
static constexpr float PortSpacing = 20;
static constexpr float PortInteriorLongSize = 25;  // Size along the edge of the bridge.
static constexpr float PortInteriorShortSize = 14; // Size from the edge to the interior of the bridge.
static constexpr float PortExteriorWidth = 12;
static constexpr float PortExteriorHeight = 24;
static constexpr float BridgeDefaultHeight = 120;
static constexpr float BridgeOutlineWidth = 4;
static constexpr float MinBridgeWidth = 250;
static constexpr float BridgeRoundRadius = 8;

class Bridge;

class Port : public Object
{
	Bridge* const _bridge;
	unsigned int const _portIndex;
	Side _side;
	float _offset;

public:
	Port (Bridge* bridge, unsigned int portIndex, Side side, float offset)
		: _bridge(bridge), _portIndex(portIndex), _side(side), _offset(offset)
	{ }

protected:
	virtual ~Port() = default;

public:
	Bridge* GetBridge() const { return _bridge; }
	Side GetSide() const { return _side; }
	float GetOffset() const { return _offset; }
	D2D1_POINT_2F GetConnectionPointLocation() const;
	bool GetMacOperational() const { return true; } // TODO
};

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
	
	void Render (ID2D1RenderTarget* dc, const DrawingObjects& dos, IDWriteFactory* dWriteFactory, uint16_t vlanNumber) const;
	static void RenderExteriorNonStpPort (ID2D1RenderTarget* dc, const DrawingObjects& dos, bool macOperational);
	static void RenderExteriorStpPort (ID2D1RenderTarget* dc, const DrawingObjects& dos, STP_PORT_ROLE role, bool learning, bool forwarding, bool operEdge);

	BridgeStartedEvent::Subscriber GetBridgeStartedEvent() { return BridgeStartedEvent::Subscriber(_em); }
	BridgeStoppingEvent::Subscriber GetBridgeStoppingEvent() { return BridgeStoppingEvent::Subscriber(_em); }
	BridgeLogLineGenerated::Subscriber GetBridgeLogLineGeneratedEvent() { return BridgeLogLineGenerated::Subscriber(_em); }

	bool IsPowered() const { return _powered; }
	void EnableStp (STP_VERSION stpVersion, unsigned int treeCount, uint32_t timestamp);
	void DisableStp (uint32_t timestamp);
	bool IsStpEnabled() const { return _stpBridge != nullptr; }
	unsigned int GetTreeCount() const;
	STP_PORT_ROLE GetStpPortRole (unsigned int portIndex, unsigned int treeIndex) const;
	bool GetStpPortLearning (unsigned int portIndex, unsigned int treeIndex) const;
	bool GetStpPortForwarding (unsigned int portIndex, unsigned int treeIndex) const;
	bool GetStpPortOperEdge (unsigned int portIndex) const;
	unsigned short GetStpBridgePriority (unsigned int treeIndex) const;
	unsigned int GetStpTreeIndexFromVlanNumber (unsigned short vlanNumber) const;
	const std::vector<BridgeLogLine>& GetLogLines() const { return _logLines; }

private:	
	static void* StpCallback_AllocAndZeroMemory (unsigned int size);
	static void  StpCallback_FreeMemory (void* p);
	static void  StpCallback_EnableLearning (STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, bool enable);
	static void  StpCallback_EnableForwarding (STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, bool enable);
	static void  StpCallback_FlushFdb (STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, enum STP_FLUSH_FDB_TYPE flushType);
	static void  StpCallback_DebugStrOut (STP_BRIDGE* bridge, int portIndex, int treeIndex, const char* nullTerminatedString, unsigned int stringLength, bool flush);
};

