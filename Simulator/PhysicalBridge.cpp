#include "pch.h"
#include "PhysicalBridge.h"
#include "Win32Defs.h"

using namespace std;
using namespace D2D1;

PhysicalBridge::PhysicalBridge (unsigned int portCount, const std::array<uint8_t, 6>& macAddress)
	: _macAddress(macAddress), _guiThreadId(this_thread::get_id())
{
	float offset = 0;

	for (size_t i = 0; i < portCount; i++)
	{
		offset += (PortSpacing / 2 + PortLongSize / 2);
		auto port = make_unique<PhysicalPort>(this, i, Side::Bottom, offset);
		_ports.push_back (move(port));
		offset += (PortLongSize / 2 + PortSpacing / 2);
	}

	_x = 0;
	_y = 0;
	_width = max (offset, MinBridgeWidth);
	_height = BridgeDefaultHeight;
}

PhysicalBridge::~PhysicalBridge()
{
	assert (this_thread::get_id() == _guiThreadId);
	if (_stpBridge != nullptr)
		STP_DestroyBridge (_stpBridge);
}

void PhysicalBridge::EnableStp (STP_VERSION stpVersion, unsigned int treeCount, uint32_t timestamp)
{
	if (this_thread::get_id() != _guiThreadId)
		throw runtime_error ("This function may be called only on the main thread.");

	if (_stpBridge != nullptr)
		throw runtime_error ("STP is already enabled on this bridge.");

	_stpBridge = STP_CreateBridge ((unsigned int) _ports.size(), treeCount, &StpCallbacks, STP_VERSION_RSTP, &_macAddress[0], 128);
	STP_SetApplicationContext (_stpBridge, this);
	STP_StartBridge (_stpBridge, timestamp);
	BridgeStartedEvent::InvokeHandlers (_em, this);

	BridgeInvalidateEvent::InvokeHandlers(_em, this);
}

void PhysicalBridge::DisableStp (uint32_t timestamp)
{
	if (this_thread::get_id() != _guiThreadId)
		throw runtime_error ("This function may be called only on the main thread.");

	if (_stpBridge == nullptr)
		throw runtime_error ("STP was not enabled on this bridge.");

	BridgeStoppingEvent::InvokeHandlers(_em, this);
	STP_StopBridge(_stpBridge, timestamp);
	STP_DestroyBridge (_stpBridge);
	_stpBridge = nullptr;

	BridgeInvalidateEvent::InvokeHandlers(_em, this);
}

void PhysicalBridge::SetLocation(float x, float y)
{
	if ((_x != x) || (_y != y))
	{
		BridgeInvalidateEvent::InvokeHandlers(_em, this);
		_x = x;
		_y = y;
		BridgeInvalidateEvent::InvokeHandlers(_em, this);
	}
}

unsigned int PhysicalBridge::GetTreeCount() const
{
	if (_stpBridge == nullptr)
		throw runtime_error ("STP was not enabled on this bridge.");

	return STP_GetTreeCount(_stpBridge);
}

STP_PORT_ROLE PhysicalBridge::GetStpPortRole (unsigned int portIndex, unsigned int treeIndex) const
{
	if (_stpBridge == nullptr)
		throw runtime_error ("STP was not enabled on this bridge.");

	return STP_GetPortRole (_stpBridge, portIndex, treeIndex);
}

bool PhysicalBridge::GetStpPortLearning (unsigned int portIndex, unsigned int treeIndex) const
{
	if (_stpBridge == nullptr)
		throw runtime_error ("STP was not enabled on this bridge.");

	return STP_GetPortLearning (_stpBridge, portIndex, treeIndex);
}

bool PhysicalBridge::GetStpPortForwarding (unsigned int portIndex, unsigned int treeIndex) const
{
	if (_stpBridge == nullptr)
		throw runtime_error ("STP was not enabled on this bridge.");

	return STP_GetPortForwarding (_stpBridge, portIndex, treeIndex);
}

bool PhysicalBridge::GetStpPortOperEdge (unsigned int portIndex) const
{
	if (_stpBridge == nullptr)
		throw runtime_error ("STP was not enabled on this bridge.");

	return STP_GetPortOperEdge (_stpBridge, portIndex);
}

unsigned short PhysicalBridge::GetStpBridgePriority (unsigned int treeIndex) const
{
	if (_stpBridge == nullptr)
		throw runtime_error ("STP was not enabled on this bridge.");

	return STP_GetBridgePriority(_stpBridge, treeIndex);
}

#pragma region STP Callbacks
const STP_CALLBACKS PhysicalBridge::StpCallbacks =
{
	&StpCallback_EnableLearning,
	&StpCallback_EnableForwarding,
	nullptr, // transmitGetBuffer;
	nullptr, // transmitReleaseBuffer;
	&StpCallback_FlushFdb,
	nullptr, // debugStrOut;
	nullptr, // onTopologyChange;
	nullptr, // onNotifiedTopologyChange;
	&StpCallback_AllocAndZeroMemory,
	&StpCallback_FreeMemory,
};


void* PhysicalBridge::StpCallback_AllocAndZeroMemory(unsigned int size)
{
	void* p = malloc(size);
	memset (p, 0, size);
	return p;
}

void PhysicalBridge::StpCallback_FreeMemory(void* p)
{
	free(p);
}

void PhysicalBridge::StpCallback_EnableLearning(STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, bool enable)
{
	auto pb = static_cast<PhysicalBridge*>(STP_GetApplicationContext(bridge));
	BridgeInvalidateEvent::InvokeHandlers (pb->_em, pb);
}

void PhysicalBridge::StpCallback_EnableForwarding(STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, bool enable)
{
	auto pb = static_cast<PhysicalBridge*>(STP_GetApplicationContext(bridge));
	BridgeInvalidateEvent::InvokeHandlers(pb->_em, pb);
}

void PhysicalBridge::StpCallback_FlushFdb (STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, enum STP_FLUSH_FDB_TYPE flushType)
{
	auto pb = static_cast<PhysicalBridge*>(STP_GetApplicationContext(bridge));
}
#pragma endregion

#pragma region PhysicalBridge::IUnknown
HRESULT STDMETHODCALLTYPE PhysicalBridge::QueryInterface(REFIID riid, void** ppvObject)
{
	throw exception ("Not implemented.");
}

ULONG STDMETHODCALLTYPE PhysicalBridge::AddRef()
{
	return InterlockedIncrement(&_refCount);
}

ULONG STDMETHODCALLTYPE PhysicalBridge::Release()
{
	ULONG newRefCount = InterlockedDecrement(&_refCount);
	if (newRefCount == 0)
		delete this;
	return newRefCount;
}
#pragma endregion
