
#pragma once
#include "EventManager.h"
#include "mstp-lib/stp.h"

static constexpr float PortLongSize = 30;
static constexpr float PortShortSize = 15;
static constexpr float PortSpacing = 20;
static constexpr float PortInteriorLongSize = 25;  // Size along the edge of the bridge.
static constexpr float PortInteriorShortSize = 14; // Size from the edge to the interior of the bridge.
static constexpr float PortExteriorWidth = 12;
static constexpr float PortExteriorHeight = 18;
static constexpr float BridgeDefaultHeight = 150;
static constexpr float BridgeOutlineWidth = 4;
static constexpr float MinBridgeWidth = 300;
static constexpr float BridgeRoundRadius = 8;

class Object
{
protected:
	virtual ~Object() { }
};

enum class Side { Left, Top, Right, Bottom };

class PhysicalBridge;

class PhysicalPort : public Object
{
	PhysicalBridge* const _bridge;
	unsigned int const _portIndex;
	Side _side;
	float _offset;

public:
	PhysicalPort (PhysicalBridge* bridge, unsigned int portIndex, Side side, float offset)
		: _bridge(bridge), _portIndex(portIndex), _side(side), _offset(offset)
	{ }

	Side GetSide() const { return _side; }
	float GetOffset() const { return _offset; }
	
	bool GetMacOperational() const { return true; } // TODO
};

struct BridgeInvalidateEvent : public Event<BridgeInvalidateEvent, void(PhysicalBridge*)> { };
struct BridgeStartedEvent : public Event<BridgeStartedEvent, void(PhysicalBridge*)> { };
struct BridgeStoppingEvent : public Event<BridgeStoppingEvent, void(PhysicalBridge*)> { };

class PhysicalBridge : public Object, public IUnknown
{
	ULONG _refCount = 1;
	float _x;
	float _y;
	float _width;
	float _height;
	EventManager _em;
	std::vector<std::unique_ptr<PhysicalPort>> _ports;
	std::array<uint8_t, 6> _macAddress;
	bool _powered = true;
	STP_BRIDGE* _stpBridge = nullptr; // when nullptr, STP is disabled in the bridge
	std::mutex _stpBridgeMutex;
	std::thread::id _guiThreadId;
	static const STP_CALLBACKS StpCallbacks;

public:
	PhysicalBridge (unsigned int portCount, const std::array<uint8_t, 6>& macAddress);
	~PhysicalBridge();

	float GetLeft() const { return _x; }
	float GetRight() const { return _x + _width; }
	float GetTop() const { return _y; }
	float GetBottom() const { return _y + _height; }
	D2D1_RECT_F GetBounds() const { return { _x, _y, _x + _width, _y + _height }; }
	unsigned int GetPortCount() const { return (unsigned int) _ports.size(); }
	PhysicalPort* GetPort(size_t portIndex) const { return _ports[portIndex].get(); }
	std::array<uint8_t, 6> GetMacAddress() const { return _macAddress; }

	void SetLocation (float x, float y);
	
	BridgeInvalidateEvent::Subscriber GetInvalidateEvent() { return BridgeInvalidateEvent::Subscriber(_em); }
	BridgeStartedEvent::Subscriber GetBridgeStartedEvent() { return BridgeStartedEvent::Subscriber(_em); }
	BridgeStoppingEvent::Subscriber GetBridgeStoppingEvent() { return BridgeStoppingEvent::Subscriber(_em); }

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

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override final;
	virtual ULONG STDMETHODCALLTYPE AddRef() override final;
	virtual ULONG STDMETHODCALLTYPE Release() override final;

private:
	static void* StpCallback_AllocAndZeroMemory (unsigned int size);
	static void  StpCallback_FreeMemory (void* p);
	static void  StpCallback_EnableLearning (STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, bool enable);
	static void  StpCallback_EnableForwarding (STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, bool enable);
	static void  StpCallback_FlushFdb (STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, enum STP_FLUSH_FDB_TYPE flushType);
};

