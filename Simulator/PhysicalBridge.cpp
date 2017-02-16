#include "pch.h"
#include "PhysicalBridge.h"
#include "Win32Defs.h"

using namespace std;
using namespace D2D1;

void PhysicalPort::Render(ID2D1DeviceContext* dc) const
{
	ComPtr<ID2D1SolidColorBrush> brush;
	dc->CreateSolidColorBrush(ColorF(ColorF::DeepPink), &brush);

	D2D1_RECT_F rect;
	switch (_side)
	{
		case Side::Bottom:
			rect.left = _bridge->GetLeft() + _offset - PortLongSize / 2;
			rect.top = _bridge->GetBottom() - PortShortSize;
			rect.right = _bridge->GetLeft() + _offset + PortLongSize / 2;
			rect.bottom = _bridge->GetBottom();
			dc->DrawRoundedRectangle (RoundedRect(rect, 3, 3), brush, 2.0f);
			break;

		default:
			throw NotImplementedException();
	}
}

PhysicalBridge::PhysicalBridge (size_t portCount, const std::array<uint8_t, 6>& macAddress)
	: _guiThreadId(this_thread::get_id())
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

	_stpBridge = STP_CreateBridge ((unsigned int) portCount, 1, &StpCallbacks, STP_VERSION_RSTP, &macAddress[0], 128);
	STP_SetApplicationContext (_stpBridge, this);
}

PhysicalBridge::~PhysicalBridge()
{
	assert (this_thread::get_id() == _guiThreadId);
	STP_DestroyBridge (_stpBridge);
}

void PhysicalBridge::Render(ID2D1DeviceContext* dc, unsigned int treeIndex, IDWriteFactory* dWriteFactory)
{
	assert (this_thread::get_id() == _guiThreadId);

	ComPtr<ID2D1SolidColorBrush> brush;
	dc->CreateSolidColorBrush (ColorF(ColorF::Red), &brush);
	dc->DrawRectangle ({ _x, _y, _x + _width, _y + _height }, brush, BridgeOutlineWidth);

	for (auto& port : _ports)
		port->Render(dc);

	lock_guard<mutex> lock(_stpBridgeMutex);

	unsigned char address[6];
	STP_GetBridgeAddress (_stpBridge, address);
	unsigned short prio = STP_GetBridgePriority(_stpBridge, treeIndex);
	wchar_t str[64];
	int strlen = swprintf_s (str, L"%04x.%02x%02x%02x%02x%02x%02x (STP %s)", prio, address[0], address[1], address[2], address[3], address[4], address[5],
		STP_IsBridgeStarted(_stpBridge) ? L"running" : L"stopped - right-click to start");

	ComPtr<IDWriteTextFormat> textFormat;
	auto hr = dWriteFactory->CreateTextFormat (L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12, L"en-US", &textFormat); ThrowIfFailed(hr);
	ComPtr<IDWriteTextLayout> textLayout;
	hr = dWriteFactory->CreateTextLayout (str, strlen, textFormat, 10000, 10000, &textLayout); ThrowIfFailed(hr);
	dc->DrawTextLayout ({ _x + BridgeOutlineWidth / 2 + 3, _y + BridgeOutlineWidth / 2 + 3}, textLayout, brush);
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

void PhysicalBridge::StartStpBridge (uint32_t timestamp)
{
	assert (this_thread::get_id() == _guiThreadId);
	lock_guard<mutex> lock (_stpBridgeMutex);
	if (STP_IsBridgeStarted (_stpBridge))
		throw InvalidOperationException (L"The bridge is already started.");
	
	STP_StartBridge (_stpBridge, timestamp);
	BridgeStartedEvent::InvokeHandlers (_em, this);

	BridgeInvalidateEvent::InvokeHandlers(_em, this);
}

void PhysicalBridge::StopStpBridge (uint32_t timestamp)
{
	assert (this_thread::get_id() == _guiThreadId);
	lock_guard<mutex> lock (_stpBridgeMutex);
	if (!STP_IsBridgeStarted(_stpBridge))
		throw InvalidOperationException (L"The bridge is already stopped.");

	BridgeStoppingEvent::InvokeHandlers(_em, this);
	STP_StopBridge(_stpBridge, timestamp);

	BridgeInvalidateEvent::InvokeHandlers(_em, this);
}

bool PhysicalBridge::IsStpBridgeStarted()
{
	return STP_IsBridgeStarted(_stpBridge);
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
	throw NotImplementedException();
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
