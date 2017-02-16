
#pragma once
#include "EventManager.h"
#include "mstp-lib/stp.h"

static constexpr float PortLongSize = 30;
static constexpr float PortShortSize = 15;
static constexpr float PortSpacing = 20;
static constexpr float BridgeDefaultHeight = 150;
static constexpr float BridgeOutlineWidth = 4;
static constexpr float MinBridgeWidth = 300;

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
	size_t const _portIndex;
	Side _side;
	float _offset;

public:
	PhysicalPort (PhysicalBridge* bridge, size_t portIndex, Side side, float offset)
		: _bridge(bridge), _portIndex(portIndex), _side(side), _offset(offset)
	{ }

	void Render(ID2D1DeviceContext* dc) const;
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
	STP_BRIDGE* _stpBridge;
	std::mutex _stpBridgeMutex;
	std::thread::id _guiThreadId;
	static const STP_CALLBACKS StpCallbacks;

public:
	PhysicalBridge (size_t portCount, const std::array<uint8_t, 6>& macAddress);
	~PhysicalBridge();

	float GetLeft() const { return _x; }
	float GetRight() const { return _x + _width; }
	float GetTop() const { return _y; }
	float GetBottom() const { return _y + _height; }

	void Render (ID2D1DeviceContext* dc, unsigned int treeIndex, IDWriteFactory* dWriteFactory);
	void SetLocation (float x, float y);
	
	BridgeInvalidateEvent::Subscriber GetInvalidateEvent() { return BridgeInvalidateEvent::Subscriber(_em); }
	BridgeStartedEvent::Subscriber GetBridgeStartedEvent() { return BridgeStartedEvent::Subscriber(_em); }
	BridgeStoppingEvent::Subscriber GetBridgeStoppingEvent() { return BridgeStoppingEvent::Subscriber(_em); }

	void StartStpBridge (uint32_t timestamp);
	void StopStpBridge (uint32_t timestamp);
	bool IsStpBridgeStarted();

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

